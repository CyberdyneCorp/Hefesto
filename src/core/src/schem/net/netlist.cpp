// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The netlist run and its file structure (capability schematic-netlisting
// "Netlist options", "Netlist file structure" with the xschem-compat=off
// line and portable paths (Q-SCHEM-13), "Top-level port order" (Q-SCHEM-5),
// "Device models", "LVS netlist mode", "Flat netlist", "Errors and
// warnings", "In-process netlisting").
//
// Owner: NETLIST.

#include "hefesto/schem/net/netlist.hpp"

#include <algorithm>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/lib/reference.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/session.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "netlist_run.hpp"

namespace hefesto::schem {

namespace detail {

namespace {

/// `T` of `**.subckt T`: the file name without folder and extension.
std::string top_cell(const Document& top) {
  const std::string stem = top.path.stem().generic_string();
  return stem.empty() ? std::string{"untitled"} : stem;
}

/// "@topschname": the top file name without folder and extension; with a
/// `(` in it, every `.` becomes `_` and no extension is removed.
std::string topschname(const Document& top) {
  std::string name = top.path.filename().generic_string();
  if (name.find('(') != std::string::npos) {
    std::replace(name.begin(), name.end(), '.', '_');
    return name;
  }
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

bool lexically_inside(const platform::Path& file, const platform::Path& folder) {
  if (folder.empty()) {
    return false;
  }
  const platform::Path relative = file.lexically_relative(folder);
  return !relative.empty() && *relative.begin() != "..";
}

}  // namespace

NetlistRun::NetlistRun(const NetlistEnvironment& environment, const Document& top, const NetlistOptions& options,
                       bool xschem_compat)
    : env_{environment},
      top_{top},
      options_{options},
      compat_{xschem_compat},
      resolve_{environment.search_path, environment.fs, &resolve_warnings_, xschem_compat},
      bus_budget_{environment.bus_budget},
      topschname_{topschname(top)} {
  element_state_.bus_budget = &bus_budget_;
}

void NetlistRun::warn(diag::Diagnostic diagnostic) {
  result_.diagnostics.push_back(std::move(diagnostic));
}

void NetlistRun::error(diag::Diagnostic diagnostic) {
  result_.failed = true;
  result_.diagnostics.push_back(std::move(diagnostic));
}

void NetlistRun::emit(std::string text, std::string prefix, bool user_code) {
  if (!text.empty()) {
    chunks_.push_back(NetlistChunk{std::move(text), std::move(prefix), user_code});
  }
}

std::string NetlistRun::path_text(const platform::Path& file) const {
  return netlist_path(file, options_.portable_paths, env_.pdk_root, top_.path.parent_path());
}

/// "Netlist options": the keys a `netlist_options` instance defines, in
/// instance order. `top_is_subckt` from an instance selects the
/// `*.PININFO` lines only.
void NetlistRun::apply_options_instances(const Level& top) {
  for (const Placed& p : top.instances) {
    if (!p.ignored && p.type == "netlist_options") {
      apply_options_instance(p);
    }
  }
}

/// The keys one `netlist_options` instance defines.
void NetlistRun::apply_options_instance(const Placed& p) {
  if (const auto v = p.instance.get("bus_replacement_char", ValueForm::decoded); v && v->size() == 2) {
    options_.bus_replacement_char = *v;
  }
  if (const auto v = p.instance.get("lvs_ignore", ValueForm::decoded)) {
    options_.lvs_ignore = is_true(*v);
  }
  if (const auto v = p.instance.get("lvs_netlist", ValueForm::decoded)) {
    options_.lvs_netlist = is_true(*v);
  }
  if (const auto v = p.instance.get("top_is_subckt", ValueForm::decoded)) {
    pininfo_top_ = is_true(*v);
  }
  if (const auto v = p.instance.get("spiceprefix", ValueForm::decoded)) {
    options_.spiceprefix = !is_false(*v);
  }
  if (const auto v = p.instance.get("hiersep", ValueForm::decoded)) {
    options_.hiersep = *v;
  }
}

/// `place` of a `netlist_commands` instance: the symbol property first,
/// then the instance property.
std::string NetlistRun::place_of(const Placed& instance) const {
  if (auto place = instance.symprops.get("place", ValueForm::decoded)) {
    return *place;
  }
  return instance.instance.get("place", ValueForm::decoded).value_or(std::string{});
}

/// "Top-level port order": from `<top base name>.sym` when it exists and has
/// pins, else the port `lab` values in instance order, sorted by
/// `sim_pinnumber` (stable, Q-SCHEM-5) when every port has one.
std::string NetlistRun::top_ports(const Level& top) {
  if (auto ports = top_symbol_ports(top)) {
    return *ports;
  }
  std::vector<const Placed*> ports;
  for (const Placed& p : top.instances) {
    if (!p.ignored && is_port_type(p.type)) {
      ports.push_back(&p);
    }
  }
  sort_top_ports(ports);
  std::string out;
  for (const Placed* p : ports) {
    const std::string lab = substitute_parents(attribute(*p, "lab"), top.parents);
    const BusExpansion bus = parse_bus(lab, &bus_budget_);
    if (auto warning = bus_diagnostic(bus, lab)) {
      warn(std::move(*warning));
    }
    out += " " + replace_bus_chars(join_bits(bus.bits, ","), options_.bus_replacement_char);
  }
  return out;
}

/// The ports of `<top base name>.sym` when it exists and has pins.
std::optional<std::string> NetlistRun::top_symbol_ports(const Level& top) {
  const std::string reference = top_cell(top_) + ".sym";
  const ResolvedReference resolved =
      resolve_reference(reference, top_.path.parent_path(), env_.search_path, env_.fs);
  if (resolved.file.empty() || !env_.fs.exists(resolved.file)) {
    return std::nullopt;
  }
  const Symbol& symbol = env_.symbols.resolve(Reference{reference}, top_, resolve_);
  if (symbol.placeholder || symbol.pins.empty()) {
    return std::nullopt;
  }
  PlannedDefinition definition;
  definition.entry.symbol = &symbol;
  definition.entry.reference = reference;
  const PropertyString templ{get_token(symbol.property(), "template").value_or(std::string{})};
  bool reached_end = false;
  check_pins(symbol, top);
  return " " + header_ports(definition, Parents{nullptr, &templ}, reached_end);
}

/// The ports sorted by `sim_pinnumber` (stable) when every port has one;
/// a warning for each pair that shares a number.
void NetlistRun::sort_top_ports(std::vector<const Placed*>& ports) {
  const bool sorted = !ports.empty() && std::all_of(ports.begin(), ports.end(), [](const Placed* p) {
    return p->instance.defines("sim_pinnumber");
  });
  if (!sorted) {
    return;
  }
  const auto value = [](const Placed* p) {
    return sim_pinnumber_value(p->instance.get("sim_pinnumber", ValueForm::decoded).value_or(""));
  };
  std::stable_sort(ports.begin(), ports.end(), [&](const Placed* a, const Placed* b) { return value(a) < value(b); });
  for (std::size_t i = 1; i < ports.size(); ++i) {
    if (value(ports[i]) == value(ports[i - 1])) {
      warn(diag::make(net_codes::kDuplicatePortNumber, "ports " + ports[i - 1]->name + " and " + ports[i]->name +
                                                           " share sim_pinnumber " + std::to_string(value(ports[i])) +
                                                           "; they keep their instance order"));
    }
  }
}

/// Item 4 of "Netlist file structure".
void NetlistRun::top_line(const Level& top) {
  const std::string_view keyword = options_.uppercase_subckt ? ".SUBCKT " : ".subckt ";
  std::string line = top_subckt_ ? std::string{} : std::string{"**"};
  line += keyword;
  line += top_cell(top_);
  line += top_ports(top);
  emit(line + "\n");
}

/// Item 6 (`place` other than header and end) or item 10 (`end`) of
/// "Netlist file structure": the user code of the top level.
void NetlistRun::top_user_code(const Level& top, std::string_view place) {
  std::string text;
  bool any = false;
  for (const Placed& p : top.instances) {
    if (p.ignored || p.shorted || p.type != "netlist_commands") {
      continue;
    }
    const std::string where = place_of(p);
    const bool selected = place == "end" ? where == "end" : (where != "header" && where != "end");
    if (selected) {
      any = true;
      text += element(top, p).text;
    }
  }
  if (place == "end") {
    any = any || !models_.empty();
    text += models_.text();
  } else if (!top_.globals.s.empty()) {
    any = true;
    text += top_.globals.s + "\n";
  }
  if (any) {
    emit(std::string{kBeginUserCode} + "\n" + text + std::string{kEndUserCode} + "\n");
  }
}

void NetlistRun::globals_block() {
  std::string text;
  for (const std::string& name : globals_) {
    text += ".GLOBAL " + replace_bus_chars(name, options_.bus_replacement_char) + "\n";
  }
  emit(std::move(text));
}

NetlistResult NetlistRun::run() {
  Level top = place(top_, Parents{}, true);
  apply_options_instances(top);
  top_subckt_ = options_.lvs_netlist || options_.top_is_subckt;
  // Options may change what is ignored (lvs_ignore): place again with them.
  top = place(top_, Parents{}, true);
  connect(top);

  emit("** sch_path: " + path_text(top_.path) + "\n");
  compat::CompatSettings settings;
  settings.xschem_compat = compat_;
  if (const std::string line = compat::xschem_compat_netlist_line(settings); !line.empty()) {
    emit(line + "\n");
  }
  for (const Placed& p : top.instances) {
    if (!p.ignored && !p.shorted && p.type == "netlist_commands" && place_of(p) == "header") {
      emit(element(top, p).text, {}, true);
    }
  }
  if (options_.hierarchy != Hierarchy::top) {
    add_used(top);
    plan_definitions();
  }
  top_line(top);
  body(top);
  top_user_code(top, "architecture");
  const std::string_view ends = options_.uppercase_subckt ? ".ENDS" : ".ends";
  emit((top_subckt_ ? std::string{} : std::string{"**"}) + std::string{ends} + "\n");
  if (options_.hierarchy != Hierarchy::top) {
    write_definitions();
  }
  globals_block();
  top_user_code(top, "end");
  if (!top_subckt_) {
    emit(".end\n");
  }
  for (diag::Diagnostic& d : resolve_warnings_.take()) {
    warn(std::move(d));
  }
  std::vector<ProcessedLine> lines = postprocess(chunks_, compat_, &result_.diagnostics, &bus_budget_);
  if (options_.hierarchy == Hierarchy::flat) {
    FlatNetlist flat = flatten_netlist(lines, env_.flat_limits);
    lines = std::move(flat.lines);
    for (diag::Diagnostic& d : flat.warnings) {
      warn(std::move(d));
    }
  }
  result_.text = break_lines(lines);
  return std::move(result_);
}

// --- Device models -------------------------------------------------------

void DeviceModels::add(std::string value) {
  std::string key = device_model_key(value);
  if (values_.find(key) == values_.end()) {
    order_.push_back(key);
  }
  values_[std::move(key)] = std::move(value);
}

std::string DeviceModels::text() const {
  std::vector<std::vector<const std::string*>> buckets(367);
  for (const std::string& key : order_) {
    buckets[device_model_bucket(key)].push_back(&key);
  }
  std::string out;
  for (const auto& bucket : buckets) {
    for (const std::string* key : bucket) {
      const std::string& value = values_.find(*key)->second;
      out += value;
      if (!value.ends_with('\n')) {
        out += '\n';
      }
    }
  }
  return out;
}

}  // namespace detail

std::string netlist_path(const platform::Path& file, bool portable, const platform::Path& pdk_root,
                         const platform::Path& top_folder) {
  if (!portable) {
    return file.generic_string();
  }
  if (detail::lexically_inside(file, pdk_root)) {
    return "$PDK_ROOT/" + file.lexically_relative(pdk_root).generic_string();
  }
  if (top_folder.empty()) {
    return file.generic_string();
  }
  return file.lexically_relative(top_folder).generic_string();
}

namespace {

bool is_model_blank(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n';
}

/// The first two blank-separated words of `text` from `from` (fewer at the
/// end of the text).
std::vector<std::string> first_two_words(std::string_view text, std::size_t from) {
  std::vector<std::string> words;
  std::size_t i = from;
  while (i < text.size() && words.size() < 2) {
    while (i < text.size() && is_model_blank(text[i])) {
      ++i;
    }
    const std::size_t start = i;
    while (i < text.size() && !is_model_blank(text[i])) {
      ++i;
    }
    if (i > start) {
      words.emplace_back(text.substr(start, i - start));
    }
  }
  return words;
}

/// Where the words after the first `.subckt` (else `.model`) start; 0
/// without either.
std::size_t after_model_marker(std::string_view lower) noexcept {
  for (const std::string_view marker : {".subckt", ".model"}) {
    if (const std::size_t at = lower.find(marker); at != std::string_view::npos) {
      return at + marker.size();
    }
  }
  return 0;
}

}  // namespace

std::string device_model_key(std::string_view value) {
  std::string lower{value};
  for (char& c : lower) {
    c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }
  const auto words = first_two_words(lower, after_model_marker(lower));
  return words.size() >= 2 ? words[0] + words[1] : lower;
}

std::size_t device_model_bucket(std::string_view key) noexcept {
  std::uint32_t h = 5381;
  for (const char c : key) {
    h = h * 33U + static_cast<unsigned char>(c);
  }
  return h % 367U;
}

NetlistResult netlist(const NetlistEnvironment& environment, const Document& top, const NetlistOptions& options,
                      const compat::CompatSettings& compat) {
  return detail::NetlistRun{environment, top, options, compat.xschem_compat}.run();
}

NetlistResult netlist(const Session& session, const Document& top, const NetlistOptions& options,
                      const compat::CompatSettings& compat) {
  SymbolCache symbols;
  platform::LocalFileSystem fs;
  const NetlistEnvironment environment{symbols, session.search_path, fs, nullptr, {}, 4};
  return netlist(environment, top, options, compat);
}

}  // namespace hefesto::schem
