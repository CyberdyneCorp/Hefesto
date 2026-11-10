// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Nonstandard rules of the basic check (drc-rule-semantics "Maxwidth
// rules", "Area rule", "Rectangle size rule", "Off-grid rule", "Angle
// rules"; openspec/changes/m3-drc design.md D6).
//
//   - Area: a flood over the 4-connected tiles whose facing side is in the
//     rule's set, summing full tile areas; it gives up (reports nothing)
//     when the sum reaches the limit or a visited tile has an edge on an
//     edge of K (Q-DRC-8, replicate). Started only on a tile's first
//     examined left-edge segment (Q-DRC-21, replicate).
//   - Maxwidth bend_illegal / both: the same flood accumulating the
//     bounding box; exclude types are ignored in both modes (Q-DRC-19).
//   - Rectangle size: the TR / RT stitch walks of drcCheckRectSize.
//   - Off-grid: the degenerate edge rectangle snapped outward to the
//     pitch; truncation toward zero with magic-compat ON, floor with it
//     OFF (Q-DRC-9).
//   - Angles: drcCheckAngles on split tiles.
//
// The order of a flood never changes its result (both stop conditions are
// monotonic), so the stack discipline need not match Magic's.
//
// (Magic ref: drc/DRCextend.c drcCheckAngles, drcCheckOffGrid,
// drcCheckArea, drcCheckMaxwidth, drcCheckRectSize)
//
// Owner: CHECK (openspec/changes/m3-drc/design.md D2).

#include <cstdlib>

#include "kernel.hpp"

namespace hefesto::drc::check_detail {

namespace {

bool touches_edge_of(const Rect& tile, const Rect& k) noexcept {
  return tile.x1 == k.x1 || tile.x0 == k.x0 || tile.y0 == k.y0 || tile.y1 == k.y1;
}

/// v / p * p truncating toward zero (C) or toward minus infinity.
Coord snap(Coord v, Coord p, bool truncate) noexcept {
  Coord q = v / p;
  if (!truncate && v % p != 0 && v < 0) {
    --q;
  }
  return q * p;
}

}  // namespace

template <typename Visit>
bool Kernel::flood(const TileView& start, const tech::TypeMask& ok, Visit&& visit) {
  flood_stack_.clear();
  flood_seen_.clear();
  flood_stack_.push_back(start);
  flood_seen_.insert(start.area);
  while (!flood_stack_.empty()) {
    const TileView tile = flood_stack_.back();
    flood_stack_.pop_back();
    if (!visit(tile)) {
      return false;
    }
    push_neighbors(tile, ok);
  }
  return true;
}

void Kernel::push_neighbors(const TileView& tile, const tech::TypeMask& ok) {
  const Rect& r = tile.area;
  const struct {
    Rect strip;
    TileType (*facing)(const TileView&) noexcept;
  } sides[] = {
      {Rect{r.x0, r.y1, r.x1, r.y1 + 1}, &bottom_type},  // top
      {Rect{r.x0 - 1, r.y0, r.x0, r.y1}, &right_type},   // left
      {Rect{r.x0, r.y0 - 1, r.x1, r.y0}, &top_type},     // bottom
      {Rect{r.x1, r.y0, r.x1 + 1, r.y1}, &left_type},    // right
  };
  for (const auto& side : sides) {
    tiles_in(*edge_plane_, side.strip, neighbors_);
    for (const TileView& n : neighbors_) {
      if (ok.test(side.facing(n)) && flood_seen_.insert(n.area).second) {
        flood_stack_.push_back(n);
      }
    }
  }
}

void Kernel::check_area(const TileView& start, const Rule& rule) {
  const std::int64_t limit = rule.cdist;
  std::int64_t area = 0;
  const bool complete = flood(start, rule.ok, [&](const TileView& t) {
    area += t.area.width() * t.area.height();
    return !touches_edge_of(t.area, check_) && area < limit;
  });
  if (!complete || area >= limit) {
    return;
  }
  Rect error = start.area;
  geo_clip(error, clip_);
  if (!error.is_null()) {
    report(error, rule);
  }
}

void Kernel::check_maxwidth(const TileView& start, const Rule& rule, bool both) {
  const std::int64_t limit = rule.dist;
  Rect bound = start.area;
  flood(start, rule.ok, [&](const TileView& t) {
    bound = db::include(bound, t.area);
    return !(bound.width() > limit && bound.height() > limit);
  });
  const bool wide = bound.width() > limit;
  const bool high = bound.height() > limit;
  if (!((wide && high) || (both && (wide || high)))) {
    return;
  }
  Rect error = start.area;
  geo_clip(error, clip_);
  if (!error.is_null()) {
    report(error, rule);
  }
}

void Kernel::check_rect_size(const TileView& start, const Rule& rule) {
  const std::int32_t max_size = rule.dist;
  const std::int32_t parity = rule.cdist;  // -1 any, 0 even, 1 odd
  TileView t = start;
  while (rule.ok.test(body_type(t)) && t.area.x1 < db::kPlaneMax) {
    t = tile_at(*edge_plane_, t.area.x1, t.area.y1 - 1);  // TR
  }
  const Coord width = t.area.x0 - start.area.x0;
  t = start;
  while (rule.ok.test(body_type(t)) && t.area.y1 < db::kPlaneMax) {
    t = tile_at(*edge_plane_, t.area.x1 - 1, t.area.y1);  // RT
  }
  const Coord height = t.area.y0 - start.area.y0;
  Coord error_width = width;
  Coord error_height = height;
  bool error = true;
  if (width > max_size) {
    error_width = width - max_size;
  } else if (height > max_size) {
    error_height = height - max_size;
  } else if (parity >= 0 && std::abs(width % 2) != parity) {
    error_width = 1;
  } else if (parity >= 0 && std::abs(height % 2) != parity) {
    error_height = 1;
  } else {
    error = false;
  }
  if (!error) {
    return;
  }
  Rect area{start.area.x0, start.area.y0, start.area.x0 + error_width, start.area.y0 + error_height};
  geo_clip(area, clip_);
  if (!area.is_null()) {
    report(area, rule);
  }
}

void Kernel::check_off_grid(Rect edge, const Rule& rule) {
  const Coord pitch = rule.dist;
  if (pitch <= 1) {
    return;  // no error by definition
  }
  const bool truncate = input_.compat.magic_compat;
  geo_clip(edge, clip_);
  edge.x0 = std::min(edge.x0, snap(edge.x0, pitch, truncate));
  edge.x1 = std::max(edge.x1, snap(edge.x1, pitch, truncate));
  edge.y0 = std::min(edge.y0, snap(edge.y0, pitch, truncate));
  edge.y1 = std::max(edge.y1, snap(edge.y1, pitch, truncate));
  if (!edge.is_null()) {
    report(edge, rule);
  }
}

void Kernel::check_angles(const TileView& tile, const Rule& rule) {
  const bool orthogonal_only = !rule.has(flags::kAngles45);
  if (!orthogonal_only && tile.area.width() == tile.area.height()) {
    return;
  }
  Rect area = tile.area;
  geo_clip(area, clip_);
  if (!area.is_null()) {
    report(area, rule);
  }
}

}  // namespace hefesto::drc::check_detail
