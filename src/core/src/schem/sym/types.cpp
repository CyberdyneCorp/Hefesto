// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol types (capability schematic-symbols-and-libraries "Symbol types").
//
// Owner: SYMLIB.

#include "hefesto/schem/sym/types.hpp"

#include <algorithm>
#include <array>

#include "hefesto/schem/file/properties.hpp"

namespace hefesto::schem {

namespace {

constexpr std::array<std::string_view, 12> kIgnoredInSchematicSymbol = {
    "logo",       "netlist_commands", "netlist_options",      "arch_declarations",
    "architecture", "attributes",     "package",              "port_attributes",
    "use",        "launcher",         "verilog_preprocessor", "timescale",
};

// Every type of the "Symbol types" table.
constexpr std::array<std::string_view, 24> kTableTypes = {
    "subcircuit", "primitive", "ipin",      "opin",      "iopin",        "label",
    "netlist_commands", "netlist_options", "launcher", "logo", "probe", "ammeter",
    "noconn",     "show_label", "bus_tap",  "scope",     "use",          "package",
    "architecture", "arch_declarations", "attributes", "port_attributes", "timescale",
    "verilog_preprocessor",
};

template <std::size_t N>
bool contains(const std::array<std::string_view, N>& list, std::string_view type) noexcept {
  return std::find(list.begin(), list.end(), type) != list.end();
}

}  // namespace

std::string symbol_type(const Symbol& symbol) {
  return get_token(symbol.property(), "type").value_or("");
}

bool can_descend(std::string_view type) noexcept {
  return type == "subcircuit" || type == "primitive";
}

bool is_port_type(std::string_view type) noexcept {
  return !port_direction(type).empty();
}

std::string_view port_direction(std::string_view type) noexcept {
  if (type == "ipin") return "in";
  if (type == "opin") return "out";
  if (type == "iopin") return "inout";
  return {};
}

bool ignored_in_schematic_symbol(std::string_view type) noexcept {
  return contains(kIgnoredInSchematicSymbol, type);
}

bool is_device_type(std::string_view type) noexcept {
  return !contains(kTableTypes, type);
}

}  // namespace hefesto::schem
