// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// CPU triangulation of filled outlines for the canvas renderer: the
// nonzero-winding fill of glyph contours (display-style-files / font-labels
// "rendering": Magic fills font label outlines with the GLU tessellator,
// nonzero rule) and the triangles of non-Manhattan tiles. The result is a
// triangle list for the polygon pipeline (hefesto/render/scene.hpp
// PolygonScene). Vulkan-free and deterministic.
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstddef>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/display/font.hpp"
#include "hefesto/render/scene.hpp"

namespace hefesto::render {

/// Most edges fill_nonzero accepts for one call; larger inputs are refused
/// (a label's glyphs stay far below this).
inline constexpr std::size_t kMaxFillEdges = 8192;

/// Appends to `out` the triangles covering the nonzero-winding fill of
/// `contours` (each a closed polygon, either orientation, possibly
/// overlapping or self-intersecting), relative to (base_x, base_y).
/// Horizontal slabs between every vertex and edge-crossing height are split
/// into trapezoids, so the fill is exact for straight edges. Points that are
/// not finite are ignored. Returns false (and appends nothing) when the
/// input has more than kMaxFillEdges edges.
bool fill_nonzero(const std::vector<std::vector<display::PointF>>& contours, double base_x, double base_y,
                  std::vector<PolygonVertex>& out);

/// Appends the three vertices of a split tile's triangle: `bounds` is the
/// tile, `diagonal` its diagonal and `right_side` which part of it is meant
/// (db::Diagonal: `\` keeps the lower-left part on the left, `/` the
/// upper-left part).
void append_triangle(const db::Rect& bounds, db::Diagonal diagonal, bool right_side, double base_x, double base_y,
                     std::vector<PolygonVertex>& out);

}  // namespace hefesto::render
