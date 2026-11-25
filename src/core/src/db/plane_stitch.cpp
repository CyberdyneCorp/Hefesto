// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::StitchEditor: split, join and stitch access on a scratch plane
// (openspec/changes/m7-extraction design D6). The primitives are
// detail::TilePlane's ports of TiSplitX and TiJoinY (tile_plane.cpp); this
// file only checks their preconditions, so a bad call cannot corrupt the
// stitches.

#include "hefesto/db/plane_stitch.hpp"

#include "db/tile_plane.hpp"

namespace hefesto::db {

namespace {

detail::Tile* tile_of(StitchTile t) noexcept {
  // The plane's tiles are mutable records owned by the edited plane.
  return const_cast<detail::Tile*>(static_cast<const detail::Tile*>(t.tile));  // NOLINT
}

StitchTile handle_of(const detail::Tile* t) noexcept {
  return StitchTile{t};
}

}  // namespace

StitchEditor::StitchEditor(Plane& scratch) noexcept : plane_(&scratch) {}

StitchEditor::~StitchEditor() {
  release();
}

StitchTile StitchEditor::hint() const noexcept {
  return handle_of(plane_->impl_->tiles.hint());
}

void StitchEditor::set_hint(StitchTile t) noexcept {
  if (t && !plane_->impl_->tiles.is_outside(tile_of(t))) {
    plane_->impl_->tiles.set_hint(tile_of(t));
  }
}

StitchTile StitchEditor::tile_at(Point p, StitchTile near) const noexcept {
  if (!in_usable_plane(Rect{p.x, p.y, p.x + 1, p.y + 1})) {
    return {};
  }
  detail::TilePlane& tiles = plane_->impl_->tiles;
  detail::Tile* start = near && !tiles.is_outside(tile_of(near)) ? tile_of(near) : tiles.hint();
  return handle_of(detail::goto_point(start, p));
}

StitchTile StitchEditor::stitch(StitchTile t, Stitch s) const noexcept {
  if (!t || plane_->impl_->tiles.is_infinity(tile_of(t))) {
    return {};
  }
  const detail::Tile* tile = tile_of(t);
  const detail::Tile* next = nullptr;
  switch (s) {
    case Stitch::lb:
      next = tile->lb;
      break;
    case Stitch::bl:
      next = tile->bl;
      break;
    case Stitch::tr:
      next = tile->tr;
      break;
    case Stitch::rt:
      next = tile->rt;
      break;
  }
  return next == nullptr || plane_->impl_->tiles.is_infinity(next) ? StitchTile{} : handle_of(next);
}

Rect StitchEditor::area(StitchTile t) const noexcept {
  if (!t || plane_->impl_->tiles.is_infinity(tile_of(t))) {
    return Rect{};
  }
  return detail::tile_rect(tile_of(t));
}

tech::TileType StitchEditor::type(StitchTile t) const noexcept {
  return t ? static_cast<tech::TileType>(tile_of(t)->body & detail::kLeftMask) : tech::kSpace;
}

bool StitchEditor::boundary(StitchTile t) const noexcept {
  return t && plane_->impl_->tiles.is_boundary(tile_of(t));
}

void StitchEditor::set_type(StitchTile t, tech::TileType type) noexcept {
  if (t && !plane_->impl_->tiles.is_outside(tile_of(t))) {
    tile_of(t)->body = static_cast<detail::Body>(type) & detail::kLeftMask;
  }
}

std::uintptr_t StitchEditor::client(StitchTile t) const noexcept {
  return t ? tile_of(t)->client : kClientDefault;
}

void StitchEditor::set_client(StitchTile t, std::uintptr_t value) noexcept {
  if (t) {
    tile_of(t)->client = value;
  }
}

StitchTile StitchEditor::split_x(StitchTile t, Coord x) {
  return split_x(t, x, false);
}

StitchTile StitchEditor::split_x(StitchTile t, Coord x, bool magic_compat) {
  detail::TilePlane& tiles = plane_->impl_->tiles;
  if (!t || tiles.is_outside(tile_of(t))) {
    return {};
  }
  detail::Tile* tile = tile_of(t);
  // A split tile keeps its diagonal body over whatever rectangle it is
  // left with, so cutting it moves the triangle: Magic's TiSplitX does
  // exactly that, so it is copied with magic-compat ON and refused with it
  // OFF (Q-EXTRACT-29).
  if ((detail::is_split(tile->body) && !magic_compat) || x <= detail::left(tile) || x >= detail::right(tile)) {
    return {};
  }
  return handle_of(tiles.split_x(tile, x));
}

bool StitchEditor::join_y(StitchTile keep, StitchTile gone) {
  return join_y(keep, gone, false);
}

bool StitchEditor::join_y(StitchTile keep, StitchTile gone, bool magic_compat) {
  detail::TilePlane& tiles = plane_->impl_->tiles;
  if (!keep || !gone || keep == gone || tiles.is_outside(tile_of(keep)) || tiles.is_outside(tile_of(gone))) {
    return false;
  }
  detail::Tile* k = tile_of(keep);
  detail::Tile* g = tile_of(gone);
  if (!magic_compat && (detail::is_split(k->body) || detail::is_split(g->body))) {
    return false;
  }
  const bool stacked = k->rt == g || g->rt == k;
  if (!stacked || detail::left(k) != detail::left(g) || detail::right(k) != detail::right(g)) {
    return false;
  }
  tiles.join_y(k, g);
  return true;
}

void StitchEditor::release() noexcept {
  plane_->impl_->tiles.end_operation();
}

void StitchEditor::neighbors(StitchTile t, Side side, std::vector<StitchTile>& out) const {
  out.clear();
  if (!t || plane_->impl_->tiles.is_outside(tile_of(t))) {
    return;
  }
  const TileView v = view(t);
  const bool vertical = side == Side::left || side == Side::right;
  std::vector<TileView> views;
  plane_->neighbors(v, side, vertical ? v.area.y0 : v.area.x0, vertical ? v.area.y1 : v.area.x1, views);
  out.reserve(views.size());
  for (const TileView& n : views) {
    out.push_back(handle(n));
  }
}

TileView StitchEditor::view(StitchTile t) const {
  if (!t || plane_->impl_->tiles.is_outside(tile_of(t))) {
    return TileView{};
  }
  const detail::Tile* tile = tile_of(t);
  return detail::view_of(tile, detail::is_split(tile->body) ? detail::kDiagonalBit : 0);
}

}  // namespace hefesto::db
