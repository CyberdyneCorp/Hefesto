// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Polygon painting for the LEF and DEF readers
// (openspec/changes/m6-lef-def-oasis/design.md D4): db::decompose_polygon
// plus the port labels a pin polygon carries, shared by LEF POLYGON
// (Q-LEFDEF-23: one decomposition, identical on every plane), DEF pin
// polygons (Q-LEFDEF-66 OFF) and DEF route polygons (Q-LEFDEF-70 OFF).
// (Magic ref: lef/lefRead.c:1037-1066 LefPaintPolygon, cif/CIFrdpt.c:604-636
// PaintPolygon)
//
// Owner: LEFVIA (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::lef {

/// The rectangles of the polygon `vertices` (as read, in file order): the
/// strips of its decomposition in Magic's list order (the strip found last
/// first). The triangles split off its diagonal edges are not in the list.
/// PaintPolygon builds its path by prepending each point, so the
/// decomposition runs over the vertices reversed.
[[nodiscard]] std::vector<db::Rect> polygon_rects(std::span<const db::Point> vertices);

/// Paints the polygon `vertices` (internal units, file order) of `type` into
/// `cell`, triangles included, and returns its rectangles (polygon_rects),
/// which the reader makes into port labels. Fails with fewer than three
/// vertices.
[[nodiscard]] diag::Outcome<std::vector<db::Rect>> paint_polygon_rects(db::CellDef& cell, const tech::Technology& tech,
                                                                       tech::TileType type,
                                                                       std::span<const db::Point> vertices);

/// A pin's port label: its name and port index.
struct PortLabel {
  std::string_view name;
  std::uint32_t index = 0;
};

/// paint_polygon_rects, then, with `port`, one sticky port label on each
/// rectangle (LefReadPort without pin attributes: all four directions,
/// automatic justification; a port label of the same name already in the
/// cell gives its index).
[[nodiscard]] diag::Status paint_polygon(db::CellDef& cell, const tech::Technology& tech, tech::TileType type,
                                         std::span<const db::Point> vertices,
                                         const std::optional<PortLabel>& port = std::nullopt);

}  // namespace hefesto::lef
