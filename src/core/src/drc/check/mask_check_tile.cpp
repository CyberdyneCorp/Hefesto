// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-tile mask-layer checks (capability drc-cif-rules "Mask-layer
// check procedure", "Mask area rule", "Mask maxwidth rule"): drcCifTile's
// left and bottom edge walks, areaCifCheck with its Euclidean corner
// filter, and the area and maxwidth floods.
//
// Replicated as Magic does them:
//   - the right half of a split tile skips the left edge and goes straight
//     to the bottom edge, so an area or maxwidth rule is then run there as
//     an edge rule;
//   - the Euclidean corner bits are cleared once per tile, not per edge,
//     and the corner radius is the rule's corner distance modulo 4096
//     (dCD_radial packs it into 12 bits).
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) drc/DRCcif.c (drcCifTile,
// areaCifCheck, drcCheckCifArea, drcCheckCifMaxwidth), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DRCCIF (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <algorithm>

#include "hefesto/drc/check/euclidean.hpp"
#include "mask_check_internal.hpp"

namespace hefesto::drc::mask_detail {

namespace {

using check_detail::body_type;
using check_detail::bottom_type;
using check_detail::left_type;
using check_detail::right_type;
using check_detail::top_type;

constexpr std::int32_t kRadialMask = 0xfff;

Rect clipped(const Rect& r, const Rect& area) noexcept {
  return Rect{std::max(r.x0, area.x0), std::max(r.y0, area.y0), std::min(r.x1, area.x1), std::min(r.y1, area.y1)};
}

bool touches_edge_of(const Rect& tile, const Rect& k) noexcept {
  return tile.x1 == k.x1 || tile.x0 == k.x0 || tile.y0 == k.y0 || tile.y1 == k.y1;
}

/// SplitDirection: 1 for `\`.
bool split_direction(const TileView& t) noexcept { return t.split && t.diagonal == db::Diagonal::nw_se; }

/// sqx >= 0, sqy >= 0 and sqx^2 + sqy^2 >= s^2 (64-bit).
bool corner_clear(std::int64_t sqx, std::int64_t sqy, std::int64_t s) noexcept {
  return sqx >= 0 && sqy >= 0 && sqx * sqx + sqy * sqy >= s * s;
}

/// drcCifPointToSegment from (px, py) to the segment (ax, ay)-(bx, by),
/// compared with the squared radius.
bool beyond_segment(std::int64_t px, std::int64_t py, Coord ax, Coord ay, Coord bx, Coord by, std::int64_t s) noexcept {
  return point_to_segment_squared(clamp_coord(px), clamp_coord(py), ax, ay, bx, by) > s * s;
}

}  // namespace

MaskChecker::MaskChecker(const CheckInput& input, const mask::LayerPlanes& planes, std::int32_t scale,
                         const Rect& cif_rect, const Rect& clip, ErrorSink& sink)
    : input_(input), planes_(planes), scale_(scale), rect_(cif_rect), clip_(clip), sink_(sink) {}

CheckOutcome MaskChecker::run(const MaskRules& rules) {
  for (std::size_t layer = 0; layer < rules.layers.size(); ++layer) {
    for (const MaskList which : {MaskList::solid_tiles, MaskList::space_tiles}) {
      for (const MaskRule& r : rules.list(layer, which)) {
        if (!run_rule(layer, which, r)) {
          return CheckOutcome::cancelled;
        }
      }
    }
  }
  return CheckOutcome::completed;
}

bool MaskChecker::run_rule(std::size_t layer, MaskList which, const MaskRule& r) {
  const mask::MaskPlane* plane = planes_[layer];
  if (plane == nullptr) {
    return true;
  }
  current_ = &r;
  layer_ = layer;
  plane_ = &plane->plane();
  const tech::TypeMask types = tech::TypeMask::of(which == MaskList::solid_tiles ? mask::kMaskSolid : tech::kSpace);
  plane_->enumerate(rect_, types, [this](const TileView& tile) { return visit(tile); });
  return !cancelled_;
}

bool MaskChecker::visit(const TileView& tile) {
  if (input_.cancel.requested()) {
    cancelled_ = true;
    return false;
  }
  count(&CheckStatistics::tiles);
  count(&CheckStatistics::cif_tiles);
  marks_ = 0;
  if (!(tile.split && tile.right_side)) {
    if (rule().has(flags::kArea)) {
      check_area(tile);
      return true;
    }
    if (rule().has(flags::kMaxWidth)) {
      check_maxwidth(tile);
      return true;
    }
    if (tile.area.x0 >= rect_.x0) {
      vertical_edges(tile);
    }
  }
  if (tile.split && tile.right_side == split_direction(tile)) {
    return true;  // the bottom of this half is the diagonal
  }
  if (tile.area.y0 >= rect_.y0) {
    horizontal_edges(tile);
  }
  return true;
}

// --- Edge walks ------------------------------------------------------------

void MaskChecker::vertical_edges(const TileView& tile) {
  const Coord top = std::min(tile.area.y1, rect_.y1);
  const Coord bottom = std::max(tile.area.y0, rect_.y0);
  check_detail::left_neighbors(*plane_, tile, bottom, top, outer_);
  for (const TileView& outer : outer_) {  // the searches below never touch outer_
    if (right_type(outer) == left_type(tile)) {
      continue;  // not a real edge
    }
    Rect region{0, std::max(outer.area.y0, bottom), 0, std::min(outer.area.y1, top)};
    if (region.y1 <= region.y0) {
      continue;
    }
    const bool needed =
        rule().reverse() ? vertical_reverse(tile, outer, region) : vertical_forward(tile, region);
    if (needed) {
      search(region);
      count(&CheckStatistics::edges);
    }
  }
}

void MaskChecker::horizontal_edges(const TileView& tile) {
  const Coord left = std::max(tile.area.x0, rect_.x0);
  const Coord right = std::min(tile.area.x1, rect_.x1);
  check_detail::bottom_neighbors(*plane_, tile, left, right, outer_);
  for (const TileView& outer : outer_) {
    if (top_type(outer) == bottom_type(tile)) {
      continue;
    }
    Rect region{std::max(outer.area.x0, left), 0, std::min(outer.area.x1, right), 0};
    if (region.x0 >= region.x1) {
      continue;
    }
    count(&CheckStatistics::rules);
    const bool needed =
        rule().reverse() ? horizontal_reverse(tile, outer, region) : horizontal_forward(tile, region);
    if (needed) {
      search(region);
      count(&CheckStatistics::edges);
    }
  }
}

TileView MaskChecker::probe(std::int64_t x, std::int64_t y, const TileView& near) const {
  return plane_->tile_at(db::Point{clamp_coord(x), clamp_coord(y)}, &near);
}

void MaskChecker::extend(tech::TileType type, Coord& side, std::int64_t delta, unsigned mark) {
  if (!rule().corner.test(type)) {
    return;
  }
  side = clamp_coord(std::int64_t{side} + delta);
  if (input_.euclidean) {
    marks_ |= mark;
  }
}

// Left edge, right to left: the corners (3) below and (4) above the edge on
// the tile's side.
bool MaskChecker::vertical_reverse(const TileView& tile, const TileView& outer, Rect& region) {
  const Rule& r = rule();
  const Coord x = tile.area.x0;
  extend(top_type(probe(x, std::int64_t{region.y0} - 1, tile)), region.y0, -std::int64_t{r.cdist},
         corner_mark::kSW);
  if (r.has(flags::kBothCorners)) {
    extend(bottom_type(probe(x, region.y1, tile)), region.y1, r.cdist, corner_mark::kNW);
  }
  region.x0 = clamp_coord(std::int64_t{x} - r.dist);
  if (outer.area.x0 <= region.x0 && outer.area.y0 <= region.y0 && outer.area.y1 >= region.y1 &&
      layer_ == current_->check_layer && r.ok.test(body_type(outer))) {
    return false;
  }
  region.x1 = x;
  return true;
}

// Left edge, left to right: the corners (1) above and (2) below the edge on
// the outer side.
bool MaskChecker::vertical_forward(const TileView& tile, Rect& region) {
  const Rule& r = rule();
  const Coord x = tile.area.x0;
  extend(bottom_type(probe(std::int64_t{x} - 1, region.y1, tile)), region.y1, r.cdist, corner_mark::kNE);
  if (r.has(flags::kBothCorners)) {
    extend(top_type(probe(std::int64_t{x} - 1, std::int64_t{region.y0} - 1, tile)), region.y0,
           -std::int64_t{r.cdist}, corner_mark::kSE);
  }
  region.x1 = clamp_coord(std::int64_t{x} + r.dist);
  if (tile.area.x1 >= region.x1 && tile.area.y0 <= region.y0 && tile.area.y1 >= region.y1 &&
      layer_ == current_->check_layer && r.ok.test(left_type(tile))) {
    return false;
  }
  region.x0 = x;
  return true;
}

// Bottom edge, top to bottom: the corners (3) right and (4) left of the
// edge on the tile's side.
bool MaskChecker::horizontal_reverse(const TileView& tile, const TileView& outer, Rect& region) {
  const Rule& r = rule();
  const Coord y = tile.area.y0;
  extend(left_type(probe(region.x1, y, tile)), region.x1, r.cdist, corner_mark::kSE);
  if (r.has(flags::kBothCorners)) {
    extend(right_type(probe(std::int64_t{region.x0} - 1, y, tile)), region.x0, -std::int64_t{r.cdist},
           corner_mark::kSW);
  }
  region.y0 = clamp_coord(std::int64_t{y} - r.dist);
  if (outer.area.y0 <= region.y0 && outer.area.x0 <= region.x0 && outer.area.x1 >= region.x1 &&
      layer_ == current_->check_layer && r.ok.test(top_type(outer))) {
    return false;
  }
  region.y1 = y;
  return true;
}

// Bottom edge, bottom to top: the corners (1) left and (2) right of the
// edge on the outer side.
bool MaskChecker::horizontal_forward(const TileView& tile, Rect& region) {
  const Rule& r = rule();
  const Coord y = tile.area.y0;
  extend(right_type(probe(std::int64_t{region.x0} - 1, std::int64_t{y} - 1, tile)), region.x0,
         -std::int64_t{r.cdist}, corner_mark::kNW);
  if (r.has(flags::kBothCorners)) {
    extend(left_type(probe(region.x1, std::int64_t{y} - 1, tile)), region.x1, r.cdist, corner_mark::kNE);
  }
  region.y1 = clamp_coord(std::int64_t{y} + r.dist);
  if (tile.area.y1 >= region.y1 && tile.area.x0 <= region.x0 && tile.area.x1 >= region.x1 &&
      layer_ == current_->check_layer && r.ok.test(body_type(tile))) {
    return false;
  }
  region.y0 = y;
  return true;
}

// --- areaCifCheck ------------------------------------------------------------

void MaskChecker::search(const Rect& region) {
  if (marks_ != 0) {
    sdist_ = rule().cdist & kRadialMask;
  }
  count(&CheckStatistics::slow);
  const mask::MaskPlane* target = planes_[current_->check_layer];
  if (target == nullptr) {
    return;
  }
  target->plane().enumerate(region, ~rule().ok, [&](const TileView& found) {
    candidate(found, region);
    return true;
  });
}

void MaskChecker::candidate(const TileView& found, const Rect& region) {
  if (rule().ok.test(body_type(found))) {
    return;
  }
  const Rect area = clipped(found.area, region);
  if (area.is_null()) {
    return;
  }
  const std::optional<Rect> mapped = map_mask_error(area, scale_, MaskErrorKind::edge, input_.compat);
  if (!mapped) {
    return;
  }
  const Rect error = clipped(*mapped, clip_);
  if (error.is_null() || (marks_ != 0 && radial_exempt(found, area, region))) {
    return;
  }
  sink_.report(error, rule());
}

// The corner tests of areaCifCheck: a candidate beyond the radius from the
// marked corner (or beyond the diagonal of a split tile) is not an error.
bool MaskChecker::radial_exempt(const TileView& found, const Rect& area, const Rect& region) const {
  const std::int64_t s = sdist_;
  const Rect& t = found.area;
  const bool direction = split_direction(found);
  const bool split = found.split;
  const bool right = found.right_side;
  if ((marks_ & corner_mark::kNW) != 0 &&
      (corner_clear(std::int64_t{region.x0} + s - area.x1, std::int64_t{area.y0} - region.y1 + s, s) ||
       (split && !direction && !right &&
        beyond_segment(std::int64_t{region.x0} + s, std::int64_t{region.y1} - s, t.x0, t.y0, t.x1, t.y1, s)))) {
    return true;
  }
  if ((marks_ & corner_mark::kNE) != 0 &&
      (corner_clear(std::int64_t{area.x0} - region.x1 + s, std::int64_t{area.y0} - region.y1 + s, s) ||
       (split && direction && right &&
        beyond_segment(std::int64_t{region.x1} - s, std::int64_t{region.y1} - s, t.x0, t.y1, t.x1, t.y0, s)))) {
    return true;
  }
  if ((marks_ & corner_mark::kSW) != 0 &&
      (corner_clear(std::int64_t{region.x0} + s - area.x1, std::int64_t{region.y0} + s - area.y1, s) ||
       (split && direction && !right &&
        beyond_segment(std::int64_t{region.x0} + s, std::int64_t{region.y0} + s, t.x0, t.y1, t.x1, t.y0, s)))) {
    return true;
  }
  return (marks_ & corner_mark::kSE) != 0 &&
         (corner_clear(std::int64_t{area.x0} - region.x1 + s, std::int64_t{region.y0} + s - area.y1, s) ||
          (split && !direction && right &&
           beyond_segment(std::int64_t{region.x1} - s, std::int64_t{region.y0} + s, t.x0, t.y0, t.x1, t.y1, s)));
}

// --- Area and maxwidth ---------------------------------------------------------

template <typename Visit>
bool MaskChecker::flood(const TileView& start, Visit&& visit_tile) {
  flood_stack_.clear();
  flood_seen_.clear();
  flood_stack_.push_back(start);
  flood_seen_.insert(start.area);
  const tech::TypeMask& ok = rule().ok;
  while (!flood_stack_.empty()) {
    const TileView tile = flood_stack_.back();
    flood_stack_.pop_back();
    if (!visit_tile(tile)) {
      return false;
    }
    const Rect& r = tile.area;
    for (const db::Side side : {db::Side::top, db::Side::left, db::Side::bottom, db::Side::right}) {
      const bool vertical = side == db::Side::left || side == db::Side::right;
      plane_->neighbors(tile, side, vertical ? r.y0 : r.x0, vertical ? r.y1 : r.x1, neighbors_);
      for (const TileView& n : neighbors_) {
        if (ok.test(body_type(n)) && flood_seen_.insert(n.area)) {  // TiGetType, as Magic tests it
          flood_stack_.push_back(n);
        }
      }
    }
  }
  return true;
}

void MaskChecker::check_area(const TileView& start) {
  const std::int64_t limit = rule().cdist;
  std::int64_t area = 0;
  const bool complete = flood(start, [&](const TileView& t) {
    area += t.area.width() * t.area.height();
    return !touches_edge_of(t.area, rect_) && area < limit;
  });
  if (complete && area < limit) {
    report_region(start.area);
  }
}

void MaskChecker::check_maxwidth(const TileView& start) {
  const std::int64_t limit = rule().dist;
  const Rect& s = start.area;
  if (rule().has(flags::kMaxWidthBoth)) {
    if (s.width() > limit || s.height() > limit) {
      report_region(s);
    }
    return;
  }
  if (rule().has(flags::kBends)) {
    if (!start.split && s.width() > limit && s.height() > limit) {
      report_region(s);  // split tiles are not checked (Magic's limitation)
    }
    return;
  }
  Rect bound = s;
  (void)flood(start, [&](const TileView& t) {
    bound = db::include(bound, t.area);
    return !(bound.width() > limit && bound.height() > limit);
  });
  if (bound.width() > limit && bound.height() > limit) {
    report_region(s);
  }
}

void MaskChecker::report_region(const Rect& tile_area) {
  if (collapses_when_truncated(tile_area, scale_)) {
    // Counted when the outward-rounded error would be reported (DRC-MASK-*).
    const std::optional<Rect> outward = map_mask_error(tile_area, scale_, MaskErrorKind::region, {false, false});
    if (outward && !clipped(*outward, clip_).is_null()) {
      ++collapsed_;
    }
  }
  const std::optional<Rect> mapped = map_mask_error(tile_area, scale_, MaskErrorKind::region, input_.compat);
  if (!mapped) {
    return;
  }
  const Rect error = clipped(*mapped, clip_);
  if (!error.is_null()) {
    sink_.report(error, rule());
  }
}

}  // namespace hefesto::drc::mask_detail
