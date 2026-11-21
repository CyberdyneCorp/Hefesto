// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Subcircuit definitions (capability schematic-netlisting "Subcircuit
// definitions" with the Q-SCHEM-14, -19 and -21 branch points, "Schematic
// resolution" with Q-SCHEM-27 and -28, "Top-level port order").
//
// The definitions are planned before the top level is written: the list of
// loaded symbols is walked once, which fixes each definition's name, so the
// instance lines written earlier already use the `<cell>_<k>` names of
// xschem-compat OFF and the `<v>` names of schematic variants.
//
// Owner: NETLIST.

#include <algorithm>
#include <array>
#include <map>
#include <set>

#include "format_scan.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "netlist_run.hpp"

namespace hefesto::schem::detail {

namespace {

/// The name without folder and extension (`libA/inv.sym` -> `inv`).
std::string cell_of(std::string_view reference) {
  const std::size_t slash = reference.rfind('/');
  if (slash != std::string_view::npos) {
    reference.remove_prefix(slash + 1);
  }
  const std::size_t dot = reference.rfind('.');
  return std::string{dot == std::string_view::npos ? reference : reference.substr(0, dot)};
}

std::string file_identity(const DefinitionEntry& entry) {
  const std::string file =
      entry.symbol->file.empty() ? entry.reference : entry.symbol->file.generic_string();
  return entry.variant.empty() ? file : file + "|" + entry.variant;
}

/// The pin `dir` a port type stands for.
std::string port_direction(std::string_view type) {
  if (type == "ipin") {
    return "in";
  }
  return type == "opin" ? "out" : "inout";
}

bool is_separator(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == ';';
}

/// The words of `text` delimited by separators.
std::vector<std::string_view> words(std::string_view text) {
  std::vector<std::string_view> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && is_separator(text[i])) {
      ++i;
    }
    const std::size_t start = i;
    while (i < text.size() && !is_separator(text[i])) {
      ++i;
    }
    if (i > start) {
      out.push_back(text.substr(start, i - start));
    }
  }
  return out;
}

/// One piece of a property string: separators, or an attribute split into
/// its key, the `=` with the separators around it, and its raw value.
struct Piece {
  bool attribute = false;
  std::string_view text;  // separators
  std::string_view key;
  std::string_view equals;
  std::string_view value;
};

/// Splits a property string by the grammar of schematic-file-format
/// "Property strings", keeping every character.
class PieceReader {
 public:
  explicit PieceReader(std::string_view text) : text_{text} {}

  std::vector<Piece> read() {
    std::vector<Piece> pieces;
    while (pos_ < text_.size()) {
      const std::size_t start = pos_;
      while (pos_ < text_.size() && is_separator(text_[pos_])) {
        ++pos_;
      }
      if (pos_ > start) {
        pieces.push_back(Piece{.text = text_.substr(start, pos_ - start)});
        continue;
      }
      pieces.push_back(attribute());
    }
    return pieces;
  }

 private:
  Piece attribute() {
    Piece piece{.attribute = true};
    const std::size_t key_start = pos_;
    scan_until([](char c) { return is_separator(c) || c == '='; });
    piece.key = text_.substr(key_start, pos_ - key_start);
    std::size_t eq = pos_;
    while (eq < text_.size() && is_separator(text_[eq])) {
      ++eq;
    }
    if (eq >= text_.size() || text_[eq] != '=') {
      return piece;
    }
    ++eq;
    while (eq < text_.size() && is_separator(text_[eq])) {
      ++eq;
    }
    piece.equals = text_.substr(pos_, eq - pos_);
    pos_ = eq;
    const std::size_t value_start = pos_;
    scan_until([](char c) { return is_separator(c); });
    piece.value = text_.substr(value_start, pos_ - value_start);
    return piece;
  }

  /// Advances to the first unquoted, unescaped character that `stop`
  /// accepts (or the end).
  template <typename Stop>
  void scan_until(Stop stop) {
    bool quoted = false;
    while (pos_ < text_.size()) {
      const char c = text_[pos_];
      if (c == '\\') {
        pos_ = std::min(pos_ + 2, text_.size());
        continue;
      }
      if (c == '"') {
        quoted = !quoted;
      } else if (!quoted && stop(c)) {
        return;
      }
      ++pos_;
    }
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

/// Q-SCHEM-21 OFF: an LF followed by optional blanks and `+` is kept, every
/// other LF becomes one space.
std::string join_template_lines(std::string text) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\n') {
      continue;
    }
    std::size_t next = i + 1;
    while (next < text.size() && (text[next] == ' ' || text[next] == '\t')) {
      ++next;
    }
    if (next >= text.size() || text[next] != '+') {
      text[i] = ' ';
    }
  }
  return text;
}

}  // namespace

