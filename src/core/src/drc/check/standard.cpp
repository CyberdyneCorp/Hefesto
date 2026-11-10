// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Standard edge rules (drc-rule-semantics "Standard edge rule check
// region", "Trigger rules", "Euclidean corner filter"; openspec/changes/
// m3-drc design.md D6): the check region E with its corner extensions,
// the candidate search on the check plane and Magic's areaCheck filters
// (run length, Euclidean corners, trigger records, 45-degree split tiles).
//
// Corner tiles follow Magic's stitch walks literally: each corner probe is
// the edge's own tile unless the (sub)range reaches the tile's end, in
// which case it is the tile across that end (see kernel.hpp for the stitch
// mapping). Vertical edges apply the edge-continuation test before
// extending a corner; horizontal edges do not, in either magic-compat mode
// (Q-DRC-6, replicate). For run-length rules the corner distance c is 0 on
// horizontal edges, and on vertical edges it is the run length with
// magic-compat ON and 0 with it OFF (Q-DRC-7). The Euclidean radius is
// c mod 4096 with magic-compat ON and c with it OFF (Q-DRC-20).
//
// (Magic ref: drc/DRCbasic.c drcTile (lines 1104-1303, 1539-1695),
// areaCheck)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include <algorithm>

#include "kernel.hpp"

