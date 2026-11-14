// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::Plane: the public face of the corner-stitched tile plane
// (layout-geometry-model). The algorithms live in tile_plane.cpp,
// paint_plane.cpp, nm_split.cpp and nm_paint.cpp (ported from Magic).

#include "hefesto/db/plane.hpp"

#include <algorithm>
#include <string>

#include "db/tile_plane.hpp"
#include "hefesto/db/diag_codes.hpp"

namespace hefesto::db {

struct Plane::Impl {
  detail::TilePlane tiles;
};

namespace {

diag::Status check_area(const Rect& area) {
  if (area.is_null() || in_usable_plane(area)) {
    return diag::Done{};
  }
  return diag::make(codes::kGeometryOutsidePlane,
                    "rectangle (" + std::to_string(area.x0) + "," + std::to_string(area.y0) + ")-(" +
                        std::to_string(area.x1) + "," + std::to_string(area.y1) +
                        ") lies outside the usable plane (-1073741818..1073741818)");
}

detail::Body exact_of(Diagonal diagonal, bool right_side) noexcept {
  return detail::kDiagonalBit | (diagonal == Diagonal::nw_se ? detail::kDirectionBit : 0U) |
         (right_side ? detail::kSideBit : 0U);
}

TileView view_of(const detail::Tile* t, detail::Body dinfo) {
  TileView v;
  v.area = detail::tile_rect(t);
  v.tile = t;
  if (!detail::is_split(t->body)) {
    v.type = static_cast<tech::TileType>(t->body);
    return v;
  }
  v.split = true;
  v.diagonal = detail::direction(t->body) != 0 ? Diagonal::nw_se : Diagonal::ne_sw;
  v.right_side = (dinfo & detail::kSideBit) != 0;
  v.left_type = static_cast<tech::TileType>(detail::left_type(t->body));
  v.right_type = static_cast<tech::TileType>(detail::right_type(t->body));
  v.type = v.right_side ? v.right_type : v.left_type;
  return v;
}

void merge_after_paint(detail::TilePlane& tiles, const Rect& area) {
  if (!(area == whole_plane())) {
    // One unit around the area, but never into the boundary tiles.
    const Rect around = expand(area, 1);
    const Rect plane = whole_plane();
    detail::merge_nm_tiles(tiles, Rect{std::max(around.x0, plane.x0), std::max(around.y0, plane.y0),
                                       std::min(around.x1, plane.x1), std::min(around.y1, plane.y1)});
  }
  tiles.end_operation();
}

}  // namespace

Plane::Plane() : impl_(std::make_unique<Impl>()) {}
Plane::~Plane() = default;
Plane::Plane(Plane&&) noexcept = default;
Plane& Plane::operator=(Plane&&) noexcept = default;

Plane Plane::clone() const {
  Plane copy;
  copy.impl_->tiles.copy_from(impl_->tiles);
  return copy;
}

diag::Status Plane::paint(const Rect& area, const PaintRow& row) {
  if (auto status = check_area(area); !status.ok() || area.is_null()) {
    return status;
  }
  detail::paint_plane(impl_->tiles, area, row);
  merge_after_paint(impl_->tiles, area);
  return diag::Done{};
}

diag::Status Plane::paint_triangle(const Rect& area, Diagonal diagonal, bool right_side, const PaintRow& row,
                                   bool magic_compat, bool* off_grid) {
  if (auto status = check_area(area); !status.ok() || area.is_null()) {
    return status;
  }
  const bool filled = detail::nm_paint_plane(impl_->tiles, exact_of(diagonal, right_side), area, row, magic_compat);
  if (off_grid != nullptr) {
    *off_grid = filled;
  }
  merge_after_paint(impl_->tiles, area);
  return diag::Done{};
}

void Plane::record_splits(std::vector<SplitEdit>* log) noexcept {
  impl_->tiles.set_split_log(log);
}

namespace {

/// The search area clamped to the plane: the boundary tiles have no
/// neighbours beyond them, so an area out to +-kInfinity must stop there.
Rect clamp_to_plane(const Rect& area) noexcept {
  const Rect plane = whole_plane();
  return Rect{std::max(area.x0, plane.x0), std::max(area.y0, plane.y0), std::min(area.x1, plane.x1),
              std::min(area.y1, plane.y1)};
}

}  // namespace

bool Plane::enumerate(const Rect& unclamped, const tech::TypeMask& types, TileVisitorRef visit) const {
  const Rect area = clamp_to_plane(unclamped);
  if (area.x0 > area.x1 || area.y0 > area.y1) {
    return true;
  }
  return !detail::search_area(impl_->tiles, area, types,
                              [&](detail::Tile* t, detail::Body dinfo) { return !visit(view_of(t, dinfo)); });
}

bool Plane::enumerate_triangle(const Rect& unclamped, Diagonal diagonal, bool right_side, const tech::TypeMask& types,
                               TileVisitorRef visit) const {
  const Rect area = clamp_to_plane(unclamped);
  if (area.x0 > area.x1 || area.y0 > area.y1) {
    return true;
  }
  return !detail::search_nm_area(impl_->tiles, exact_of(diagonal, right_side), area, types,
                                 [&](detail::Tile* t, detail::Body dinfo) { return !visit(view_of(t, dinfo)); });
}

namespace {

detail::Tile* start_tile(const TileView* near, const detail::TilePlane& tiles) noexcept {
  // The plane's tiles are mutable stitch records; the walks only read them.
  return near != nullptr && near->tile != nullptr
             ? const_cast<detail::Tile*>(static_cast<const detail::Tile*>(near->tile))  // NOLINT
             : tiles.hint();
}

/// How enumerate() over `rect` reports `t` first; false when it skips it
/// (a split tile with `rect` beyond its diagonal on both sides).
bool first_view(detail::Tile* t, const Rect& rect, TileView& out) {
  detail::Body dinfo = 0;
  if (detail::is_split(t->body) && !detail::first_split_side(t, rect, dinfo)) {
    return false;
  }
  out = view_of(t, dinfo);
  return true;
}

/// The one-unit strip beyond side `side` of `r` over [lo, hi).
Rect side_strip(const Rect& r, Side side, Coord lo, Coord hi) noexcept {
  switch (side) {
    case Side::left:
      return Rect{r.x0 - 1, lo, r.x0, hi};
    case Side::right:
      return Rect{r.x1, lo, r.x1 + 1, hi};
    case Side::bottom:
      return Rect{lo, r.y0 - 1, hi, r.y0};
    case Side::top:
      break;
  }
  return Rect{lo, r.y1, hi, r.y1 + 1};
}

bool vertical_side(Side side) noexcept { return side == Side::left || side == Side::right; }

/// One side's stitch walk: where it starts (the tile beyond the side at
/// one end of the strip), the stitch to the next tile, whether that tile
/// still borders the strip, and whether it runs against the requested
/// order (increasing y or x).
struct SideWalk {
  Point start;
  detail::Tile* (*next)(const detail::Tile*) noexcept;
  bool (*within)(const detail::Tile*, const Rect&) noexcept;
  bool backwards;
};

SideWalk side_walk(Side side, const Rect& strip) noexcept {
  switch (side) {
    case Side::left:  // BL, then RT upwards
      return {Point{strip.x0, strip.y0}, [](const detail::Tile* t) noexcept { return t->rt; },
              [](const detail::Tile* t, const Rect& s) noexcept { return detail::bottom(t) < s.y1; }, false};
    case Side::right:  // TR, then LB downwards
      return {Point{strip.x0, strip.y1 - 1}, [](const detail::Tile* t) noexcept { return t->lb; },
              [](const detail::Tile* t, const Rect& s) noexcept { return detail::top(t) > s.y0; }, true};
    case Side::bottom:  // LB, then TR rightwards
      return {Point{strip.x0, strip.y0}, [](const detail::Tile* t) noexcept { return t->tr; },
              [](const detail::Tile* t, const Rect& s) noexcept { return detail::left(t) < s.x1; }, false};
    case Side::top:
      break;
  }
  // RT, then BL leftwards
  return {Point{strip.x1 - 1, strip.y0}, [](const detail::Tile* t) noexcept { return t->bl; },
          [](const detail::Tile* t, const Rect& s) noexcept { return detail::right(t) > s.x0; }, true};
}

void walk_side(detail::Tile* tile, Side side, const Rect& strip, std::vector<TileView>& out) {
  const SideWalk walk = side_walk(side, strip);
  TileView v;
  for (detail::Tile* t = detail::goto_point(tile, walk.start); walk.within(t, strip); t = walk.next(t)) {
    if (first_view(t, strip, v)) {
      out.push_back(v);
    }
  }
  if (walk.backwards) {
    std::reverse(out.begin(), out.end());
  }
}

}  // namespace

TileView Plane::tile_at(Point p, const TileView* near) const {
  const Rect square{p.x, p.y, p.x + 1, p.y + 1};
  TileView found;
  if (in_usable_plane(square)) {
    detail::Tile* t = detail::goto_point(start_tile(near, impl_->tiles), p);
    if (!detail::is_split(t->body)) {
      return view_of(t, 0);
    }
    if (first_view(t, square, found)) {
      return found;
    }
  }
  enumerate(square, tech::TypeMask::all(), [&](const TileView& t) {
    found = t;
    return false;
  });
  return found;
}

void Plane::neighbors(const TileView& t, Side side, Coord lo, Coord hi, std::vector<TileView>& out) const {
  out.clear();
  if (lo >= hi) {
    return;
  }
  const Rect strip = side_strip(t.area, side, lo, hi);
  const bool within_side = vertical_side(side) ? t.area.y0 <= lo && hi <= t.area.y1 : t.area.x0 <= lo && hi <= t.area.x1;
  if (t.tile != nullptr && within_side && in_usable_plane(strip)) {
    walk_side(start_tile(&t, impl_->tiles), side, strip, out);
    return;
  }
  // The same tiles by an area search of the strip (no stitch start).
  enumerate(strip, tech::TypeMask::all(), [&](const TileView& n) {
    if (out.empty() || !(out.back().area == n.area)) {
      out.push_back(n);
    }
    return true;
  });
  const auto key = [&](const TileView& n) { return vertical_side(side) ? n.area.y0 : n.area.x0; };
  std::sort(out.begin(), out.end(), [&](const TileView& a, const TileView& b) { return key(a) < key(b); });
  out.erase(std::unique(out.begin(), out.end(), [](const TileView& a, const TileView& b) { return a.area == b.area; }),
            out.end());
}

tech::TileType Plane::type_at(Point p) const {
  const detail::Tile* t = detail::goto_point(impl_->tiles.hint(), p);
  if (!detail::is_split(t->body)) {
    return static_cast<tech::TileType>(t->body);
  }
  // Twice the center of the unit square, compared against the diagonal.
  const Rect r = detail::tile_rect(t);
  const std::int64_t cx = 2 * (std::int64_t{p.x} - r.x0) + 1;
  const std::int64_t cy = 2 * (std::int64_t{p.y} - r.y0) + 1;
  const std::int64_t w = r.width();
  const std::int64_t h = r.height();
  const bool nw_se = detail::direction(t->body) != 0;
  // Left part: above `/` (cy*w > cx*h) or below `\` (cy*w < (2w-cx)*h).
  const bool on_left = nw_se ? cy * w < (2 * w - cx) * h : cy * w > cx * h;
  return static_cast<tech::TileType>(on_left ? detail::left_type(t->body) : detail::right_type(t->body));
}

Rect Plane::bounding_box() const {
  Rect box;
  impl_->tiles.for_each_tile([&](const detail::Tile* t) {
    if (detail::left_type(t->body) != tech::kSpace || detail::right_type(t->body) != tech::kSpace) {
      box = include(box, detail::tile_rect(t));
    }
  });
  return box;
}

bool Plane::empty() const {
  return bounding_box().is_null();
}

std::size_t Plane::tile_count() const {
  std::size_t n = 0;
  impl_->tiles.for_each_tile([&](const detail::Tile*) { ++n; });
  return n;
}

}  // namespace hefesto::db
