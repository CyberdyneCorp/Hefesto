// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The connectivity of the edited schematic (design.md D11: net highlighting
// and bus editing reuse NETLIST's connectivity for one schematic) and the
// bus flags of "Bus editing".
//
// Each connectivity update has its own bus budget, as each netlist run has
// (Q-SCHEM-41): it is recomputed after every edit, so charging the
// session's budget would use it up by editing alone. Instance names and the
// element names of a descent take from the session budget.
//
// Owner: EDIT.

#include <string>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/file/transform.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "hefesto/schem/sym/types.hpp"
#include "internal.hpp"

namespace hefesto::schem::editimpl {

namespace {

bool ignored_value(const std::optional<std::string>& value) {
  return value && is_true(*value);
}

NetInstance net_instance(EditContext& ec, const InstanceRecord& record, BusBudget* budget) {
  const Symbol& symbol = ec.symbol(record);
  NetInstance n;
  n.name = get_token(record.props, "name").value_or(std::string{});
  n.type = symbol_type(symbol);
  n.ignored = symbol.placeholder || ignored_value(get_token(record.props, "spice_ignore")) ||
              ignored_value(get_token(symbol.property(), "spice_ignore"));
  n.shorted = get_token(record.props, "spice_ignore") == "short";
  if (is_naming_type(n.type) || n.type == "bus_tap") n.lab = get_token(record.props, "lab");
  auto global = get_token(record.props, "global");
  if (!global) global = get_token(symbol.property(), "global");
  n.global = global && (is_true(*global) || *global == "ground");
  n.vector_width = bus_width(n.name, budget);
  const Placement placement{record.origin, record.rotation, record.flip};
  for (const PinIndex k : pin_order(symbol)) {
    const Pin& pin = symbol.pins[k];
    n.pins.push_back({pin.name, transform_point(placement, pin_point(pin.box)), bus_width(pin.name, budget)});
  }
  return n;
}

std::size_t net_width(const NetNaming& naming, const std::string& net, BusBudget* budget) {
  if (net.starts_with('#')) {
    const auto it = naming.multiplicity.find(net);
    return it == naming.multiplicity.end() ? 1 : it->second;
  }
  return bus_width(net, budget);
}

void set_bus_flag(WireRecord& wire, bool bus) {
  const auto value = get_token(wire.props, "bus");
  if (bus && value != "1") wire.props = set_token(wire.props, "bus", "1");
  if (!bus && value) wire.props = set_token(wire.props, "bus", std::nullopt);
}

std::string mismatch_text(const NetInstance& instance, const NetPin& pin, const std::string& net, std::size_t width) {
  return "schematic: bus width mismatch at pin " + pin.name + " of " + instance.name + ": net " + net + " has " +
         std::to_string(width) + " bits, the pin " + std::to_string(pin.width) + " and the instance " +
         std::to_string(instance.vector_width);
}

void width_mismatches(const DocumentNets& d, BusBudget* budget, std::set<std::string>& out) {
  for (std::size_t i = 0; i < d.nets.instances.size() && i < d.naming.pin_nets.size(); ++i) {
    const NetInstance& instance = d.nets.instances[i];
    for (std::size_t p = 0; p < instance.pins.size() && p < d.naming.pin_nets[i].size(); ++p) {
      const std::string& net = d.naming.pin_nets[i][p];
      if (net.empty()) continue;
      const NetPin& pin = instance.pins[p];
      const std::size_t width = net_width(d.naming, net, budget);
      if (width != pin.width && width != pin.width * instance.vector_width) {
        out.insert(mismatch_text(instance, pin, net, width));
      }
    }
  }
}

}  // namespace

DocumentNets document_nets(EditContext& ec, OpenDocument& open) {
  EditContext other(ec.ctx(), ec.session(), open);
  return document_nets(other);
}

DocumentNets document_nets(EditContext& ec) {
  DocumentNets d;
  BusBudget budget;  // one connectivity pass, as one netlist run
  d.nets.bus_budget = &budget;
  for (const WireRecord& wire : ec.doc().wires) {
    d.nets.wires.push_back({wire.p1, wire.p2, ignored_value(get_token(wire.props, "spice_ignore"))});
  }
  for (const InstanceRecord& record : ec.doc().instances) d.nets.instances.push_back(net_instance(ec, record, &budget));
  d.naming = name_nets(d.nets);
  d.nets.bus_budget = nullptr;
  return d;
}

void update_buses(EditContext& ec) {
  const DocumentNets d = document_nets(ec);
  BusBudget budget;
  Document& doc = ec.doc();
  for (std::size_t i = 0; i < doc.wires.size() && i < d.naming.wire_nets.size(); ++i) {
    const std::string& net = d.naming.wire_nets[i];
    set_bus_flag(doc.wires[i], !net.empty() && net_width(d.naming, net, &budget) > 1);
  }
  std::set<std::string> mismatches;
  width_mismatches(d, &budget, mismatches);
  std::set<std::string>& reported = ec.session().edit.width_warnings[ec.id()];
  for (const std::string& text : mismatches) {
    if (!reported.contains(text)) ec.ctx().emit(diag::make(kWidthMismatch, text));
  }
  reported = std::move(mismatches);
}

}  // namespace hefesto::schem::editimpl
