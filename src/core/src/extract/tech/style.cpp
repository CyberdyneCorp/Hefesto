// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ExtStyle's queries and the free helpers of extract/tech/style.hpp. The
// builders live in style_table.cpp (load_styles, select) and conversion.cpp
// (micron_convert, rescale).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c
// ExtGetGateTypesMask, database/DBcellsrch.c DBScaleValue and
// ext2spice/ext2spice.c (the `$` substitution of esFetInfo), Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH (design D3, D7).

#include "hefesto/extract/tech/style.hpp"

namespace hefesto::extract {

std::vector<const DevicePattern*> ExtStyle::records_for(tech::TileType type) const {
  std::vector<const DevicePattern*> out;
  for (auto it = devices.rbegin(); it != devices.rend(); ++it) {
    if (it->types.test(type)) {
      out.push_back(&*it);
    }
  }
  return out;
}

tech::TypeMask ExtStyle::gate_types() const {
  tech::TypeMask gates;
  for (const DevicePattern& device : devices) {
    switch (device.device_class) {
      case DeviceClass::fet:
      case DeviceClass::mosfet:
      case DeviceClass::asymmetric:
      case DeviceClass::msubckt:
        gates |= device.types;
        break;
      default:
        break;
    }
  }
  return gates;
}

std::int32_t scale_distance(std::int32_t value, std::int32_t n, std::int32_t d) noexcept {
  if (n == 0) {
    return value;
  }
  std::int64_t v = std::int64_t{value} * d;
  if (v > 0) {
    v /= n;
  } else if (v < 0) {
    v = ((v + 1) / n) - 1;
  }
  return static_cast<std::int32_t>(v);
}

std::string substitute_substrate_node(
    std::string_view name, const std::function<std::optional<std::string>(std::string_view)>& lookup) {
  if (name.empty() || name.front() != '$' || !lookup) {
    return std::string{name};
  }
  if (std::optional<std::string> value = lookup(name.substr(1))) {
    return *std::move(value);
  }
  return std::string{name};
}

}  // namespace hefesto::extract