namespace hefesto::drc::check_detail {

namespace {

/// The edge continues past the probed end when the types on both sides
/// stay those of the edge (drcTile: "Make sure the edge stops at ...").
bool edge_ends(const TileView& left_probe, const TileView& right_probe, const TileView& outer,
               const TileView& tile) noexcept {
  return right_type(left_probe) != right_type(outer) || left_type(right_probe) != left_type(tile);
}

std::int32_t outside_extra(const Rule& rule) noexcept { return rule.has(flags::kOutside) ? 1 : 0; }
std::int32_t outside_offset(const Rule& rule) noexcept { return rule.has(flags::kOutside) ? rule.dist : 0; }

}  // namespace

std::int32_t Kernel::corner_distance(const Edge& edge, const Rule& rule) const noexcept {
  if (!rule.has(flags::kRunLength)) {
    return rule.cdist;
  }
  return edge.vertical && input_.compat.magic_compat ? rule.cdist : 0;
}

bool Kernel::standard_rule(const Edge& edge, const Rule& rule, bool trigger, RuleState& state) {
  const std::int32_t c = corner_distance(edge, rule);
  Search search;
  search.rule = &rule;
  entries_ = 0;
  do {
    along_range(edge, c, state, search);
    const bool run =
        edge.vertical ? vertical_region(edge, rule, c, search) : horizontal_region(edge, rule, c, search);
    if (!run) {
      continue;  // the tile itself proves the region clean
    }
    if (search.marks != 0) {
      search.sdist = input_.compat.magic_compat ? (c & 0xfff) : c;
    }
    count(&CheckStatistics::slow);
    entries_ = 0;
    search_region(search);
  } while (state.triggered > 0);
  if (entries_ == 0) {
    return !trigger;  // a trigger that found nothing skips its rule
  }
  state.triggered = entries_;
  return true;
}

void Kernel::along_range(const Edge& edge, std::int32_t c, RuleState& state, Search& search) {
  Coord lo = edge.lo;
  Coord hi = edge.hi;
  if (state.triggered > 0) {
    const auto index = static_cast<std::size_t>(--state.triggered);
    const std::vector<Rect>& source = state.from_maxrects ? maxrects_ : rlist_;
    if (index < source.size()) {
      const Rect& t = source[index];
      hi = std::min((edge.vertical ? t.y1 : t.x1) + c, edge.hi);
      lo = std::max((edge.vertical ? t.y0 : t.x0) - c, edge.lo);
    }
  }
  if (edge.vertical) {
    search.region.y0 = lo;
    search.region.y1 = hi;
  } else {
    search.region.x0 = lo;
    search.region.x1 = hi;
  }
}

void Kernel::mark(const Rule& rule, unsigned corner, Search& search) const noexcept {
  if (input_.euclidean && !rule.has(flags::kManhattan)) {
    search.marks |= corner;
  }
}

bool Kernel::vertical_region(const Edge& edge, const Rule& rule, std::int32_t c, Search& search) {
  return rule.reverse() ? vertical_reverse(edge, rule, c, search) : vertical_forward(edge, rule, c, search);
}

bool Kernel::horizontal_region(const Edge& edge, const Rule& rule, std::int32_t c, Search& search) {
  return rule.reverse() ? horizontal_reverse(edge, rule, c, search) : horizontal_forward(edge, rule, c, search);
}

TileView Kernel::top_probe(const Edge& edge, bool left_side, Coord limit) const {
  const TileView& t = left_side ? *edge.outer : *edge.tile;
  return t.area.y1 <= limit ? tile_at(*edge_plane_, left_side ? edge.at - 1 : edge.at, t.area.y1) : t;
}

TileView Kernel::bottom_probe(const Edge& edge, bool left_side, Coord limit) const {
  const TileView& t = left_side ? *edge.outer : *edge.tile;
  return t.area.y0 >= limit ? tile_at(*edge_plane_, left_side ? edge.at - 1 : edge.at, t.area.y0 - 1) : t;
}

void Kernel::extend_top(const Edge& edge, const Rule& rule, std::int32_t c, bool test_left, unsigned corner,
                        Search& search) const {
  Rect& e = search.region;
  // Point (1) above-left of the top end or (4) above-right is tested; the
  // other one checks that the edge does not continue.
  const TileView tested = top_probe(edge, test_left, e.y1);
  if (!rule.corner.test(bottom_type(tested))) {
    return;
  }
  const TileView other = top_probe(edge, !test_left, e.y1);
  if (edge_ends(test_left ? tested : other, test_left ? other : tested, *edge.outer, *edge.tile)) {
    e.y1 += c;
    mark(rule, corner, search);
  }
}

void Kernel::extend_bottom(const Edge& edge, const Rule& rule, std::int32_t c, bool test_left, unsigned corner,
                           Search& search) const {
  Rect& e = search.region;
  // Point (2) below-left of the bottom end or (3) below-right is tested.
  const TileView tested = bottom_probe(edge, test_left, e.y0);
  if (!rule.corner.test(top_type(tested))) {
    return;
  }
  const TileView other = bottom_probe(edge, !test_left, e.y0);
  if (edge_ends(test_left ? tested : other, test_left ? other : tested, *edge.outer, *edge.tile)) {
    e.y0 -= c;
    mark(rule, corner, search);
  }
}

bool Kernel::vertical_forward(const Edge& edge, const Rule& rule, std::int32_t c, Search& search) {
  const TileView& tile = *edge.tile;
  Rect& e = search.region;
  // Top corner tests point (1), then the bottom corner point (2).
  extend_top(edge, rule, c, true, radial::kNE, search);
  if (rule.has(flags::kBothCorners)) {
    extend_bottom(edge, rule, c, true, radial::kSE, search);
  }
  e.x1 = edge.at + rule.dist + outside_extra(rule);
  if (tile.area.x1 >= e.x1 && tile.area.y0 <= e.y0 && tile.area.y1 >= e.y1 && plane_ == rule.check_plane &&
      rule.ok.test(left_type(tile))) {
    return false;
  }
  e.x0 = edge.at + outside_offset(rule);
  search.initial = edge.outer;
  return true;
}

bool Kernel::vertical_reverse(const Edge& edge, const Rule& rule, std::int32_t c, Search& search) {
  const TileView& outer = *edge.outer;
  Rect& e = search.region;
  // Bottom corner tests point (3), then the top corner point (4).
  extend_bottom(edge, rule, c, false, radial::kSW, search);
  if (rule.has(flags::kBothCorners)) {
    extend_top(edge, rule, c, false, radial::kNW, search);
  }
  e.x0 = edge.at - rule.dist - outside_extra(rule);
  if (outer.area.x0 <= e.x0 && outer.area.y0 <= e.y0 && outer.area.y1 >= e.y1 && plane_ == rule.check_plane &&
      rule.ok.test(right_type(outer))) {
    return false;
  }
  e.x1 = edge.at - outside_offset(rule);
  search.initial = edge.tile;
  return true;
}

bool Kernel::horizontal_forward(const Edge& edge, const Rule& rule, std::int32_t c, Search& search) {
  const TileView& tile = *edge.tile;
  const TileView& outer = *edge.outer;
  const Coord y = edge.at;
  Rect& e = search.region;
  // Left end: point (1) below-left of pointL (no continuation test, Q-DRC-6).
  const TileView left = outer.area.x0 >= e.x0 ? tile_at(*edge_plane_, outer.area.x0 - 1, y - 1) : outer;
  if (rule.corner.test(top_type(left))) {
    e.x0 -= c;
    mark(rule, radial::kNW, search);
  }
  if (rule.has(flags::kBothCorners)) {
    // Right end: point (2) below-right of pointR.
    const TileView right = outer.area.x1 <= e.x1 ? tile_at(*edge_plane_, outer.area.x1, outer.area.y1 - 1) : outer;
    if (rule.corner.test(top_type(right))) {
      e.x1 += c;
      mark(rule, radial::kNE, search);
    }
  }
  e.y1 = y + rule.dist + outside_extra(rule);
  if (tile.area.y1 >= e.y1 && tile.area.x0 <= e.x0 && tile.area.x1 >= e.x1 && plane_ == rule.check_plane &&
      rule.ok.test(bottom_type(tile))) {
    return false;
  }
  e.y0 = y + outside_offset(rule);
  search.initial = &outer;
  return true;
}

bool Kernel::horizontal_reverse(const Edge& edge, const Rule& rule, std::int32_t c, Search& search) {
  const TileView& tile = *edge.tile;
  const TileView& outer = *edge.outer;
  const Coord y = edge.at;
  Rect& e = search.region;
  // Right end: point (3) above-right of pointR.
  const TileView right = tile.area.x1 <= e.x1 ? tile_at(*edge_plane_, tile.area.x1, y) : tile;
  if (rule.corner.test(bottom_type(right))) {
    e.x1 += c;
    mark(rule, radial::kSE, search);
  }
  if (rule.has(flags::kBothCorners)) {
    // Left end: point (4) above-left of pointL.
    const TileView left = tile.area.x0 >= e.x0 ? tile_at(*edge_plane_, tile.area.x0 - 1, tile.area.y0) : tile;
    if (rule.corner.test(bottom_type(left))) {
      e.x0 -= c;
      mark(rule, radial::kSW, search);
    }
  }
  e.y0 = y - rule.dist - outside_extra(rule);
  if (outer.area.y0 <= e.y0 && outer.area.x0 <= e.x0 && outer.area.x1 >= e.x1 && plane_ == rule.check_plane &&
      rule.ok.test(top_type(outer))) {
    return false;
  }
  e.y1 = y - outside_offset(rule);
  search.initial = &tile;
  return true;
}

void Kernel::search_region(const Search& search) {
  const Rule& rule = *search.rule;
  const db::Plane* plane = plane_of(rule.check_plane);
  if (plane == nullptr) {
    return;
  }
  tech::TypeMask wrong = ~rule.ok;
  wrong.clear_type(tech::kErrorS);  // the subcell-error type is never searched
  plane->enumerate(search.region, wrong, [&](const TileView& found) {
    candidate(found, search);
    return true;
  });
}

void Kernel::candidate(const TileView& found, const Search& search) {
  const Rule& rule = *search.rule;
  const bool trigger = rule.has(flags::kTrigger);
  Rect area = found.area;
  geo_clip(area, trigger ? check_ : clip_);
  geo_clip(area, search.region);
  if (area.is_null()) {
    return;
  }
  // Run-length rules ignore candidates shorter than the run length both ways.
  if (rule.has(flags::kRunLength) && area.width() < rule.cdist && area.height() < rule.cdist) {
    return;
  }
  if (search.marks != 0 && radial_exempt(found, area, search)) {
    return;
  }
  if (trigger) {
    record_trigger(area);
    return;
  }
  // "angles 45-only" rejects errors inside split tiles.
  if (!found.split || (rule.flags & (flags::kAngles45 | flags::kAngles90)) != flags::kAngles45) {
    report(area, rule);
  }
}

void Kernel::record_trigger(const Rect& area) {
  const auto index = static_cast<std::size_t>(entries_);
  if (index < rlist_.size()) {
    rlist_[index] = area;
  } else {
    rlist_.push_back(area);
  }
  ++entries_;
}

}  // namespace hefesto::drc::check_detail
