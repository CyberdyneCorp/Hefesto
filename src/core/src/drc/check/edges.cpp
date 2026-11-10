// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The edge walks of drcTile and the per-pair rule loop (drc-rule-semantics
// "Basic check scope", "Trigger rules", "Wide-spacing trigger (maximal
// rectangle search)", "Maxwidth rules", "Exceptions and exemptions at
// check time"; openspec/changes/m3-drc design.md D6).
//
// The rule loop is a state machine shared by both orientations: the
// `triggered` count and the maximal-rectangle flag survive from one rule
// to the next exactly as in Magic, so a trigger followed by a rule skipped
// for an exception or the edge plane hands its records to the rule after
// it, as Magic does.
//
// (Magic ref: drc/DRCbasic.c drcTile (lines 845-1307 vertical, 1309-1700
// horizontal); drc/DRCextend.c drcCanonicalMaxwidth)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <string_view>

#include "hefesto/db/cell.hpp"
#include "hefesto/drc/check/maxrect.hpp"
#include "hefesto/hints/hints.hpp"
#include "kernel.hpp"

namespace hefesto::drc::check_detail {

void Kernel::vertical_edges(const TileView& tile) {
  const Coord top = std::min(tile.area.y1, check_.y1);
  const Coord bottom = std::max(tile.area.y0, check_.y0);
  left_neighbors(*edge_plane_, tile, bottom, top, left_);
  bool first = true;
  for (const TileView& outer : left_) {
    const TileType b = left_type(tile);
    const TileType a = right_type(outer);
    if (a == b) {
      continue;
    }
    const Coord edge_top = std::min(outer.area.y1, top);
    const Coord edge_bottom = std::max(outer.area.y0, bottom);
    if (edge_top <= edge_bottom) {
      continue;
    }
    evaluate_edge(Edge{true, tile.area.x0, edge_bottom, edge_top, &tile, &outer, first}, rules_.rules(a, b));
    count(&CheckStatistics::edges);
    first = false;
  }
}

void Kernel::horizontal_edges(const TileView& tile) {
  const Coord left = std::max(tile.area.x0, check_.x0);
  const Coord right = std::min(tile.area.x1, check_.x1);
  bottom_neighbors(*edge_plane_, tile, left, right, below_);
  bool first = true;
  for (const TileView& outer : below_) {
    const TileType b = bottom_type(tile);
    const TileType a = top_type(outer);
    if (a == b) {
      continue;
    }
    const Coord edge_left = std::max(outer.area.x0, left);
    const Coord edge_right = std::min(outer.area.x1, right);
    if (edge_left >= edge_right) {
      continue;
    }
    evaluate_edge(Edge{false, tile.area.y0, edge_left, edge_right, &tile, &outer, first}, rules_.rules(a, b));
    count(&CheckStatistics::edges);
    first = false;
  }
}

void Kernel::evaluate_edge(const Edge& edge, std::span<const Rule> rules) {
  RuleState state;
  for (std::size_t i = 0; i < rules.size(); ++i) {
    if (!evaluate_rule(edge, rules[i], state)) {
      ++i;  // cptr = cptr->drcc_next: skip the triggered rule
    }
  }
}

bool Kernel::evaluate_rule(const Edge& edge, const Rule& rule, RuleState& state) {
  if (!exception_allows(edge, rule)) {
    return true;
  }
  // DRC_ANGLES_90 and DRC_SPLITTILE rules belong to the split-tile pass.
  if ((rule.flags & (flags::kAngles90 | flags::kSplitTile)) != 0) {
    return true;
  }
  const bool trigger = rule.has(flags::kTrigger);
  if (plane_ != rule.edge_plane) {
    return !trigger;
  }
  count(&CheckStatistics::rules);
  if (state.triggered == 0) {
    state.from_maxrects = false;
  }
  if (edge.vertical && rule.has(flags::kArea)) {
    if (edge.first) {
      check_area(*edge.tile, rule);
    }
    return true;
  }
  if (rule.has(flags::kMaxWidth | flags::kBends)) {
    return maxwidth_bends(edge, rule, trigger, state);
  }
  if (edge.vertical) {
    if (evaluate_vertical_nonstandard(edge, rule)) {
      return true;
    }
  } else if ((rule.flags & (flags::kArea | flags::kRectSize | flags::kMaxWidth)) != 0) {
    return !trigger;  // only checked on left edges
  }
  if (rule.has(flags::kOffGrid)) {
    check_off_grid(edge.vertical ? Rect{edge.at, edge.lo, edge.at, edge.hi} : Rect{edge.lo, edge.at, edge.hi, edge.at},
                   rule);
    return true;
  }
  return standard_rule(edge, rule, trigger, state);
}

bool Kernel::evaluate_vertical_nonstandard(const Edge& edge, const Rule& rule) {
  if (rule.has(flags::kMaxWidth)) {
    if (edge.first) {
      check_maxwidth(*edge.tile, rule, rule.has(flags::kMaxWidthBoth));
    }
    return true;
  }
  if (!rule.has(flags::kRectSize)) {
    return false;
  }
  // Only the lower-left tile of a rectangle: the types left of its bottom
  // (BL) and below its left end (LB) must not be in the rule's set.
  if (edge.first) {
    const TileView& tile = *edge.tile;
    const TileView bl = tile_at(*edge_plane_, tile.area.x0 - 1, tile.area.y0);
    const TileView lb = tile_at(*edge_plane_, tile.area.x0, tile.area.y0 - 1);
    if (!rule.ok.test(right_type(bl)) && !rule.ok.test(top_type(lb))) {
      check_rect_size(tile, rule);
    }
  }
  return true;
}

bool Kernel::maxwidth_bends(const Edge& edge, const Rule& rule, bool trigger, RuleState& state) {
  const bool widen = !trigger || rules_.options().wide_width_noninclusive;
  const std::int32_t limit = rule.dist + (widen ? 1 : 0);
  int dir = 0;
  const TileView* start = nullptr;
  if (rule.reverse()) {
    dir = edge.vertical ? direction::kWest : direction::kSouth;
    start = edge.outer;
  } else {
    dir = edge.vertical ? direction::kEast : direction::kNorth;
    start = edge.tile;
  }
  const bool found = canonical_maxwidth(*start, dir, limit, rule);
  state.triggered = 0;
  if (trigger) {
    return maxwidth_trigger(edge, rule, found, state);
  }
  if (found) {
    for (Rect& r : maxrects_) {
      geo_clip(r, clip_);
      if (!r.is_null()) {
        report(r, rule);
      }
    }
  }
  return true;
}

bool Kernel::maxwidth_trigger(const Edge& edge, const Rule& rule, bool found, RuleState& state) const {
  if (!found) {
    return false;
  }
  const int entries = static_cast<int>(maxrects_.size());
  if (rule.cdist <= rule.dist) {
    state.triggered = entries;
    state.from_maxrects = true;
    return true;
  }
  // Run-length rule: the segment and some candidate must exceed it.
  if (edge.hi - edge.lo < rule.cdist) {
    return false;
  }
  const bool long_enough = std::any_of(maxrects_.begin(), maxrects_.end(), [&](const Rect& r) {
    return (edge.vertical ? r.height() : r.width()) > rule.cdist;
  });
  if (!long_enough) {
    return false;
  }
  state.triggered = entries;
  state.from_maxrects = true;
  return true;
}

bool Kernel::exception_allows(const Edge& edge, const Rule& rule) const {
  if (rule.exception == kNoException) {
    return true;
  }
  const bool exemption = (rule.exception & kExemptionBit) != 0;
  const auto index = static_cast<std::size_t>(rule.exception & 0x7fU);
  const auto names = rules_.exception_names();
  bool inside = false;
  if (index < names.size()) {
    std::string_view name = names[index];
    if (const auto stripped = hints::hint_name(name)) {
      name = *stripped;  // tolerate a name stored with its MASKHINTS_ prefix
    }
    if (const hints::HintRegion* region = hints::find_hint(cell_, name)) {
      const hints::Segment segment{edge.vertical, edge.at, edge.lo, edge.hi};
      inside = hints::inside(*region, segment, input_.compat.magic_compat);
    }
  }
  return exemption ? !inside : inside;
}

bool Kernel::canonical_maxwidth(const TileView& start, int dir, std::int32_t limit, const Rule& rule) {
  const Rect s = start.area;
  const std::int32_t side = limit - 1;
  Rect bounds;
  switch (dir) {
    case direction::kNorth:
      bounds = Rect{s.x0 - side, s.y0, s.x1 + side, s.y0 + limit};
      break;
    case direction::kSouth:
      bounds = Rect{s.x0 - side, s.y1 - limit, s.x1 + side, s.y1};
      break;
    case direction::kEast:
      bounds = Rect{s.x0, s.y0 - side, s.x0 + limit, s.y1 + side};
      break;
    default:  // kWest
      bounds = Rect{s.x1 - limit, s.y0 - side, s.x1, s.y1 + side};
      break;
  }
  const db::Plane* plane = plane_of(rule.edge_plane);
  if (plane == nullptr) {
    maxrects_.clear();
    return false;
  }
  maxrects_ = maximal_rectangles(*plane, bounds, ~rule.ok, limit);
  if (maxrects_.empty()) {
    return false;
  }
  // "exclude" types: a candidate touching one on the check plane is dropped
  // by collapsing it to a point (it stays in the list, as in Magic).
  const db::Plane* exclude = rule.check_plane != rule.edge_plane ? plane_of(rule.check_plane) : nullptr;
  if (exclude == nullptr) {
    return true;
  }
  for (Rect& r : maxrects_) {
    const bool covered = !exclude->enumerate(r, rule.corner, [](const TileView&) { return false; });
    if (covered) {
      r.x1 = r.x0;
      r.y1 = r.y0;
    }
  }
  return true;
}

}  // namespace hefesto::drc::check_detail
