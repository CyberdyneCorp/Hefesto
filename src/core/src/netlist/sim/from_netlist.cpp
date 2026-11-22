// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// from_netlist (netlist/sim/writer.hpp): the visit data ext2sim reads, taken
// from a flattened extflat::Netlist.
//
// The values EFVisitNodes and EFVisitDevs pass (node capacitance, lumped
// resistance, area/perimeter and attributes; device L, W, values and scale;
// EFScale and EFTech) are fields EXTFLAT adds to extflat's records in M7
// round 2 (tasks.md 10.x). Until both workstreams are integrated each is
// copied under an `if constexpr (requires ...)` guard, so this file builds
// with and without them; the guards go once the fields are on main.
//
// Owner: SIM (design D3).

#include <string>
#include <unordered_map>

#include "hefesto/netlist/sim/writer.hpp"

namespace hefesto::netlist::sim {

namespace {

HierName split(const std::string& name) {
  HierName parts;
  std::size_t start = 0;
  while (true) {
    const std::size_t slash = name.find('/', start);
    parts.push_back(name.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
    if (slash == std::string::npos) {
      return parts;
    }
    start = slash + 1;
  }
}

std::string joined(const HierName& name) {
  std::string out;
  for (const std::string& part : name) {
    out += (out.empty() ? "" : "/") + part;
  }
  return out;
}

using NodeIndex = std::unordered_map<std::string, std::size_t>;

// EFHNIsGlob, the non-Tcl test: the leaf component ends in `!`.
bool is_global(const HierName& name) {
  return !name.empty() && !name.back().empty() && name.back().back() == '!';
}

template <class Flat>
void node_values(const Flat& flat, SimNode& node) {
  if constexpr (requires { flat.cap, flat.resistance, flat.area_perim, flat.attributes; }) {
    node.cap = static_cast<double>(flat.cap);
    node.resist = flat.resistance;
    node.attributes = flat.attributes;
    for (const auto& [area, perim] : flat.area_perim) {
      node.area_perim.push_back(AreaPerim{area, perim});
    }
  }
}

SimNode node_of(const extflat::FlatNode& flat) {
  SimNode node;
  for (const std::vector<std::string>& name : flat.names) {
    node.names.push_back(NodeName{name, is_global(name)});
  }
  if (node.names.empty()) {
    const HierName name = split(flat.name);
    node.names.push_back(NodeName{name, is_global(name)});
  }
  node.location = flat.location;
  node.layer = flat.layer;
  node_values(flat, node);
  return node;
}

std::optional<std::size_t> find(const NodeIndex& index, const std::string& name) {
  const auto it = index.find(name);
  return it == index.end() ? std::nullopt : std::optional<std::size_t>{it->second};
}

std::string attribute_text(const std::vector<std::string>& attributes) {
  std::string text;
  for (const std::string& attr : attributes) {
    text += (text.empty() ? "" : ",") + attr;
  }
  return text;
}

template <class Flat>
void device_values(const Flat& flat, SimDevice& device) {
  if constexpr (requires { flat.length, flat.width, flat.res, flat.cap, flat.scale; }) {
    device.length = flat.length;
    device.width = flat.width;
    device.resistance = static_cast<double>(flat.res);
    device.capacitance = static_cast<double>(flat.cap);
    device.scale = flat.scale;
  }
}

template <class Flat>
void netlist_values(const Flat& flat, SimInput& input) {
  if constexpr (requires { flat.scale, flat.tech, flat.style; }) {
    input.scale = flat.scale;
    input.tech = flat.tech.value_or(std::string{});
    input.style_matched = flat.style.has_value();
  }
}

SimDevice device_of(const extflat::FlatDevice& flat, const NodeIndex& index) {
  SimDevice device;
  device.device_class = device_class(flat.device_class).value_or(DeviceClass::subckt);
  device.type = flat.model;
  device.lower_left = db::Point{flat.area.x0, flat.area.y0};
  device.prefix = flat.hier_prefix;
  for (const extflat::FlatTerminal& term : flat.terminals) {
    device.terminals.push_back(SimTerminal{find(index, term.node), attribute_text(term.attributes), {}, {}});
  }
  if (!flat.substrate.empty()) {
    const HierName parts = split(flat.substrate);
    device.substrate = SimDevice::Substrate{parts.back(), find(index, flat.substrate), flat.substrate, {}};
  }
  device_values(flat, device);
  return device;
}

}  // namespace

SimInput from_netlist(const extflat::Netlist& netlist) {
  SimInput input;
  netlist_values(netlist, input);
  NodeIndex index;
  netlist.visit_nodes([&](const extflat::FlatNode& flat) {
    const std::size_t at = input.nodes.size();
    input.nodes.push_back(node_of(flat));
    index.emplace(flat.name, at);
    for (const NodeName& name : input.nodes.back().names) {
      index.emplace(joined(name.name), at);
    }
  });
  netlist.visit_devices([&](const extflat::FlatDevice& flat) { input.devices.push_back(device_of(flat, index)); });
  netlist.visit_caps([&](const extflat::FlatCap& cap) { input.caps.push_back(SimCap{split(cap.a), split(cap.b), cap.cap}); });
  netlist.visit_resists([&](const extflat::FlatResist& resist) {
    input.resists.push_back(SimResist{split(resist.a), split(resist.b), static_cast<float>(resist.resistance)});
  });
  return input;
}

}  // namespace hefesto::netlist::sim
