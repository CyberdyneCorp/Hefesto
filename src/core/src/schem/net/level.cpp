// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// One schematic of a netlist run: its instances with their symbols and
// attributes ("Ignored and shorted objects"), its connectivity and net names
// (connectivity.hpp), the element text of each instance ("Format string
// substitution", "Hierarchical parameters") and the body ("Subcircuit
// body"), with the warnings of "Errors and warnings".
//
// Owner: NETLIST.

#include <algorithm>
#include <array>
#include <map>
#include <tuple>

#include "format_scan.hpp"
#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/expr.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "netlist_run.hpp"

namespace hefesto::schem::detail {

namespace {

/// The instance types that need no `name` ("Errors and warnings").
bool needs_no_name(std::string_view type) noexcept {
  static constexpr std::array<std::string_view, 7> kTypes{
      "package", "port_attributes", "architecture", "arch_declarations", "attributes", "netlist_options", "use"};
  return std::find(kTypes.begin(), kTypes.end(), type) != kTypes.end();
}

/// `true` or `open`: ignored; `short`: shorted ("Ignored and shorted
/// objects").
bool ignores(const std::optional<std::string>& value) {
  return value && (is_true(*value) || *value == "open");
}

bool shorts(const std::optional<std::string>& value) {
  return value && *value == "short";
}

char port_letter(std::string_view type) noexcept {
  if (type == "ipin") {
    return 'I';
  }
  return type == "opin" ? 'O' : 'B';
}

std::string file_stem(const platform::Path& path) {
  return path.stem().generic_string();
}

}  // namespace

std::string NetlistRun::substitute_parents(std::string_view text, const Parents& parents) {
  std::string value{text};
  if (value.find_first_of("@%") != std::string::npos) {
    const std::array<const PropertySource*, 2> chain{parents.instance, parents.templ};
    std::vector<const PropertySource*> present;
    for (const PropertySource* source : chain) {
      if (source != nullptr) {
        present.push_back(source);
      }
    }
    value = substitute_from_parents(value, present);
  }
  if (value.find("expr") != std::string::npos) {
    ExprText evaluated = substitute_expr(value, ExprOptions{compat_, env_.ev_precision});
    for (diag::Diagnostic& d : evaluated.warnings) {
      warn(std::move(d));
    }
    value = std::move(evaluated.text);
  }
  return value;
}

/// The value `@key` reads first: the instance property string, then the
/// symbol template.
std::string NetlistRun::attribute(const Placed& instance, std::string_view key) const {
  if (auto value = instance.instance.get(key, ValueForm::decoded)) {
    return *value;
  }
  return instance.templ.get(key, ValueForm::decoded).value_or(std::string{});
}

/// A `tcleval(...)` instance value evaluated ("Ignored and shorted
/// objects"); any other value unchanged.
std::string NetlistRun::evaluate_value(const std::string& value, const Placed& instance) const {
  if (!value.starts_with("tcleval(")) {
    return value;
  }
  if (env_.tcl == nullptr) {
    return std::string{kTclFailure};
  }
  TclContext context;
  context.instname = instance.name;
  context.symname = instance.record->symbol;
  std::string out = env_.tcl->evaluate(value, context);
  while (!out.empty() && out.back() == '\n') {
    out.pop_back();
  }
  return out;
}

void NetlistRun::place_one(Level& level, std::size_t index) {
  const Document& document = *level.document;
  Placed p;
  p.index = index;
  p.record = &document.instances[index];
  p.symbol = &env_.symbols.resolve(Reference{p.record->symbol}, document, resolve_);
  p.instance = PropertyString{p.record->props};
  p.symprops = PropertyString{p.symbol->property()};
  p.templ = PropertyString{get_token(p.symbol->property(), "template").value_or(std::string{})};
  p.type = symbol_type(*p.symbol);
  p.order = pin_order(*p.symbol);
  for (const PinIndex k : p.order) {
    p.pin_attributes.emplace_back(p.symbol->pins[k].box.props);
  }
  p.name = substitute_parents(attribute(p, "name"), level.parents);

  const auto own = [&](std::string_view key) -> std::optional<std::string> {
    auto value = p.instance.get(key, ValueForm::decoded);
    return value ? std::optional<std::string>{evaluate_value(*value, p)} : std::nullopt;
  };
  const auto symbol_value = [&](std::string_view key) { return p.symprops.get(key, ValueForm::decoded); };
  p.ignored = p.symbol->placeholder || ignores(own("spice_ignore")) || ignores(symbol_value("spice_ignore"));
  p.shorted = shorts(own("spice_ignore")) || shorts(symbol_value("spice_ignore"));
  if (options_.lvs_ignore) {
    p.ignored = p.ignored || ignores(own("lvs_ignore")) || ignores(symbol_value("lvs_ignore"));
    p.shorted = p.shorted || shorts(own("lvs_ignore")) || shorts(symbol_value("lvs_ignore"));
  }
  level.instances.push_back(std::move(p));
}

Level NetlistRun::place(const Document& document, Parents parents, bool top) {
  Level level;
  level.document = &document;
  level.parents = parents;
  level.top = top;
  level.instances.reserve(document.instances.size());
  for (std::size_t i = 0; i < document.instances.size(); ++i) {
    place_one(level, i);
  }
  return level;
}

void NetlistRun::connect(Level& level) {
  SchematicNets nets;
  nets.bus_budget = &bus_budget_;
  for (const WireRecord& wire : level.document->wires) {
    const auto spice = get_token(wire.props, "spice_ignore");
    const auto lvs = get_token(wire.props, "lvs_ignore");
    nets.wires.push_back({wire.p1, wire.p2, ignores(spice) || (options_.lvs_ignore && ignores(lvs))});
  }
  for (const Placed& p : level.instances) {
    nets.instances.push_back(net_instance(level, p));
  }
  level.naming = name_nets(nets);
  record_naming(level);
}

/// One instance as the connectivity sees it: its naming `lab`, `global`,
/// bus widths and pin points.
NetInstance NetlistRun::net_instance(const Level& level, const Placed& p) {
  NetInstance n;
  n.name = p.name;
  n.type = p.type;
  n.ignored = p.ignored;
  n.shorted = p.shorted;
  if (is_naming_type(p.type) || p.type == "bus_tap") {
    if (auto lab = p.instance.get("lab", ValueForm::decoded)) {
      n.lab = substitute_parents(*lab, level.parents);
    }
  }
  auto global = p.instance.get("global", ValueForm::decoded);
  if (!global) {
    global = p.symprops.get("global", ValueForm::decoded);
  }
  n.global = global && (is_true(*global) || *global == "ground");
  n.vector_width = width_of(p.name);
  const Placement placement{p.record->origin, p.record->rotation, p.record->flip};
  for (const PinIndex k : p.order) {
    const Pin& pin = p.symbol->pins[k];
    const std::string name = substitute_parents(pin.name, level.parents);
    n.pins.push_back({name, transform_point(placement, pin_point(pin.box)), width_of(name)});
  }
  return n;
}

/// The bus width of `name`, counted once per distinct name in a run (a
/// symbol placed many times repeats its pin names) and without building the
/// bit names.
std::size_t NetlistRun::width_of(const std::string& name) {
  const auto [it, inserted] = widths_.try_emplace(name, 0);
  if (inserted) {
    it->second = bus_width(name, &bus_budget_);
  }
  return it->second;
}

/// Shorts, globals and the computed `lab` values of one schematic, and the
/// warning for instances whose pins all touch nothing.
void NetlistRun::record_naming(const Level& level) {
  for (const diag::Diagnostic& d : level.naming.diagnostics) {
    result_.diagnostics.push_back(d);
  }
  result_.failed = result_.failed || level.naming.failed();
  for (const std::string& name : level.naming.globals) {
    if (globals_seen_.insert(name).second) {
      globals_.push_back(name);
    }
  }
  const platform::Path& path = level.document->path;
  for (const auto& [index, lab] : level.naming.label_labs) {
    result_.labels.push_back({path, index, lab, LabelTarget::instance});
  }
  std::map<std::string, std::size_t> uses;
  for (std::size_t w = 0; w < level.naming.wire_nets.size(); ++w) {
    const std::string& net = level.naming.wire_nets[w];
    if (!net.empty()) {
      result_.labels.push_back({path, w, net, LabelTarget::wire});
      uses[net] += 2;  // a wire counts as a connection
    }
  }
  count_pin_uses(level.naming, uses);
  for (const Placed& p : level.instances) {
    check_instance(level, p);
    const auto& pins = level.naming.pin_nets[p.index];
    const bool open = !p.ignored && !pins.empty() &&
                      std::all_of(pins.begin(), pins.end(), [&](const std::string& net) { return uses[net] <= 1; });
    if (open && !is_naming_type(p.type)) {
      warn(diag::make(net_codes::kAllPinsUnconnected, "instance " + p.name + " (" + p.record->symbol +
                                                          "): all pins unconnected"));
    }
  }
}

/// One use of each net per pin that touches it.
void NetlistRun::count_pin_uses(const NetNaming& naming, std::map<std::string, std::size_t>& uses) {
  for (const auto& pins : naming.pin_nets) {
    for (const std::string& net : pins) {
      ++uses[net];
    }
  }
}

/// "Errors and warnings": no `name`, no `type`, overlapping identical
/// instances.
void NetlistRun::check_instance(const Level& level, const Placed& instance) {
  if (instance.symbol->placeholder || instance.ignored) {
    return;
  }
  if (instance.type.empty() && untyped_warned_.insert(instance.symbol).second) {
    warn(diag::make(net_codes::kNoType, "Symbol: " + instance.record->symbol + ": no type attribute set"));
  }
  if (!instance.instance.defines("name") && !instance.templ.defines("name") && !needs_no_name(instance.type)) {
    warn(diag::make(net_codes::kNoName, "instance: " + std::to_string(instance.index) + " (" +
                                            instance.record->symbol + "): no name attribute set"));
  }
  for (std::size_t j = 0; j < instance.index; ++j) {
    const InstanceRecord& other = level.document->instances[j];
    const InstanceRecord& self = *instance.record;
    if (std::tie(other.symbol, other.origin.x, other.origin.y, other.rotation, other.flip) ==
        std::tie(self.symbol, self.origin.x, self.origin.y, self.rotation, self.flip)) {
      warn(diag::make(net_codes::kOverlapping, "instance " + instance.name + " overlaps instance " +
                                                   level.instances[j].name + " of the same symbol"));
      break;
    }
  }
}

ElementText NetlistRun::element(const Level& level, const Placed& p) {
  ElementContext ctx;
  ctx.instance = &p.instance;
  ctx.symbol = &p.symprops;
  ctx.templ = &p.templ;
  ctx.parent_instance = level.parents.instance;
  ctx.parent_template = level.parents.templ;
  const auto& nets = level.naming.pin_nets[p.index];
  for (std::size_t k = 0; k < p.order.size(); ++k) {
    const Pin& pin = p.symbol->pins[p.order[k]];
    ElementPin e;
    e.name = substitute_parents(pin.name, level.parents);
    e.width = width_of(e.name);
    if (k < nets.size() && !nets[k].empty()) {
      e.net = net_text(nets[k], level.naming, options_.bus_replacement_char, &bus_budget_);
    }
    e.spice_ignore = is_true(get_token(pin.box.props, "spice_ignore").value_or(std::string{}));
    e.attributes = &p.pin_attributes[k];
    ctx.pins.push_back(std::move(e));
  }
  ctx.index = p.index;
  ctx.symref = p.record->symbol;
  ctx.symname = symname_override(p);
  ctx.schname = level.document->path.filename().generic_string();
  ctx.topschname = topschname_;
  ctx.tcl.sch_basename = file_stem(level.document->path);
  ElementOptions options;
  options.lvs_netlist = options_.lvs_netlist;
  options.spiceprefix = options_.spiceprefix;
  options.xschem_compat = compat_;
  options.bus_replacement = options_.bus_replacement_char;
  options.ev_precision = env_.ev_precision;
  options.tcl = env_.tcl;
  ElementText text = build_element(ctx, options, element_state_);
  for (diag::Diagnostic& d : text.diagnostics) {
    warn(std::move(d));
  }
  text.diagnostics.clear();
  return text;
}

/// "Device models": the instance's `device_model`, else the symbol's, with
/// `@` tokens substituted, or Tcl-evaluated in `tcleval(...)` form.
void NetlistRun::collect_device_model(const Level& level, const Placed& p) {
  auto value = p.instance.get("device_model", ValueForm::decoded);
  if (!value) {
    value = p.symprops.get("device_model", ValueForm::decoded);
  }
  if (!value || value->empty()) {
    return;
  }
  if (value->starts_with("tcleval(")) {
    models_.add(evaluate_value(*value, p));
    return;
  }
  ElementContext ctx;
  ctx.instance = &p.instance;
  ctx.symbol = &p.symprops;
  ctx.templ = &p.templ;
  ctx.parent_instance = level.parents.instance;
  ctx.parent_template = level.parents.templ;
  std::string text;
  detail::scan_format(
      *value, false, [&](char c) { text += c; },
      [&](char /*sigil*/, const std::string& key, std::string_view source) {
        const auto resolved = resolve_parameter(ctx, key);
        text += resolved ? *resolved : std::string{source};
      });
  models_.add(std::move(text));
}

bool NetlistRun::pininfo(const Level& level) const {
  return options_.lvs_netlist || (level.top && (options_.top_is_subckt || pininfo_top_));
}

/// Item 1 of "Subcircuit body": the port comments, or one `*.PININFO` line.
void NetlistRun::port_lines(const Level& level) {
  std::string info = "*.PININFO ";
  bool any = false;
  for (const Placed& p : level.instances) {
    if (p.ignored || !is_port_type(p.type)) {
      continue;
    }
    if (!pininfo(level)) {
      emit(element(level, p).text);
      continue;
    }
    any = true;
    info += substitute_parents(attribute(p, "lab"), level.parents) + ":" + port_letter(p.type) + " ";
  }
  if (any) {
    emit(info + "\n");
  }
}

void NetlistRun::instance_line(const Level& level, const Placed& p) {
  ElementText text = element(level, p);
  emit(std::move(text.text), std::move(text.spice_prefix));
  collect_device_model(level, p);
}

/// "Subcircuit body".
void NetlistRun::body(Level& level) {
  port_lines(level);
  for (const Placed& p : level.instances) {
    if (p.ignored || p.shorted || p.type == "label" || is_port_type(p.type)) {
      continue;
    }
    const bool commands = p.type == "netlist_commands";
    if (level.top && commands) {
      continue;
    }
    if (!level.top && is_true(attribute(p, "only_toplevel"))) {
      continue;
    }
    if (commands) {
      emit(std::string{kBeginUserCode} + "\n" + element(level, p).text + std::string{kEndUserCode} + "\n");
      continue;
    }
    instance_line(level, p);
  }
}

}  // namespace hefesto::schem::detail
