// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CIF wire outlines (capability cif-format-io "Wire command"; Magic's
// CIFPaintWirePath with end caps, cif/CIFrdpt.c:339-560). A wire path in
// doubled read units and a width in read units become closed outlines in
// read units, with square half-width end extensions in both magic-compat
// settings (Q-MASK-333, replicate). The arithmetic is Magic's: doubles in
// Magic's operation order, 90 degrees as 1.5708 rad, round half away from
// zero (design D22). The outline points keep Magic's list order (one side
// prepended, the other appended), because the polygon conversion paints
// in that order.
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D2, D13).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/mask/plane/plane.hpp"

namespace hefesto::cif {

/// A point in read units, kept in 64 bits until it is range-checked.
struct ReadPoint {
  std::int64_t x = 0;
  std::int64_t y = 0;

  friend bool operator==(const ReadPoint&, const ReadPoint&) = default;
};

struct WireOutline {
  /// The closed outlines, in read units, in the order Magic paints them.
  std::vector<mask::Polygon> polygons;
  /// Magic's TxError warnings (direction reversal, duplicate point).
  std::vector<std::string> warnings;
  /// Set when a junction exceeded the mitre limit: the outline being built
  /// is dropped and the rest of the route ignored.
  bool truncated = false;
  std::string mitre_message;  // Magic's first line; it then prints "Route has been truncated."
  /// A computed point left the usable plane; `polygons` must not be used.
  bool out_of_range = false;
};

/// The outlines of a wire of width `width` (read units) along `path`
/// (doubled read units, at least one point).
[[nodiscard]] WireOutline wire_outline(const std::vector<ReadPoint>& path, std::int64_t width);

}  // namespace hefesto::cif
