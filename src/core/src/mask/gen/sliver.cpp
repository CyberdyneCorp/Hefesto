// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sliver growth of hierarchical pieces (capability
// mask-hierarchical-generation "Sliver growth of hierarchical pieces",
// Q-MASK-409; design D8).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFhier.c (cifGrowSliver),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). Magic's two `printf` debug lines are not
// printed.
//
// Owner: GEN.

#include <vector>

#include "mask/gen/hier_internal.hpp"

namespace hefesto::mask::gen_detail {

namespace {

// TiGetType of the corner-stitch neighbour at the start of `side`'s range
// [lo, hi): a split neighbour gives its left type.
bool solid_neighbour(const db::Plane& plane, const db::TileView& t, db::Side side, std::int64_t lo, std::int64_t hi) {
  std::vector<db::TileView> found;
  plane.neighbors(t, side, static_cast<db::Coord>(lo), static_cast<db::Coord>(hi), found);
  if (found.empty()) {
    return false;
  }
  const db::TileView& n = found.front();
  return (n.split ? n.left_type : n.type) != tech::kSpace;
}

struct SliverShape {
  bool up = false;    // BL or TR non-space
  bool side = false;  // LB or RT non-space
};

SliverShape shape(const db::Plane& plane, const db::TileView& t) {
  const db::Rect& a = t.area;
  SliverShape s;
  s.up = solid_neighbour(plane, t, db::Side::left, a.y0, std::int64_t{a.y0} + 1) ||    // BL
         solid_neighbour(plane, t, db::Side::right, std::int64_t{a.y1} - 1, a.y1);     // TR
  s.side = solid_neighbour(plane, t, db::Side::bottom, a.x0, std::int64_t{a.x0} + 1) ||  // LB
           solid_neighbour(plane, t, db::Side::top, std::int64_t{a.x1} - 1, a.x1);       // RT
  return s;
}

// Grows [lo, hi] to `m`: Magic's (m - d) / 2 on each side; OFF puts the
// odd unit on the high side.
void widen(db::Coord& lo, db::Coord& hi, std::int64_t m, bool magic_compat) {
  const std::int64_t d = std::int64_t{hi} - lo;
  const std::int64_t half = (m - d) / 2;
  lo = clamp_coord(lo - half);
  hi = clamp_coord(hi + (magic_compat ? half : m - d - half));
}

}  // namespace

db::Rect grow_sliver(const db::Plane& plane, const db::TileView& t, std::int32_t min_width, bool magic_compat) {
  db::Rect area = t.area;
  const SliverShape s = shape(plane, t);
  if (min_width == 0) {
    return area;
  }
  const std::int64_t m = min_width;
  const std::int64_t h = area.height();
  const std::int64_t w = area.width();
  if (h < w || s.up) {
    if (h < m) {
      widen(area.y0, area.y1, m, magic_compat);
    } else if (magic_compat) {
      return area;  // the stop skips the second branch
    }
  }
  if ((w < h || s.side) && w < m) {
    widen(area.x0, area.x1, m, magic_compat);
  }
  return area;
}

}  // namespace hefesto::mask::gen_detail