std::string NetlistRun::cell_name(const DefinitionEntry& entry) const {
  return cell_of(entry.variant.empty() ? std::string_view{entry.reference} : std::string_view{entry.variant});
}

/// The instance's `schematic` value (its `@` tokens substituted from the
/// instance) when it differs from its symbol's: a variant (Q-SCHEM-28).
std::string NetlistRun::variant_value(const Placed& instance) const {
  const auto value = instance.instance.get("schematic", ValueForm::decoded);
  if (!value || value->empty()) {
    return {};
  }
  const std::array<const PropertySource*, 1> self{&instance.instance};
  std::string substituted = substitute_from_parents(*value, self);
  const std::string own = instance.symprops.get("schematic", ValueForm::decoded).value_or(std::string{});
  return substituted == own ? std::string{} : substituted;
}

std::optional<std::string> NetlistRun::symname_override(const Placed& instance) const {
  const std::string variant = variant_value(instance);
  const auto it = symnames_.find({instance.symbol, variant});
  if (it != symnames_.end()) {
    return it->second;
  }
  if (!variant.empty()) {
    return cell_of(variant);
  }
  return std::nullopt;
}

std::string NetlistRun::symbol_format(const Symbol& symbol) const {
  const std::string& props = symbol.property();
  if (options_.lvs_netlist) {
    if (auto format = get_token(props, "lvs_format")) {
      return *format;
    }
  }
  return get_token(props, "format").value_or(std::string{});
}

/// Appends the symbols first used by `level`, then its variants, to the list
/// of loaded symbols.
void NetlistRun::add_used(const Level& level) {
  const platform::Path folder = level.document->path.parent_path();
  for (const Placed& p : level.instances) {
    if (!p.symbol->placeholder && entry_keys_.insert({p.symbol, std::string{}}).second) {
      entries_.push_back(DefinitionEntry{p.symbol, p.record->symbol, {}, {}, folder});
    }
  }
  for (const Placed& p : level.instances) {
    std::string variant = variant_value(p);
    if (!p.symbol->placeholder && !variant.empty() && entry_keys_.insert({p.symbol, variant}).second) {
      entries_.push_back(DefinitionEntry{p.symbol, p.record->symbol, std::move(variant), p.record->props, folder});
    }
  }
}

/// The skip rules of "Subcircuit definitions" other than the cell name.
bool NetlistRun::skip_definition(const DefinitionEntry& entry) const {
  const std::string& props = entry.symbol->property();
  const auto value = [&](std::string_view key) { return get_token(props, key).value_or(std::string{}); };
  const auto ignored = [](const std::string& v) { return is_true(v) || v == "open" || v == "short"; };
  if (ignored(value("spice_ignore")) || (options_.lvs_ignore && ignored(value("lvs_ignore")))) {
    return true;
  }
  if (symbol_type(*entry.symbol) != "subcircuit" || value("default_schematic") == "ignore") {
    return true;
  }
  if (is_true(value("spice_primitive")) || (options_.lvs_netlist && is_true(value("lvs_primitive")))) {
    return true;
  }
  return symbol_format(*entry.symbol).empty();
}

/// The Q-SCHEM-14 branch point: a cell name already written from another
/// file is skipped (ON) or renamed `<cell>_<k>` (OFF); both modes warn.
std::optional<std::string> NetlistRun::definition_name(const DefinitionEntry& entry) {
  const std::string cell = cell_name(entry);
  const std::string identity = file_identity(entry);
  const auto same_cell = [&](const PlannedDefinition& d) { return d.name == cell; };
  const auto first = std::find_if(planned_.begin(), planned_.end(), same_cell);
  if (first == planned_.end()) {
    return cell;
  }
  if (file_identity(first->entry) == identity) {
    return std::nullopt;
  }
  warn(diag::make(net_codes::kSameCellName, "subcircuit " + cell + " of " + identity + " has the cell name of " +
                                                file_identity(first->entry) +
                                                (compat_ ? "; only the first is written" : "; renamed")));
  if (compat_) {
    return std::nullopt;
  }
  for (int k = 1;; ++k) {
    const std::string name = cell + "_" + std::to_string(k);
    if (std::none_of(planned_.begin(), planned_.end(), [&](const PlannedDefinition& d) { return d.name == name; })) {
      return name;
    }
  }
}

