// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The interned device-class table (lvs-netlist-input "Device classes").
//
// Owner: LVSREAD.

#include "hefesto/lvs/net/device_class.hpp"

#include <array>
#include <cstddef>

namespace hefesto::lvs {

namespace {

struct ClassInfo {
  DeviceClass device_class;
  std::string_view name;
  PinRange pins;
};

constexpr std::array<ClassInfo, 21> kClasses{{
    {DeviceClass::undefined, "undefined", {0, 0}},
    {DeviceClass::nmos, "nmos", {3, 3}},
    {DeviceClass::pmos, "pmos", {3, 3}},
    {DeviceClass::nmos4, "nmos4", {4, 4}},
    {DeviceClass::pmos4, "pmos4", {4, 4}},
    {DeviceClass::fet, "fet", {3, 4}},
    {DeviceClass::bjt, "bjt", {3, 3}},
    {DeviceClass::npn, "npn", {3, 3}},
    {DeviceClass::pnp, "pnp", {3, 3}},
    {DeviceClass::resistor, "resistor", {2, 2}},
    {DeviceClass::res3, "res3", {3, 3}},
    {DeviceClass::capacitor, "capacitor", {2, 2}},
    {DeviceClass::cap3, "cap3", {3, 3}},
    {DeviceClass::ecap, "ecap", {3, 3}},
    {DeviceClass::diode, "diode", {2, 2}},
    {DeviceClass::inductor, "inductor", {2, 2}},
    {DeviceClass::xline, "transmission line", {4, 4}},
    {DeviceClass::vsource, "voltage source", {0, 0}},
    {DeviceClass::isource, "current source", {0, 0}},
    {DeviceClass::module, "module", {0, 0}},
    {DeviceClass::subcircuit, "subcircuit", {0, 0}},
}};

const ClassInfo& info(DeviceClass device_class) noexcept {
  const auto index = static_cast<std::size_t>(device_class);
  return index < kClasses.size() ? kClasses.at(index) : kClasses.front();
}

}  // namespace

std::string_view device_class_name(DeviceClass device_class) noexcept {
  return info(device_class).name;
}

std::optional<DeviceClass> find_device_class(std::string_view name) noexcept {
  for (const auto& entry : kClasses) {
    if (entry.name == name) {
      return entry.device_class;
    }
  }
  return std::nullopt;
}

PinRange device_class_pins(DeviceClass device_class) noexcept {
  return info(device_class).pins;
}

}  // namespace hefesto::lvs
