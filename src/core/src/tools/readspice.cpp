// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// readspice: native port of the Tcl procedure (see readspice.hpp).
// (Magic ref: tcltk/readspice.tcl:16-254)

#include "hefesto/tools/readspice.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tools/diag_codes.hpp"
#include "ui/command_util.hpp"

namespace hefesto::tools {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using ui::detail::run_forwarded;
using ui::detail::value_words;

// readspice.tcl:28-38.
constexpr std::array<std::string_view, 6> kCandidateExtensions = {".spi", ".spc", ".spice", ".sp", ".ckt", ".cdl"};

std::optional<std::string> read_file(platform::FileSystem& files, const platform::Path& cwd, const std::string& name) {
  platform::Path path{name};
  if (path.is_relative() && !cwd.empty()) {
    path = cwd / path;
  }
  auto text = files.read(path);
  if (!text.ok()) {
    return std::nullopt;
  }
  return std::move(text).value();
}

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

void note(CommandContext& ctx, std::string text) {
  ctx.emit(diag::make(codes::kReadSpiceNote, std::move(text)));
}

// The ports of a cell as (name, index) pairs, sorted: compared before and
// after the renumbering for "port order was modified" (Q-SCRIPT-32 fix).
using PortOrder = std::vector<std::pair<std::string, std::uint32_t>>;

PortOrder port_order(const editor::EditorState& editor, const std::string& cell) {
  PortOrder out;
  const db::CellDef* def = editor.cells.find(cell);
  if (def == nullptr) {
    return out;
  }
  for (const db::Label& label : def->labels()) {
    if (label.is_port()) {
      out.emplace_back(label.text, label.port_index);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::string replace_chars(std::string text, char a, char a_to, char b, char b_to) {
  for (char& c : text) {
    if (c == a) {
      c = a_to;
    } else if (c == b) {
      c = b_to;
    }
  }
  return text;
}

// readspice.tcl:166 `string map {\[ < \] >]} $pin` (Q-SCRIPT-47): Magic's
// map turns `]` into `>]`, so `D[0]` becomes `D<0>]`; `magic` false is the
// intended `[`/`]` to `<`/`>`.
std::string square_to_angle(const std::string& pin, bool magic) {
  if (!magic) {
    return replace_chars(pin, '[', '<', ']', '>');
  }
  std::string out;
  for (const char c : pin) {
    if (c == '[') {
      out += '<';
    } else if (c == ']') {
      out += ">]";
    } else {
      out += c;
    }
  }
  return out;
}

std::optional<std::int64_t> as_int(const cmd::Value& value) {
  if (const auto* i = std::get_if<std::int64_t>(&value.data())) {
    return *i;
  }
  return ui::detail::parse_int(value.to_string());
}

// One readspice run: the state of readspice.tcl's loop over the logical lines.
class Annotator {
 public:
  Annotator(CommandContext& ctx, editor::EditorState& editor, bool magic_bus_brackets)
      : ctx_(ctx), editor_(editor), magic_bus_brackets_(magic_bus_brackets) {}

  void line(const SpiceLine& line) {
    if (line.keyword == ".subckt") {
      subckt(line.tokens);
    } else if (line.keyword == ".pininfo") {
      pininfo(line.tokens);
    } else if (line.keyword == ".ends") {
      cell_.clear();
      loaded_ = false;
    }
  }

 private:
  // `[cmd ...]` of a nested command: its value as a string.
  std::string run(const std::vector<std::string>& words) { return run_forwarded(ctx_, words).value.to_string(); }

  // `port $name index -quiet`: the index, or nullopt for Tcl's "".
  std::optional<std::int64_t> port_index(const std::string& name) {
    const CommandResult r = run_forwarded(ctx_, {"port", name, "index", "-quiet"});
    return r.value.is_none() || r.value.to_string().empty() ? std::nullopt : as_int(r.value);
  }

  std::string port_name(std::int64_t index) { return run({"port", std::to_string(index), "name", "-quiet"}); }

  std::int64_t port_last() { return as_int(run_forwarded(ctx_, {"port", "last"}).value).value_or(-1); }

  void subckt(const std::vector<std::string>& tokens) {
    cell_ = tokens.size() > 1 ? tokens[1] : std::string{};
    const std::string exists = run({"cellname", "list", "exists", cell_});
    loaded_ = !exists.empty() && exists != "0";
    pins_.clear();
    if (!loaded_) {
      note(ctx_, "Cell " + cell_ + " in netlist has not been loaded.");
      return;
    }
    (void)run({"load", cell_});
    (void)run({"box", "values", "0", "0", "0", "0"});
    note(ctx_, "Annotating cell " + cell_);
    const PortOrder before = port_order(editor_, cell_);
    move_ports_out_of_the_way(static_cast<std::int64_t>(tokens.size()) - 1);
    collect_labels();
    bool created = false;
    std::int64_t n = 1;
    for (std::size_t i = 2; i < tokens.size(); ++i) {
      if (tokens[i].find('=') != std::string::npos) {
        break;  // parameters follow
      }
      if (!tokens[i].empty()) {
        created = assign_pin(tokens[i], n) || created;
      }
    }
    if (created || port_order(editor_, cell_) != before) {
      note(ctx_, "Cell " + cell_ + " port order was modified.");
    }
  }

  // Step 2 (readspice.tcl:115-132): every named port moves to an index at or
  // above max(port last, npins), in `port first` / `port P next` order.
  void move_ports_out_of_the_way(std::int64_t npins) {
    const std::int64_t highport = port_last();
    std::int64_t outport = std::max(highport, npins);
    std::optional<std::int64_t> p = as_int(run_forwarded(ctx_, {"port", "first"}).value);
    while (!p || (*p != -1 && *p <= highport)) {
      if (!p) {
        ctx_.emit(diag::make(codes::kReadSpiceNumbering, "Error:  " + cell_ + " port numbering failed."));
        break;
      }
      const std::optional<std::int64_t> next = as_int(run_forwarded(ctx_, {"port", std::to_string(*p), "next"}).value);
      if (!port_name(*p).empty()) {
        (void)run({"port", std::to_string(*p), "index", std::to_string(outport)});
        ++outport;
      }
      p = next;
    }
  }

  // Step 3 (readspice.tcl:135-140): the texts of every label of the cell.
  void collect_labels() {
    (void)run({"select", "top", "cell"});
    (void)run({"unexpand"});
    (void)run({"select", "area", "labels"});
    (void)run({"expand"});
    const CommandResult what = run_forwarded(ctx_, {"what", "-list"});
    labels_.clear();
    const auto* parts = std::get_if<cmd::Value::List>(&what.value.data());
    if (parts != nullptr && parts->size() > 1) {
      for (const cmd::Value& record : value_list((*parts)[1])) {
        const std::vector<std::string> words = value_words(record);
        labels_.push_back(words.empty() ? std::string{} : words.front());
      }
    }
    (void)run({"select", "clear"});
  }

  static cmd::Value::List value_list(const cmd::Value& value) {
    if (const auto* list = std::get_if<cmd::Value::List>(&value.data())) {
      return *list;
    }
    cmd::Value::List out;
    for (std::string& word : value_words(value)) {
      out.emplace_back(std::move(word));
    }
    return out;
  }

  // Step 4 lookups (readspice.tcl:161-185): the port named `pin`, with
  // `[]`/`<>` mapped, or a case-insensitive match. Sets `testpin`.
  std::optional<std::int64_t> find_port(const std::string& pin, std::string& testpin) {
    for (const std::string& candidate :
         {pin, square_to_angle(pin, magic_bus_brackets_), replace_chars(pin, '<', '[', '>', ']')}) {
      testpin = candidate;
      if (const auto index = port_index(testpin)) {
        return index;
      }
    }
    const std::string lower = ascii_lower(pin);
    const std::int64_t highport = port_last();
    for (std::int64_t p = 0; p <= highport; ++p) {
      testpin = port_name(p);
      if (ascii_lower(testpin) == lower) {
        return port_index(testpin);
      }
    }
    return std::nullopt;
  }

  // readspice.tcl:190-200: a bare label matching `pin` becomes port `n`.
  bool label_to_port(const std::string& pin, std::int64_t n, std::string& testpin) {
    const std::string lower = ascii_lower(pin);
    for (const std::string& text : labels_) {
      if (ascii_lower(text) == lower) {
        testpin = text;
        (void)run({"goto", testpin});
        (void)run({"port", testpin, "make", std::to_string(n)});
        return true;
      }
    }
    return false;
  }

  // One pin of the subcircuit line; true when a port was created.
  bool assign_pin(const std::string& pin, std::int64_t& n) {
    std::string testpin;
    std::optional<std::int64_t> index = find_port(pin, testpin);
    bool created = false;
    if (!index && label_to_port(pin, n, testpin)) {
      index = -1;
      created = true;
    }
    if (index) {
      (void)run({"port", testpin, "index", std::to_string(n)});
      ++n;
      pins_[pin] = testpin;
      return created;
    }
    if (!run({"goto", pin}).empty()) {
      (void)run({"port", pin, "make", std::to_string(n)});
      ++n;
      created = true;
    }
    pins_[pin] = pin;
    return created;
  }

  // "Port direction from PININFO" (readspice.tcl:227-247; Q-SCRIPT-19:
  // `switch` semantics, literal and case-sensitive).
  void pininfo(const std::vector<std::string>& tokens) {
    if (cell_.empty() || !loaded_) {
      return;
    }
    for (std::size_t i = 1; i < tokens.size(); ++i) {
      const std::vector<std::string> pair = tcl_split(tokens[i], ":");
      const std::string name = pair.empty() ? std::string{} : pair[0];
      const std::string dir = pair.size() > 1 ? pair[1] : std::string{};
      const auto found = pins_.find(name);
      if (found == pins_.end()) {
        if (!name.empty()) {
          ctx_.emit(diag::make(codes::kReadSpicePinInfo, ".PININFO error:  Pin " + name + " not found."));
        }
        continue;
      }
      set_direction(found->second, dir);
    }
  }

  void set_direction(const std::string& pin, const std::string& dir) {
    if (run({"port", pin, "class"}) != "default") {
      return;
    }
    static const std::map<std::string, std::string, std::less<>> kClasses = {
        {"B", "inout"}, {"I", "input"}, {"O", "output"}};
    if (const auto it = kClasses.find(dir); it != kClasses.end()) {
      (void)run({"port", pin, "class", it->second});
    }
  }

  CommandContext& ctx_;
  editor::EditorState& editor_;
  bool magic_bus_brackets_ = true;
  std::string cell_;
  bool loaded_ = false;
  std::map<std::string, std::string> pins_;  // netlist pin -> matched name (pindict)
  std::vector<std::string> labels_;          // label texts of the cell (`all`)
};

}  // namespace

std::string tcl_file_rootname(std::string_view name) {
  const std::size_t slash = name.rfind('/');
  const std::size_t tail = slash == std::string_view::npos ? 0 : slash + 1;
  const std::size_t dot = name.rfind('.');
  if (dot == std::string_view::npos || dot < tail) {
    return std::string{name};
  }
  return std::string{name.substr(0, dot)};
}

std::optional<NetlistFile> resolve_netlist(platform::FileSystem& files, const platform::Path& cwd,
                                           std::string_view name) {
  const std::string given{name};
  if (auto text = read_file(files, cwd, given)) {
    return NetlistFile{given, std::move(*text), ends_with(given, ".cdl")};
  }
  const std::string root = tcl_file_rootname(name);
  for (const std::string_view ext : kCandidateExtensions) {
    const std::string candidate = root + std::string{ext};
    if (auto text = read_file(files, cwd, candidate)) {
      return NetlistFile{candidate, std::move(*text), ext == ".cdl" || ends_with(given, ".cdl")};
    }
  }
  return std::nullopt;
}

CommandResult readspice(CommandContext& ctx, std::string_view netlist, const ReadSpiceOptions& options) {
  auto editor = cmd::require_editor(ctx);
  if (!editor.ok()) {
    return CommandResult::failure(editor.error());
  }
  const platform::Path cwd = editor.value()->cell_session.file_host().current_directory();
  const std::optional<NetlistFile> file = resolve_netlist(ctx.services().files, cwd, netlist);
  if (!file) {
    ctx.emit(diag::make(codes::kReadSpiceNoFile, "Error:  Can't read netlist file " + std::string{netlist}));
    return CommandResult::success(cmd::Value{std::int64_t{1}});
  }
  note(ctx, "Annotating port orders from " + file->name);
  Annotator annotator{ctx, *editor.value(), options.magic_bus_brackets};
  SpiceLineReader reader{file->text, options.continuation, file->cdl};
  (void)run_forwarded(ctx, {"suspendall"});
  while (const std::optional<std::string> line = reader.next()) {
    annotator.line(tokenize_spice_line(*line));
  }
  (void)run_forwarded(ctx, {"resumeall"});
  return CommandResult::success();
}

}  // namespace hefesto::tools