/// "Schematic resolution".
platform::Path NetlistRun::resolve_schematic(const DefinitionEntry& entry, const std::string& cell) {
  std::string value = entry.variant;
  if (value.empty()) {
    value = get_token(entry.symbol->property(), "schematic").value_or(std::string{});
  }
  if (!value.empty()) {
    const std::string symbol_cell = cell_of(entry.reference);
    for (std::size_t at = value.find("@symname"); at != std::string::npos; at = value.find("@symname", at)) {
      value.replace(at, 8, symbol_cell);
      at += symbol_cell.size();
    }
    if (value.starts_with("tcleval(")) {
      value = env_.tcl != nullptr ? env_.tcl->evaluate(value, TclContext{}) : std::string{kTclFailure};
      while (!value.empty() && value.back() == '\n') {
        value.pop_back();
      }
    }
    const ResolvedReference resolved = resolve_reference(value, entry.from_folder, env_.search_path, env_.fs);
    if (!resolved.file.empty() && (env_.fs.exists(resolved.file) || entry.variant.empty())) {
      return resolved.file;
    }
  }
  (void)cell;
  if (!entry.symbol->file.empty()) {
    platform::Path file = entry.symbol->file;
    return file.replace_extension(".sch");
  }
  return entry.from_folder / (cell_of(entry.reference) + ".sch");
}

const Document* NetlistRun::load_schematic(const platform::Path& file) {
  if (const auto it = documents_.find(file); it != documents_.end()) {
    return it->second.get();
  }
  std::unique_ptr<Document> document;
  if (env_.fs.exists(file)) {
    ReadOptions read;
    read.kind = FileKind::schematic;
    read.xschem_compat = compat_;
    read.warnings = &resolve_warnings_;
    read.source_name = file.generic_string();
    auto loaded = load_document(env_.fs, file, read);
    if (loaded) {
      document = std::make_unique<Document>(std::move(loaded).value());
    }
  }
  const Document* out = document.get();
  documents_.emplace(file, std::move(document));
  return out;
}

/// Walks the list of loaded symbols from the first, appending the symbols
/// first used inside each definition written.
void NetlistRun::plan_definitions() {
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const DefinitionEntry entry = entries_[i];
    if (skip_definition(entry)) {
      continue;
    }
    const auto name = definition_name(entry);
    if (!name) {
      continue;
    }
    symnames_[entry.key()] = *name;
    PlannedDefinition definition{entry, *name, {}, nullptr};
    const std::string& props = entry.symbol->property();
    if (get_token(props, "spice_sym_def").value_or(std::string{}).empty()) {
      definition.schematic = resolve_schematic(entry, cell_name(entry));
      definition.document = load_schematic(definition.schematic);
    }
    planned_.push_back(definition);
    if (definition.document != nullptr && !is_true(get_token(props, "spice_stop").value_or(std::string{}))) {
      add_used(place(*definition.document, Parents{}, false));
    }
  }
}

void NetlistRun::write_definitions() {
  for (const PlannedDefinition& definition : planned_) {
    write_definition(definition);
  }
}

