// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Wire routing and junction dots of the wire tool (capability
// schematic-editing "Wires"). The tool computes the segments of a route
// and issues one `schematic wire` command per segment, so a replayed log
// draws the same wires.
//
// Owner: EDIT.

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/schem/file/document.hpp"

namespace hefesto::schem {

/// The route between the two points of a wire: horizontal then vertical,
/// vertical then horizontal, or one oblique segment.
enum class WireRoute : std::uint8_t { horizontal_first, vertical_first, oblique };

struct WireSegment {
  Point p1;
  Point p2;

  friend bool operator==(const WireSegment&, const WireSegment&) = default;
};

/// The segments of a wire from `from` to `to`: two for a Manhattan route
/// (one when the points share a coordinate), one for an oblique route, none
/// when the points are equal.
[[nodiscard]] std::vector<WireSegment> route_wire(Point from, Point to, WireRoute route);

/// The route after `route` in the cycle H-V, V-H, oblique.
[[nodiscard]] WireRoute next_route(WireRoute route) noexcept;

/// "Orthogonal routing": H-V when the pointer moved at least as far
/// horizontally as vertically from `from`, else V-H.
[[nodiscard]] WireRoute orthogonal_route(Point from, Point pointer) noexcept;

/// The junction dots of `document`: each wire endpoint where, counting
/// every other wire whose interior holds it as 2 and every other wire with
/// a coincident endpoint as 1, the count is greater than 1. Each point once,
/// in wire order.
[[nodiscard]] std::vector<Point> junction_points(const Document& document);

}  // namespace hefesto::schem
