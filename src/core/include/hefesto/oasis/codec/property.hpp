// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// OASIS PROPERTY values and the standard property names (capability
// oasis-format "PROPERTY records", "Standard properties";
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D11).
//
// Owner: OASFMT (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::oasis {

/// One property value. Value types 0-7 decode to `real`, 8 to `unsigned`,
/// 9 to `signed`, 10-12 to the a-, b- and n-string kinds and 13-15 to the
/// PROPSTRING references of those kinds.
struct PropertyValue {
  enum class Kind : std::uint8_t {
    real,
    unsigned_integer,
    signed_integer,
    a_string,
    b_string,
    n_string,
    a_reference,
    b_reference,
    n_reference,
  };

  Kind kind = Kind::unsigned_integer;
  double real = 0.0;
  std::uint64_t unsigned_value = 0;
  std::int64_t signed_value = 0;
  std::string string;          // the string kinds
  std::uint64_t reference = 0;  // the PROPSTRING reference kinds

  [[nodiscard]] static PropertyValue of_unsigned(std::uint64_t value);
  [[nodiscard]] static PropertyValue of_string(Kind kind, std::string value);

  friend bool operator==(const PropertyValue&, const PropertyValue&) = default;
};

/// One PROPERTY record after modal resolution. `name` is set when the name
/// was given inline, `name_reference` when it was a PROPNAME refnum.
struct Property {
  std::string name;
  std::optional<std::uint64_t> name_reference;
  bool standard = false;
  std::vector<PropertyValue> values;

  friend bool operator==(const Property&, const Property&) = default;
};

/// The standard properties Hefesto recognises (oasis-format "Standard
/// properties").
enum class StandardProperty : std::uint8_t {
  max_signed_integer_width,
  max_unsigned_integer_width,
  top_cell,
  bounding_boxes_available,
  bounding_box,
  cell_offset,
  gds_property,
};

/// The standard property named `name`, if it is one of the seven.
[[nodiscard]] std::optional<StandardProperty> standard_property(std::string_view name) noexcept;

/// The name of `property`, e.g. "S_CELL_OFFSET".
[[nodiscard]] std::string_view standard_property_name(StandardProperty property) noexcept;

}  // namespace hefesto::oasis