void NetlistRun::write_definition(const PlannedDefinition& definition) {
  const DefinitionEntry& entry = definition.entry;
  const Symbol& symbol = *entry.symbol;
  const std::string reference = entry.variant.empty() ? entry.reference : cell_name(entry) + ".sym";
  std::string head = "\n* expanding   symbol:  " + reference + " # of pins=" + std::to_string(symbol.pins.size()) +
                     "\n** sym_path: " + path_text(symbol.file) + "\n";
  const PropertyString templ{get_token(symbol.property(), "template").value_or(std::string{})};
  if (const auto def = get_token(symbol.property(), "spice_sym_def"); def && !def->empty()) {
    std::string text;
    detail::scan_format(
        *def, false, [&](char c) { text += c; },
        [&](char /*sigil*/, const std::string& key, std::string_view source) {
          const auto value = key == "symname" ? std::optional<std::string>{definition.name}
                                              : templ.get(key, ValueForm::decoded);
          text += value ? *value : std::string{source};
        });
    emit(head + text + "\n");
    return;
  }
  const PropertyString variant{entry.variant_props};
  const Parents parents{entry.variant.empty() ? nullptr : &variant, &templ};
  head += "** sch_path: " + path_text(definition.schematic) + "\n";
  head += header_line(definition, parents);
  emit(std::move(head));
  if (definition.document == nullptr) {
    warn(diag::make(net_codes::kMissingSchematic,
                    "load_schematic(): unable to open file: " + definition.schematic.generic_string()));
  } else if (!is_true(get_token(symbol.property(), "spice_stop").value_or(std::string{}))) {
    Level level = place(*definition.document, parents, false);
    connect(level);
    check_pins(symbol, level);
    body(level);
  }
  if (definition.document != nullptr && !definition.document->globals.s.empty()) {
    std::string s = definition.document->globals.s;
    if (!s.ends_with('\n')) {
      s += '\n';
    }
    emit(std::string{kBeginUserCode} + "\n" + s + std::string{kEndUserCode} + "\n");
  }
  emit(std::string{options_.uppercase_subckt ? ".ENDS" : ".ends"} + "\n\n");
}

/// "pin mismatches between a symbol and its schematic": the bits of the
/// symbol's pin names and of the schematic's port `lab` values differ, or a
/// pin's `dir` is not the direction of its port (`ipin` in, `opin` out,
/// `iopin` inout).
void NetlistRun::check_pins(const Symbol& symbol, const Level& level) {
  std::map<std::string, std::string> pins;   // bit -> dir
  std::map<std::string, std::string> ports;  // bit -> dir
  for (const Pin& pin : symbol.pins) {
    for (std::string& bit : expand_bus(substitute_parents(pin.name, level.parents))) {
      pins.emplace(std::move(bit), pin.direction);
    }
  }
  for (const Placed& p : level.instances) {
    if (!p.ignored && is_port_type(p.type)) {
      for (std::string& bit : expand_bus(substitute_parents(attribute(p, "lab"), level.parents))) {
        ports.emplace(std::move(bit), port_direction(p.type));
      }
    }
  }
  const auto same = [](const auto& a, const auto& b) {
    return a.first == b.first && (a.second.empty() || a.second == b.second);
  };
  if (!std::equal(pins.begin(), pins.end(), ports.begin(), ports.end(), same)) {
    error(diag::make(net_codes::kPinMismatch, "pins of symbol " + symbol.file.generic_string() +
                                                  " and ports of schematic " +
                                                  level.document->path.generic_string() +
                                                  " differ in names or directions"));
  }
}

/// The `.subckt` header of "Subcircuit definitions", item 4.
std::string NetlistRun::header_line(const PlannedDefinition& definition, const Parents& parents) {
  const auto model = parents.templ->get("model", ValueForm::decoded);
  std::string line = options_.uppercase_subckt ? ".SUBCKT " : ".subckt ";
  line += model && !model->empty() ? *model : definition.name;
  line += ' ';
  bool reached_end = false;
  line += header_ports(definition, parents, reached_end);
  if (reached_end) {
    line += '\n';
  }
  line += header_template(definition);
  line += '\n';
  return line;
}

/// The port names of `pins` (pin order): each name substituted in the parent
/// context, `expr`-evaluated, bus-expanded, followed by a space.
std::string NetlistRun::port_names(const Symbol& symbol, const Parents& parents, std::span<const PinIndex> pins) {
  std::string out;
  for (const PinIndex k : pins) {
    const std::string name = substitute_parents(symbol.pins[k].name, parents);
    const BusExpansion bus = parse_bus(name);
    if (auto warning = bus_diagnostic(bus, name)) {
      warn(std::move(*warning));
    }
    out += replace_bus_chars(join_bits(bus.bits, ","), options_.bus_replacement_char) + " ";
  }
  return out;
}

namespace {

/// The pins `@pinlist` lists: in pin order, the first of each distinct name,
/// without `spice_ignore` true.
std::vector<PinIndex> listed_pins(const Symbol& symbol, std::span<const PinIndex> order) {
  std::vector<PinIndex> listed;
  std::set<std::string_view> seen;
  for (const PinIndex k : order) {
    const Pin& pin = symbol.pins[k];
    if (seen.insert(pin.name).second && !is_true(get_token(pin.box.props, "spice_ignore").value_or(std::string{}))) {
      listed.push_back(k);
    }
  }
  return listed;
}

/// The pin `@@<ref>` or `@#<ref>` names: by name, or by its position.
std::optional<PinIndex> find_pin(const Symbol& symbol, std::span<const PinIndex> order, std::string_view ref) {
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (symbol.pins[order[i]].name == ref || std::to_string(i) == ref) {
      return order[i];
    }
  }
  return std::nullopt;
}

}  // namespace

