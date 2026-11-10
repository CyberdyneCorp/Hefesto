// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::Plane: the public face of the corner-stitched tile plane
// (layout-geometry-model). The algorithms live in tile_plane.cpp,
// paint_plane.cpp, nm_split.cpp and nm_paint.cpp (ported from Magic).

#include "hefesto/db/plane.hpp"

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
    detail::merge_nm_tiles(tiles, expand(area, 1));
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
                                   bool magic_compat) {
  if (auto status = check_area(area); !status.ok() || area.is_null()) {
    return status;
  }
  detail::nm_paint_plane(impl_->tiles, exact_of(diagonal, right_side), area, row, magic_compat);
  merge_after_paint(impl_->tiles, area);
  return diag::Done{};
}

void Plane::record_splits(std::vector<SplitEdit>* log) noexcept {
  impl_->tiles.set_split_log(log);
}

bool Plane::enumerate(const Rect& area, const tech::TypeMask& types, const TileVisitor& visit) const {
  if (area.x0 > area.x1 || area.y0 > area.y1) {
    return true;
  }
  return !detail::search_area(impl_->tiles, area, types,
                              [&](detail::Tile* t, detail::Body dinfo) { return !visit(view_of(t, dinfo)); });
}

bool Plane::enumerate_triangle(const Rect& area, Diagonal diagonal, bool right_side, const tech::TypeMask& types,
                               const TileVisitor& visit) const {
  if (area.x0 > area.x1 || area.y0 > area.y1) {
    return true;
  }
  return !detail::search_nm_area(impl_->tiles, exact_of(diagonal, right_side), area, types,
                                 [&](detail::Tile* t, detail::Body dinfo) { return !visit(view_of(t, dinfo)); });
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
