// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Stitch navigation for the basic check over db::Plane's read-only tile
// views (see kernel.hpp): point location and the left/bottom neighbor
// walks of drcTile.
//
// (Magic ref: tiles/tile.h GOTOPOINT, BL/LB/TR/RT stitches)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include <algorithm>

#include "kernel.hpp"

namespace hefesto::drc::check_detail {

namespace {

const tech::TypeMask& all_types() {
  static const tech::TypeMask all = tech::TypeMask::all();
  return all;
}

bool same_tile(const TileView& a, const TileView& b) noexcept { return a.area == b.area; }

}  // namespace

TileView tile_at(const db::Plane& plane, Coord x, Coord y) {
  TileView found;
  plane.enumerate(Rect{x, y, x + 1, y + 1}, all_types(), [&](const TileView& t) {
    found = t;
    return false;
  });
  return found;
}

void tiles_in(const db::Plane& plane, const Rect& area, std::vector<TileView>& out) {
  out.clear();
  plane.enumerate(area, all_types(), [&](const TileView& t) {
    if (out.empty() || !same_tile(out.back(), t)) {
      out.push_back(t);
    }
    return true;
  });
}

void left_neighbors(const db::Plane& plane, const TileView& t, Coord y0, Coord y1, std::vector<TileView>& out) {
  out.clear();
  if (y0 >= y1) {
    return;
  }
  tiles_in(plane, Rect{t.area.x0 - 1, y0, t.area.x0, y1}, out);
  std::sort(out.begin(), out.end(), [](const TileView& a, const TileView& b) { return a.area.y0 < b.area.y0; });
  out.erase(std::unique(out.begin(), out.end(), same_tile), out.end());
}

void bottom_neighbors(const db::Plane& plane, const TileView& t, Coord x0, Coord x1, std::vector<TileView>& out) {
  out.clear();
  if (x0 >= x1) {
    return;
  }
  tiles_in(plane, Rect{x0, t.area.y0 - 1, x1, t.area.y0}, out);
  std::sort(out.begin(), out.end(), [](const TileView& a, const TileView& b) { return a.area.x0 < b.area.x0; });
  out.erase(std::unique(out.begin(), out.end(), same_tile), out.end());
}

}  // namespace hefesto::drc::check_detail
