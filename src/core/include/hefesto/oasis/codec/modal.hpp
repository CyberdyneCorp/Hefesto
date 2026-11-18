// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS modal variables (capability oasis-format "Modal variables and
// reset rules"; openspec/changes/m6-lef-def-oasis/design.md D4, D11,
// Q-OASIS-5): the state a record's omitted fields are taken from. The
// decoder and the encoder each keep one. Both reset it at START, at every
// CELL record and at every name record (Q-OASIS-5, as the KLayout reader
// does); a CBLOCK boundary does not reset it.
//
// Owner: OASFMT (design D2; the header was created by the M6 scaffold).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/oasis/codec/elements.hpp"
#include "hefesto/oasis/codec/property.hpp"

namespace hefesto::oasis {

/// The modal variables; nullopt = undefined (a record that relies on an
/// undefined variable is an error). A default-constructed state is the
/// state after a reset: the x and y variables are 0 and xy-mode absolute.
struct ModalState {
  std::optional<Repetition> repetition;
  std::optional<std::int64_t> placement_x = 0;
  std::optional<std::int64_t> placement_y = 0;
  /// placement-cell: set by name or by CELLNAME refnum (one of the two).
  std::optional<std::string> placement_cell;
  std::optional<std::uint64_t> placement_cell_reference;
  std::optional<std::uint64_t> layer;
  std::optional<std::uint64_t> datatype;
  std::optional<std::uint64_t> textlayer;
  std::optional<std::uint64_t> texttype;
  std::optional<std::int64_t> text_x = 0;
  std::optional<std::int64_t> text_y = 0;
  /// text-string: set inline or by TEXTSTRING refnum (one of the two).
  std::optional<std::string> text_string;
  std::optional<std::uint64_t> text_string_reference;
  std::optional<std::int64_t> geometry_x = 0;
  std::optional<std::int64_t> geometry_y = 0;
  bool xy_absolute = true;
  std::optional<std::int64_t> geometry_w;
  std::optional<std::int64_t> geometry_h;
  /// polygon-point-list: the vertices as in Element::points, and the type.
  std::optional<std::vector<Point64>> polygon_points;
  std::optional<std::uint8_t> polygon_point_list_type;
  std::optional<std::int64_t> path_half_width;
  std::optional<std::vector<Point64>> path_points;
  std::optional<std::uint8_t> path_point_list_type;
  std::optional<std::int64_t> path_start_extension;
  std::optional<std::int64_t> path_end_extension;
  std::optional<std::uint8_t> ctrapezoid_type;
  std::optional<std::int64_t> circle_radius;
  /// last-property-name: inline or by PROPNAME refnum (one of the two).
  std::optional<std::string> last_property_name;
  std::optional<std::uint64_t> last_property_name_reference;
  std::optional<std::vector<PropertyValue>> last_property_values;
  std::optional<bool> last_property_standard;

  /// The reset of START, CELL and every name record (Q-OASIS-5): every
  /// variable becomes undefined, except the x and y variables, which become
  /// 0, and xy-mode, which becomes absolute.
  void reset() { *this = ModalState{}; }

  friend bool operator==(const ModalState&, const ModalState&) = default;
};

}  // namespace hefesto::oasis
