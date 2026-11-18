// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PROPERTY values and the standard property names (capability oasis-format
// "Standard properties"; design D11).
//
// Owner: OASFMT (design D2).

#include "hefesto/oasis/codec/property.hpp"

#include <array>
#include <utility>

namespace hefesto::oasis {

namespace {

constexpr std::array<std::string_view, 7> kNames = {
    "S_MAX_SIGNED_INTEGER_WIDTH", "S_MAX_UNSIGNED_INTEGER_WIDTH", "S_TOP_CELL",     "S_BOUNDING_BOXES_AVAILABLE",
    "S_BOUNDING_BOX",             "S_CELL_OFFSET",                "S_GDS_PROPERTY",
};

}  // namespace

PropertyValue PropertyValue::of_unsigned(std::uint64_t value) {
  PropertyValue v;
  v.kind = Kind::unsigned_integer;
  v.unsigned_value = value;
  return v;
}

PropertyValue PropertyValue::of_string(Kind kind, std::string value) {
  PropertyValue v;
  v.kind = kind;
  v.string = std::move(value);
  return v;
}

std::optional<StandardProperty> standard_property(std::string_view name) noexcept {
  for (std::size_t i = 0; i < kNames.size(); ++i) {
    if (kNames[i] == name) {
      return static_cast<StandardProperty>(i);
    }
  }
  return std::nullopt;
}

std::string_view standard_property_name(StandardProperty property) noexcept {
  return kNames[static_cast<std::size_t>(property)];
}

}  // namespace hefesto::oasis
