// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The device classes of a netlist cell (capability lvs-netlist-input
// "Device classes"; openspec/changes/m9-lvs-simulation design.md D6). The
// classes are interned: one enumerator each, with a constant table of names
// and pin counts.
//
// Owner: LVSREAD.

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace hefesto::lvs {

enum class DeviceClass : std::uint8_t {
  undefined,
  nmos,
  pmos,
  nmos4,
  pmos4,
  fet,
  bjt,
  npn,
  pnp,
  resistor,
  res3,
  capacitor,
  cap3,
  ecap,
  diode,
  inductor,
  xline,  // transmission line
  vsource,
  isource,
  module,      // black box: compared as a device with pin checking
  subcircuit,  // the only flattenable class
};

/// Pin counts a class allows; `max == 0` means any count (undefined,
/// sources, module, subcircuit).
struct PinRange {
  std::uint8_t min = 0;
  std::uint8_t max = 0;
};

/// The class's name as written in reports ("nmos", "transmission line", ...).
[[nodiscard]] std::string_view device_class_name(DeviceClass device_class) noexcept;

/// The class named `name` (an exact device_class_name()); nullopt if none.
[[nodiscard]] std::optional<DeviceClass> find_device_class(std::string_view name) noexcept;

[[nodiscard]] PinRange device_class_pins(DeviceClass device_class) noexcept;

/// Every class except subcircuit and module.
[[nodiscard]] constexpr bool is_primitive(DeviceClass device_class) noexcept {
  return device_class != DeviceClass::subcircuit && device_class != DeviceClass::module;
}

/// Only subcircuit cells are flattened and descended into.
[[nodiscard]] constexpr bool is_flattenable(DeviceClass device_class) noexcept {
  return device_class == DeviceClass::subcircuit;
}

}  // namespace hefesto::lvs
