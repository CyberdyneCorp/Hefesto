// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Stitch navigation for the basic check over db::Plane's read-only tile
// views (see kernel.hpp): point location and the left/bottom neighbor
// walks of drcTile, through db::Plane::tile_at() and Plane::neighbors(),
// which walk the corner stitches from the view's own tile as Magic does.
//
// (Magic ref: tiles/tile.h GOTOPOINT, BL/LB/TR/RT stitches)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include "kernel.hpp"

namespace hefesto::drc::check_detail {

void left_neighbors(const db::Plane& plane, const TileView& t, Coord y0, Coord y1, std::vector<TileView>& out) {
  plane.neighbors(t, db::Side::left, y0, y1, out);
}

void bottom_neighbors(const db::Plane& plane, const TileView& t, Coord x0, Coord x1, std::vector<TileView>& out) {
  plane.neighbors(t, db::Side::bottom, x0, x1, out);
}

}  // namespace hefesto::drc::check_detail
