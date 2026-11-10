// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The maximal-rectangle search (drc-rule-semantics "Wide-spacing trigger
// (maximal rectangle search)", "Maxwidth rules"; openspec/changes/m3-drc
// design.md D6): every tile of a wrong type overlapping the bounds, in
// area-enumeration order, replaces each candidate it overlaps by the parts
// above, below, left and right of it that are at least `max_distance`
// thick; the search stops as soon as no candidate is left. Sides of tiles
// that reach the plane boundary never cut. Candidates may overlap and are
// kept in Magic's list order. Split tiles are treated as their bounding
// rectangles (Magic's FindMaxRects does not handle non-Manhattan tiles).
//
// (Magic ref: utils/maxrect.c FindMaxRects; drc/DRCextend.c
// drcCanonicalMaxwidth)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/check/maxrect.hpp"

namespace hefesto::drc {

namespace {

/// FindMaxRects for one candidate `sr` and one wrong-type tile `area`.
void cut(const db::Rect& sr, const db::Rect& area, std::int32_t max_distance, std::vector<db::Rect>& out) {
  if (!db::overlap(sr, area)) {
    out.push_back(sr);
    return;
  }
  constexpr db::Coord kHigh = db::kInfinity - 2;
  constexpr db::Coord kLow = db::kMinusInfinity + 2;
  if (area.y1 < kHigh && sr.y1 >= area.y1 + max_distance) {
    out.push_back(db::Rect{sr.x0, area.y1, sr.x1, sr.y1});  // top
  }
  if (area.y0 > kLow && sr.y0 <= area.y0 - max_distance) {
    out.push_back(db::Rect{sr.x0, sr.y0, sr.x1, area.y0});  // bottom
  }
  if (area.x0 > kLow && sr.x0 <= area.x0 - max_distance) {
    out.push_back(db::Rect{sr.x0, sr.y0, area.x0, sr.y1});  // left
  }
  if (area.x1 < kHigh && sr.x1 >= area.x1 + max_distance) {
    out.push_back(db::Rect{area.x1, sr.y0, sr.x1, sr.y1});  // right
  }
}

}  // namespace

std::vector<db::Rect> maximal_rectangles(const db::Plane& plane, const db::Rect& bounds,
                                         const tech::TypeMask& wrong_types, std::int32_t max_distance) {
  std::vector<db::Rect> candidates{bounds};
  std::vector<db::Rect> next;
  plane.enumerate(bounds, wrong_types, [&](const db::TileView& tile) {
    next.clear();
    for (const db::Rect& sr : candidates) {
      cut(sr, tile.area, max_distance, next);
    }
    candidates.swap(next);
    return !candidates.empty();  // no maximal rectangle left: stop
  });
  return candidates;
}

}  // namespace hefesto::drc
