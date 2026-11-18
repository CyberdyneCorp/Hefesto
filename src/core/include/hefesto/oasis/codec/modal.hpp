// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The OASIS modal variables (capability oasis-format "Modal variables";
// openspec/changes/m6-lef-def-oasis/design.md D4, D11, Q-OASIS-5): the
// state a record's omitted fields are taken from. The decoder and the
// encoder each keep one; both reset it at the start of each CELL and after
// each CBLOCK boundary as the specification requires.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASFMT fills
// in the reset rules in src/core/src/oasis/codec/ (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/oasis/codec/elements.hpp"

namespace hefesto::oasis {

/// The modal variables; nullopt = undefined (a record that relies on an
/// undefined variable is an error).
struct ModalState {
  std::optional<Repetition> repetition;
  std::optional<std::int64_t> placement_x;
  std::optional<std::int64_t> placement_y;
  std::optional<std::string> placement_cell;
  std::optional<std::uint64_t> layer;
  std::optional<std::uint64_t> datatype;
  std::optional<std::uint64_t> textlayer;
  std::optional<std::uint64_t> texttype;
  std::optional<std::int64_t> text_x;
  std::optional<std::int64_t> text_y;
  std::optional<std::string> text_string;
  std::optional<std::int64_t> geometry_x;
  std::optional<std::int64_t> geometry_y;
  bool xy_absolute = true;
  std::optional<std::int64_t> geometry_w;
  std::optional<std::int64_t> geometry_h;
  std::optional<std::vector<Point64>> polygon_points;
  std::optional<std::int64_t> path_half_width;
  std::optional<std::vector<Point64>> path_points;
  std::optional<std::int64_t> path_start_extension;
  std::optional<std::int64_t> path_end_extension;
  std::optional<std::uint8_t> ctrapezoid_type;
  std::optional<std::int64_t> circle_radius;
  std::optional<std::string> last_property_name;

  /// The reset at the start of a CELL record (Q-OASIS-5): every variable
  /// becomes undefined and xy-mode absolute. SCAFFOLD: resets everything
  /// until OASFMT lands the exact rule.
  void reset() { *this = ModalState{}; }

  friend bool operator==(const ModalState&, const ModalState&) = default;
};

}  // namespace hefesto::oasis
