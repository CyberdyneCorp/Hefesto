// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The SPICE writer's input from a flattened netlist and the extraction
// style (netlist/spice/input.hpp).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c:917-964 (the
// esFetInfo table from ExtGetDevInfo) and extract/ExtBasic.c:59-61
// (extDevTable), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: SPICE.

#include "hefesto/netlist/spice/input.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace hefesto::netlist::spice {

namespace {

// extDevTable, in DEV_* order, with the old `fet` record's keyword first.
constexpr std::array<std::pair<std::string_view, DeviceClass>, 17> kClasses{{
    {"fet", DeviceClass::fet},
    {"mosfet", DeviceClass::mosfet},
    {"asymmetric", DeviceClass::asymmetric},
    {"bjt", DeviceClass::bjt},
    {"devres", DeviceClass::res},
    {"devcap", DeviceClass::cap},
    {"devcaprev", DeviceClass::caprev},
    {"vsource", DeviceClass::volt},
    {"diode", DeviceClass::diode},
    {"pdiode", DeviceClass::pdiode},
    {"ndiode", DeviceClass::ndiode},
    {"subckt", DeviceClass::subckt},
    {"rsubckt", DeviceClass::rsubckt},
    {"msubckt", DeviceClass::msubckt},
    {"csubckt", DeviceClass::csubckt},
    {"dsubckt", DeviceClass::dsubckt},
    {"veriloga", DeviceClass::veriloga},
}};

std::string joined(const std::vector<std::string>& parts) {
  std::string text;
  for (const std::string& part : parts) {
    text += (text.empty() ? "" : "/") + part;
  }
  return text;
}

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> parts;
  while (!path.empty()) {
    const std::size_t slash = path.find('/');
    parts.emplace_back(path.substr(0, slash));
    if (slash == std::string_view::npos) {
      break;
    }
    path.remove_prefix(slash + 1);
  }
  return parts;
}

InputNode node_of(const extflat::FlatNode& flat) {
  InputNode node;
  node.name = flat.names.empty() ? split_path(flat.name) : flat.names.front();
  node.aliases.push_back(flat.name);
  for (const auto& name : flat.names) {
    node.aliases.push_back(joined(name));
  }
  node.cap = flat.cap;
  for (const auto& [area, perim] : flat.area_perim) {
    node.pa.push_back(AreaPerim{area, perim});
  }
  node.attributes = flat.attributes;
  node.substrate = flat.substrate;
  node.port = flat.port;
  return node;
}

InputTerminal terminal_of(const extflat::FlatTerminal& flat) {
  InputTerminal term;
  term.node = flat.node;
  term.length = flat.length;
  if (!flat.attribute_text.empty()) {
    term.attributes = flat.attribute_text;  // dterm_attrs, kept raw as Magic does
  } else if (!flat.attributes.empty()) {
    std::string text;
    for (const std::string& a : flat.attributes) {
      text += (text.empty() ? "" : ",") + a;
    }
    term.attributes = std::move(text);
  }
  return term;
}

// The substrate name inside the device's own cell: the flat name without
// the device's instance path.
std::string local_name(const std::string& flat, const std::string& prefix) {
  if (!prefix.empty() && flat.size() > prefix.size() && flat.starts_with(prefix) && flat[prefix.size()] == '/') {
    return flat.substr(prefix.size() + 1);
  }
  return flat;
}

InputDevice device_of(const extflat::FlatDevice& flat) {
  InputDevice dev;
  dev.device_class = device_class_from(flat.device_class).value_or(DeviceClass::subckt);
  dev.type = flat.model;
  dev.rect = flat.area;
  for (const auto& t : flat.terminals) {
    dev.terminals.push_back(terminal_of(t));
  }
  if (!flat.substrate.empty()) {
    dev.substrate = flat.substrate;
    dev.substrate_local =
        flat.substrate_local.empty() ? local_name(flat.substrate, flat.hier_prefix) : flat.substrate_local;
  }
  dev.prefix = split_path(flat.hier_prefix);
  dev.params = flat.params;
  dev.length = flat.length;
  dev.width = flat.width;
  dev.area = flat.device_area;
  dev.perim = flat.perimeter;
  dev.resistance = flat.res;
  dev.capacitance = flat.cap;
  dev.scale = flat.scale;
  return dev;
}

// The `parameters` records (EFDevParams), keyed by device type or `:cell`.
void copy_params(const extflat::Netlist& netlist, SpiceInput& input) {
  for (const auto& [type, decls] : netlist.device_params) {
    std::vector<ParamDecl>& out = input.params[type];
    for (const extflat::DeviceParam& d : decls) {
      out.push_back(ParamDecl{d.type, d.name, d.scale, d.offset});
    }
  }
}

}  // namespace

std::optional<DeviceClass> device_class_from(std::string_view keyword) noexcept {
  for (const auto& [name, cls] : kClasses) {
    if (name == keyword) {
      return cls;
    }
  }
  return std::nullopt;
}

void add_type_name(SpiceInput& input, const std::string& type) {
  if (std::find(input.type_names.begin(), input.type_names.end(), type) == input.type_names.end()) {
    input.type_names.push_back(type);
  }
}

void apply_device_info(SpiceInput& input, std::span<const extract::DeviceInfo> table) {
  for (const extract::DeviceInfo& info : table) {
    add_type_name(input, info.model);
    DeviceTypeInfo& type = input.types[info.model];
    type.source_class = info.source_class;
    type.drain_class = info.drain_class;
    type.substrate_class = info.substrate_class;
    type.default_substrate =
        info.substrate_node.empty() ? std::nullopt : std::optional<std::string>{info.substrate_node};
    if (info.model == "None") {
      input.has_none_type = true;
    }
  }
}

SpiceInput from_netlist(const extflat::Netlist& netlist, std::string_view cell) {
  SpiceInput input;
  input.cell = std::string{cell};
  input.compat_substrate = netlist.substrate_compat;  // EFCompat
  input.scale = netlist.scale;                          // EFScale
  input.tech = netlist.tech.value_or(std::string{});    // EFTech
  input.style_matches = netlist.style.has_value();     // EFStyle != NULL
  copy_params(netlist, input);
  netlist.visit_nodes([&](const extflat::FlatNode& flat) {
    input.nodes.push_back(node_of(flat));
    if (flat.top_port) {
      input.ports.push_back(RootPort{flat.name, -1});
    }
  });
  netlist.visit_devices([&](const extflat::FlatDevice& flat) {
    input.devices.push_back(device_of(flat));
    add_type_name(input, flat.model);
  });
  netlist.visit_caps([&](const extflat::FlatCap& cap) { input.caps.push_back(cap); });
  netlist.visit_resists([&](const extflat::FlatResist& r) { input.resists.push_back(r); });
  return input;
}

}  // namespace hefesto::netlist::spice
