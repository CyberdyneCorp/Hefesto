// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The nonzero-winding strip decomposer that turns a polygon into Manhattan
// rectangles and triangles (capability cmd-paint-erase "Polygon command",
// Q-CMDEDIT-19; shared with CIF/GDS polygon input, mask-input-styles, M5).
// (Magic ref: cif/CIFrdpt.c:604-636, cif/CIFrdpoly.c)
//
// Owner: EDIT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <optional>
#include <span>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::db {

/// One piece of a decomposed polygon: a rectangle, or the triangle of
/// `area` on the side `triangle` names.
struct PolygonPiece {
  Rect area;
  std::optional<TriangleSide> triangle;

  friend bool operator==(const PolygonPiece&, const PolygonPiece&) = default;
};

/// Decomposes the implicitly closed polygon `vertices` (at least three), in
/// CIFPath order (the `polygon` command passes its points reversed, as
/// Magic's PaintPolygon builds its path). The pieces are in Magic's paint
/// order: the triangles split off the diagonal edges, then the strips.
/// Fails only with fewer than three vertices.
[[nodiscard]] diag::Outcome<std::vector<PolygonPiece>> decompose_polygon(std::span<const Point> vertices);

}  // namespace hefesto::db