/// The text one format token adds to a header port list; sets `stopped` at
/// `@symname` or `@model`.
std::string NetlistRun::header_token(const HeaderScan& scan, char sigil, const std::string& key,
                                     std::string_view source, bool& stopped) {
  if (stopped || sigil != '@' || key == "spiceprefix" || key == "name") {
    return {};
  }
  if (key == "symname" || key == "model") {
    stopped = true;
    return {};
  }
  if (key == "pinlist") {
    return port_names(scan.symbol, scan.parents, scan.listed);
  }
  if (key.starts_with('@') || key.starts_with('#')) {
    const auto pin = find_pin(scan.symbol, scan.order, std::string_view{key}.substr(1));
    return pin ? port_names(scan.symbol, scan.parents, std::span<const PinIndex>{&*pin, 1}) : std::string{};
  }
  const std::size_t after = static_cast<std::size_t>(source.data() - scan.format.data()) + source.size();
  return after < scan.format.size() && is_separator(scan.format[after]) ? key + " " : std::string{};
}

/// The port list of a header from the symbol format: `@pinlist`, `@@pin`
/// and `@#n` write pin names, other `@token`s followed by a separator write
/// their name, `@symname` and `@model` end the scan.
std::string NetlistRun::header_ports(const PlannedDefinition& definition, const Parents& parents,
                                     bool& reached_end) {
  const Symbol& symbol = *definition.entry.symbol;
  std::string format = symbol_format(symbol);
  if (format.starts_with("tcleval(")) {
    format = env_.tcl != nullptr ? env_.tcl->evaluate(format, TclContext{}) : std::string{kTclFailure};
  }
  const std::vector<PinIndex> order = pin_order(symbol);
  const std::vector<PinIndex> listed = listed_pins(symbol, order);
  const HeaderScan scan{symbol, parents, order, listed, format};
  std::string out;
  bool stopped = false;
  detail::scan_format(
      scan.format, true, [](char /*c*/) {},
      [&](char sigil, const std::string& key, std::string_view source) {
        out += header_token(scan, sigil, key, source, stopped);
      });
  reached_end = !stopped;
  return out;
}

/// The template on the header: `name`, `spiceprefix`, `model` and the keys
/// in `extra` removed (Q-SCHEM-19), values decoded, separators kept, and
/// its LFs by the Q-SCHEM-21 branch point.
std::string NetlistRun::header_template(const PlannedDefinition& definition) {
  const Symbol& symbol = *definition.entry.symbol;
  const std::string templ = get_token(symbol.property(), "template").value_or(std::string{});
  const std::string extra = get_token(symbol.property(), "extra").value_or(std::string{});
  const std::vector<std::string_view> extra_words = words(extra);
  std::string out;
  for (const Piece& piece : PieceReader{templ}.read()) {
    if (!piece.attribute) {
      out += piece.text;
      continue;
    }
    if (piece.key == "name" || piece.key == "spiceprefix" || piece.key == "model") {
      continue;
    }
    const bool substring = !piece.key.empty() && extra.find(piece.key) != std::string::npos;
    const bool whole = std::find(extra_words.begin(), extra_words.end(), piece.key) != extra_words.end();
    if (substring && !whole) {
      warn(diag::make(net_codes::kExtraSubstring, "template key " + std::string{piece.key} + " of " +
                                                      symbol.reference + " occurs in extra=\"" + extra +
                                                      "\" only inside a word"));
    }
    if (compat_ ? substring : whole) {
      continue;
    }
    out += piece.key;
    out += piece.equals;
    out += decode_value(piece.value);
  }
  if (templ.find('\n') != std::string::npos) {
    warn(diag::make(net_codes::kTemplateLf, "the template of " + symbol.reference +
                                                " spans several lines; its .subckt header " +
                                                (compat_ ? "keeps the line breaks" : "joins them")));
    if (!compat_) {
      out = join_template_lines(std::move(out));
    }
  }
  return out;
}

}  // namespace hefesto::schem::detail
