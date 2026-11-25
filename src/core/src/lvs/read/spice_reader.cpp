// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE/CDL reader: file loop, dot cards, subcircuit definitions,
// globals, parameters, models and includes (lvs-netlist-input "SPICE
// lexical structure", "SPICE subcircuit definitions", "Globals, includes
// and parameters", "Library and black-box reads", "Input limits").
// Element cards are in spice_cards.cpp.
//
// Includes recurse once per nested file and stop at the include-depth limit
// (Q-LVS-36), so the recursion is bounded by the limit (at most 128 files).
//
// Owner: LVSREAD.

#include "lvs/read/spice_reader.hpp"

#include <filesystem>
#include <utility>

#include "hefesto/diag/sink.hpp"
#include "hefesto/lvs/net/codes.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/not_implemented.hpp"
#include "hefesto/lvs/read/codes.hpp"
#include "hefesto/platform/file_system.hpp"
#include "lvs/read/spice_lexer.hpp"

namespace hefesto::lvs::spice {

namespace codes = read_codes;

namespace {

constexpr std::uint32_t kMaxReportedWarnings = 100;

char upper(char c) noexcept {
  return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

bool is_high(char c) noexcept {
  return static_cast<unsigned char>(c) >= 0x80;
}

/// Ports named 1, 2, ... (or one proxy port): a placeholder made by a call.
bool numbered_ports(const Cell& cell) {
  if (cell.ports.size() == 1 && cell.ports.front().proxy) {
    return true;
  }
  for (std::size_t i = 0; i < cell.ports.size(); ++i) {
    if (cell.ports[i].name != std::to_string(i + 1)) {
      return false;
    }
  }
  return true;
}

std::optional<DeviceClass> model_class(std::string_view type) {
  const auto paren = type.find('(');
  const auto word = type.substr(0, paren);
  static constexpr std::pair<std::string_view, DeviceClass> kTypes[] = {
      {"NMOS", DeviceClass::nmos},   {"PMOS", DeviceClass::pmos},     {"PNP", DeviceClass::pnp},
      {"NPN", DeviceClass::npn},     {"D", DeviceClass::diode},       {"R", DeviceClass::resistor},
      {"C", DeviceClass::capacitor}, {"L", DeviceClass::inductor},    {"V", DeviceClass::vsource},
      {"I", DeviceClass::isource},
  };
  for (const auto& [name, device_class] : kTypes) {
    if (keyword_equal(word, name)) {
      return device_class;
    }
  }
  return std::nullopt;
}

std::string strip_include_quotes(std::string_view raw) {
  const auto is_quote = [](char c) { return c == '\'' || c == '"' || c == '`'; };
  while (!raw.empty() && is_quote(raw.front())) {
    raw.remove_prefix(1);
  }
  while (!raw.empty() && is_quote(raw.back())) {
    raw.remove_suffix(1);
  }
  return std::string{raw};
}

}  // namespace

bool keyword_equal(std::string_view token, std::string_view keyword) noexcept {
  if (token.size() != keyword.size()) {
    return false;
  }
  for (std::size_t i = 0; i < token.size(); ++i) {
    if (upper(token[i]) != upper(keyword[i])) {
      return false;
    }
  }
  return true;
}

SpiceReader::SpiceReader(NetlistDb& db, const ReadOptions& options, FileNumber file, std::string top_name)
    : db_(db),
      options_(options),
      file_(file),
      top_name_(std::move(top_name)),
      case_insensitive_(!db.case_sensitive_read),
      devices_(db.instance_count()),
      first_new_cell_(static_cast<CellId>(db.cells.size())) {}

void SpiceReader::warn(FileState& file, std::string_view code, std::string text) {
  ++file.warnings;
  if (options_.sink == nullptr || file.warnings > kMaxReportedWarnings + 1) {
    return;
  }
  if (file.warnings == kMaxReportedWarnings + 1) {
    options_.sink->emit(error(file, codes::kTooManyWarnings, "Too many warnings. . . will not report any more."));
    return;
  }
  options_.sink->emit(error(file, code, std::move(text)));
}

void SpiceReader::note(const FileState& file, std::string_view code, std::string text) {
  if (options_.sink != nullptr) {
    options_.sink->emit(error(file, code, std::move(text)));
  }
}

diag::Diagnostic SpiceReader::error(const FileState& file, std::string_view code, std::string text) const {
  return diag::make(code, std::move(text), diag::SourceLocation{file.name, file.line});
}

diag::Status SpiceReader::check_tokens(const FileState& file, const Tokens& tokens) const {
  for (const auto& token : tokens) {
    std::size_t high = 0;
    for (const char c : token) {
      high += is_high(c) ? std::size_t{1} : std::size_t{0};
    }
    if (high > 0 && token.size() < 4 * high) {
      return error(file, codes::kBinary, "Input file \"" + file.name + "\" appears to be binary. . . bailing out");
    }
    if (token.find('=') == std::string::npos && token.size() > options_.limits.name_length) {
      return error(file, codes::kNameLength,
                   "name of " + std::to_string(token.size()) + " bytes exceeds the name-length limit of " +
                       std::to_string(options_.limits.name_length) + " bytes in " + file.name + " line " +
                       std::to_string(file.line));
    }
  }
  return diag::Done{};
}

CellId SpiceReader::new_cell(std::string name, DeviceClass device_class) {
  Cell cell{std::move(name), file_};
  cell.device_class = device_class;
  cell.flags.case_insensitive = case_insensitive_;
  return db_.add_cell(std::move(cell));
}

CellId SpiceReader::current_cell() {
  if (!open_.empty()) {
    return open_.back();
  }
  if (!file_cell_) {
    file_cell_ = new_cell(top_name_, DeviceClass::subcircuit);
    cell(*file_cell_).flags.top = true;
  }
  return *file_cell_;
}

std::optional<CellId> SpiceReader::defining() const noexcept {
  if (open_.empty()) {
    return std::nullopt;
  }
  return open_.back();
}

diag::Status SpiceReader::add_device(const FileState& file) {
  if (++devices_ > options_.limits.devices) {
    return error(file, net_codes::kDeviceLimit,
                 "the devices-per-run limit of " + std::to_string(options_.limits.devices) + " is exceeded in " +
                     file.name + " line " + std::to_string(file.line));
  }
  return diag::Done{};
}

diag::Outcome<CellId> SpiceReader::read_top(std::string_view bytes) {
  FileState file{top_name_};
  if (!case_insensitive_) {
    warn(file, codes::kCaseSensitive,
         "Warning:  A case-sensitive file has been read and so the SPICE netlist must be treated case-sensitive to "
         "match.");
  }
  if (auto status = read_text(file, bytes); !status) {
    return status.error();
  }
  if (library_skip_ > 0) {
    warn(file, codes::kMissingEnds, "Missing .ENDS statement on subcircuit.");
  }
  while (!open_.empty()) {
    warn(file, codes::kMissingEnds, "Missing .ENDS statement on subcircuit.");
    close_definition(open_.back());
  }
  finish_instances();
  if (file.warnings > 0) {
    note(file, codes::kSummary,
         "File " + file.name + " read with " + std::to_string(file.warnings) +
             (file.warnings == 1 ? " warning." : " warnings."));
  }
  return top_cell();
}

diag::Status SpiceReader::read_text(FileState& file, std::string_view bytes) {
  Lexer lexer{bytes, options_.limits.logical_line};
  LogicalLine line;
  for (;;) {
    const auto next = lexer.next(line);
    file.line = line.line;
    if (next == Lexer::Next::end) {
      return diag::Done{};
    }
    if (next == Lexer::Next::too_long) {
      return error(file, codes::kLineLength,
                   "logical line exceeds the line limit of " + std::to_string(options_.limits.logical_line) +
                       " bytes in " + file.name + " line " + std::to_string(file.line));
    }
    const auto tokens = tokenize(line.text);
    if (tokens.empty()) {
      continue;
    }
    if (auto status = check_tokens(file, tokens); !status) {
      return status;
    }
    if (auto status = process(file, tokens); !status) {
      return status;
    }
  }
}

diag::Status SpiceReader::process(FileState& file, const Tokens& tokens) {
  const std::string& first = tokens.front();
  if (file.control) {
    file.control = !keyword_equal(first, ".endc");
    return diag::Done{};
  }
  if (library_skip_ > 0) {
    return skip_library_body(file, tokens);
  }
  if (first.starts_with('*')) {
    if (keyword_equal(first, "*.global")) {
      global(tokens);
    }
    return diag::Done{};
  }
  if (first.starts_with('.')) {
    return dot_card(file, tokens);
  }
  switch (upper(first.front())) {
    case 'X': return options_.library ? diag::Status{diag::Done{}} : call(file, tokens);
    case 'M':
    case 'Q':
    case 'D':
    case 'C':
    case 'R':
    case 'L':
    case 'T':
    case 'V':
    case 'I':
    case 'E': return options_.library ? diag::Status{diag::Done{}} : primitive(file, tokens);
    default: warn(file, codes::kIgnoredCard, "Ignoring line starting with token: " + first); return diag::Done{};
  }
}

diag::Status SpiceReader::dot_card(FileState& file, const Tokens& tokens) {
  const std::string& card = tokens.front();
  if (keyword_equal(card, ".subckt")) {
    return subckt(file, tokens);
  }
  if (keyword_equal(card, ".include")) {
    return include(file, tokens);
  }
  if (keyword_equal(card, ".ends")) {
    ends(file);
  } else if (keyword_equal(card, ".global")) {
    global(tokens);
  } else if (keyword_equal(card, ".param")) {
    param(tokens);
  } else if (keyword_equal(card, ".model")) {
    model(tokens);
  } else if (keyword_equal(card, ".control")) {
    file.control = true;
  } else if (!keyword_equal(card, ".end")) {
    warn(file, codes::kIgnoredCard, "Ignoring line starting with token: " + card);
  }
  return diag::Done{};
}

diag::Status SpiceReader::skip_library_body(FileState& /*file*/, const Tokens& tokens) {
  if (keyword_equal(tokens.front(), ".subckt")) {
    ++library_skip_;
  } else if (keyword_equal(tokens.front(), ".ends")) {
    --library_skip_;
  }
  return diag::Done{};
}

std::optional<CellId> SpiceReader::reuse_placeholder(FileState& file, std::string_view name, diag::Status& status) {
  const auto existing = db_.find_cell(name, file_);
  if (!existing || !cell(*existing).flags.placeholder) {
    return std::nullopt;
  }
  Cell& placeholder = cell(*existing);
  if (!numbered_ports(placeholder)) {
    // A Verilog placeholder: its pins are reordered by the pin matching of
    // lvs-hierarchy once the Verilog reader exists (tasks.md 6.3).
    (void)file;
    status = not_implemented("lvs::read_netlist (Verilog placeholder replaced by SPICE definition)");
    return std::nullopt;
  }
  placeholder.ports.clear();
  placeholder.clear_nets();
  placeholder.instances.clear();
  placeholder.properties.clear();
  placeholder.flags.placeholder = false;
  placeholder.device_class = options_.library ? DeviceClass::module : DeviceClass::subcircuit;
  return existing;
}

void SpiceReader::rename_duplicate(FileState& file, CellId existing) {
  const std::string base = cell(existing).name;
  std::string renamed;
  for (std::size_t k = 0;; ++k) {
    renamed = base + "[[" + std::to_string(k) + "]]";
    if (!db_.find_cell_exact(renamed, file_)) {
      break;
    }
  }
  warn(file, codes::kDuplicateCell, "Duplicate cell " + base + " in file");
  warn(file, codes::kDuplicateCell, "Renaming original cell to " + renamed);
  db_.rename_cell(existing, renamed);
  cell(existing).flags.duplicate = true;
}

namespace {

/// The ports and the `key=value` properties of interest of a .SUBCKT line;
/// a definition without ports gets one proxy port.
void read_header(Cell& definition, const Tokens& tokens) {
  for (std::size_t i = 2; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];
    const auto eq = token.find('=');
    if (keyword_equal(token, "params:")) {
      continue;
    }
    if (eq != std::string::npos && eq > 0) {
      definition.properties.push_back(PropertyDef{token.substr(0, eq), PropertyType::string, 0.0, token.substr(eq + 1)});
      continue;
    }
    const NetId net = definition.find_net(token) == kNoNet ? definition.add_net(token) : kNoNet;
    definition.ports.push_back(Port{token, net, false});
  }
  if (definition.ports.empty()) {
    definition.ports.push_back(Port{"", kNoNet, true});
  }
}

}  // namespace

diag::Status SpiceReader::subckt(FileState& file, const Tokens& tokens) {
  if (tokens.size() < 2) {
    warn(file, codes::kBadlyFormed, "Badly formed line in input.");
    return diag::Done{};
  }
  if (!open_.empty()) {
    warn(file, codes::kMissingEnds, "Missing .ENDS statement on subcircuit.");
  }
  const std::string& name = tokens[1];
  diag::Status status = diag::Done{};
  auto id = reuse_placeholder(file, name, status);
  if (!status) {
    return status;
  }
  if (!id) {
    if (const auto existing = db_.find_cell(name, file_)) {
      rename_duplicate(file, *existing);
    }
    id = new_cell(name, options_.library ? DeviceClass::module : DeviceClass::subcircuit);
  }
  Cell& definition = cell(*id);
  read_header(definition, tokens);
  const std::optional<CellId> enclosing = open_.empty() ? file_cell_ : std::optional{open_.back()};
  if (enclosing) {
    for (const auto& net : cell(*enclosing).nets) {
      if (net.global && definition.find_net(net.name) == kNoNet) {
        definition.add_net(net.name, true);
      }
    }
  }
  if (options_.library) {
    finish_ports(definition);
    library_skip_ = 1;
    last_top_definition_ = *id;
    return diag::Done{};
  }
  open_.push_back(*id);
  return diag::Done{};
}

void SpiceReader::ends(FileState& file) {
  if (open_.empty()) {
    warn(file, codes::kStrayEnds, ".ENDS occurred outside of a subcircuit!");
    return;
  }
  close_definition(open_.back());
}

void SpiceReader::close_definition(CellId id) {
  open_.pop_back();
  Cell& definition = cell(id);
  finish_ports(definition);
  if (options_.auto_blackbox && definition.instances.empty()) {
    definition.device_class = DeviceClass::module;
  }
  if (open_.empty()) {
    last_top_definition_ = id;
  }
}

void SpiceReader::finish_ports(Cell& definition) {
  for (auto& port : definition.ports) {
    if (port.proxy) {
      port.net = definition.add_net("");
      continue;
    }
    if (port.net != kNoNet) {
      continue;
    }
    for (const auto& other : definition.ports) {
      if (other.net != kNoNet && names_equal(other.name, port.name, definition.flags.case_insensitive)) {
        port.net = other.net;
        break;
      }
    }
  }
  std::vector<bool> used(definition.nets.size(), false);
  const auto mark = [&used](NetId net) {
    if (net != kNoNet && net <= used.size()) {
      used[net - 1] = true;
    }
  };
  for (const auto& port : definition.ports) {
    mark(port.net);
  }
  for (const auto& instance : definition.instances) {
    for (const NetId net : instance.connections) {
      mark(net);
    }
  }
  bool drop = false;
  for (std::size_t i = 0; i < used.size(); ++i) {
    used[i] = used[i] || !definition.nets[i].global;
    drop = drop || !used[i];
  }
  if (drop) {
    definition.compact_nets(used);
  }
}

void SpiceReader::finish_instances() {
  for (auto& owner : db_.cells) {
    if (owner.file != file_) {
      continue;
    }
    for (auto& instance : owner.instances) {
      instance.connections.resize(db_.cells.at(instance.cell).ports.size(), kNoNet);
    }
  }
}

CellId SpiceReader::top_cell() {
  if (file_cell_ && !cell(*file_cell_).instances.empty()) {
    return *file_cell_;
  }
  auto named = db_.find_cell_exact(top_name_, file_);
  if (!named) {
    named = new_cell(top_name_, DeviceClass::subcircuit);
    cell(*named).flags.top = true;
  }
  return last_top_definition_.value_or(*named);
}

void SpiceReader::global(const Tokens& tokens) {
  Cell& owner = cell(current_cell());
  for (std::size_t i = 1; i < tokens.size(); ++i) {
    const NetId net = owner.find_net(tokens[i]);
    if (net != kNoNet) {
      owner.net(net).global = true;
    } else {
      owner.add_net(tokens[i], true);
    }
  }
}

void SpiceReader::param(const Tokens& tokens) {
  auto& table = db_.parameters[file_];
  for (std::size_t i = 1; i < tokens.size(); ++i) {
    const auto eq = tokens[i].find('=');
    if (eq != std::string::npos && eq > 0) {
      table[name_key(std::string_view{tokens[i]}.substr(0, eq))] = tokens[i].substr(eq + 1);
    }
  }
}

void SpiceReader::model(const Tokens& tokens) {
  if (tokens.size() < 3) {
    return;
  }
  const auto id = db_.find_cell(tokens[1], file_);
  const auto device_class = model_class(tokens[2]);
  if (id && device_class) {
    cell(*id).device_class = *device_class;
  }
}

namespace {

/// The path an .INCLUDE names: quotes stripped, `~` expanded through the
/// host (Q-LVS-14), a relative path joined to the including file's folder.
diag::Outcome<std::string> include_path(const ReadOptions& options, const FileState& file, std::string_view raw) {
  std::string name = strip_include_quotes(raw);
  if (name.starts_with('~')) {
    const auto slash = name.find('/');
    const std::string user = name.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
    const auto home = options.home_directory ? options.home_directory(user) : std::nullopt;
    if (!home) {
      return diag::make(read_codes::kPathUnreadable,
                        "cannot expand '~" + user + "' in include path '" + name + "' on this platform",
                        diag::SourceLocation{file.name, file.line});
    }
    return *home + (slash == std::string::npos ? std::string{} : name.substr(slash));
  }
  const std::filesystem::path path{name};
  if (path.is_absolute()) {
    return name;
  }
  return (std::filesystem::path{file.name}.parent_path() / path).string();
}

}  // namespace

diag::Status SpiceReader::include(FileState& file, const Tokens& tokens) {
  if (tokens.size() < 2) {
    warn(file, codes::kIncludeUnreadable, "Missing file name on .include line.");
    return diag::Done{};
  }
  if (include_depth_ >= options_.limits.include_depth) {
    return error(file, codes::kIncludeDepth,
                 "include depth limit of " + std::to_string(options_.limits.include_depth) + " exceeded in " +
                     file.name + " line " + std::to_string(file.line));
  }
  if (++includes_ > kMaxIncludes) {
    return error(file, codes::kIncludeCount,
                 "more than " + std::to_string(kMaxIncludes) + " include reads in one netlist at " + file.name +
                     " line " + std::to_string(file.line));
  }
  auto path = include_path(options_, file, tokens[1]);
  if (!path) {
    return path.error();
  }
  if (options_.files == nullptr) {
    return error(file, codes::kPathUnreadable, "no file service to read include file '" + path.value() + "'");
  }
  auto bytes = options_.files->read(path.value());
  if (!bytes) {
    warn(file, codes::kIncludeUnreadable, "Cannot open include file '" + path.value() + "'.");
    return diag::Done{};
  }
  FileState inner{std::move(path).value()};
  ++include_depth_;
  auto status = read_text(inner, bytes.value());
  --include_depth_;
  if (status && inner.warnings > 0) {
    note(inner, codes::kSummary,
         "File " + inner.name + " read with " + std::to_string(inner.warnings) +
             (inner.warnings == 1 ? " warning." : " warnings."));
  }
  return status;
}

}  // namespace hefesto::lvs::spice
