// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask planes (capability mask-output-operators "Geometric model of mask
// planes"; openspec/changes/m5-mask-gds-cif/design.md D6): a db::Plane
// painted with Magic's CIF paint and erase tables, and the scratch pool.
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) cif/CIFgen.c (CIFPaintTable,
// CIFEraseTable, cifPaintFunc, cifClipPlane), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: OPS.

#include "hefesto/mask/plane/plane.hpp"

#include <algorithm>
#include <utility>

namespace hefesto::mask {

namespace {

db::PaintRow filled_row(tech::TileType type) {
  db::PaintRow row{};
  row.fill(type);
  return row;
}

const tech::TypeMask& solid_mask() {
  static const tech::TypeMask mask = tech::TypeMask::of(kMaskSolid);
  return mask;
}

/// The paintable part of a mask plane: the usable plane (TiPlaneRect) less
/// one unit on each side. db::Plane::paint merges split tiles over the area
/// grown by 1, and that merge must not reach the boundary tiles; the
/// one-unit rim stays space, which no operator result reaches.
constexpr db::Rect kMaskUniverse = db::expand(db::whole_plane(), -1);

db::Rect clip_to_plane(const db::Rect& area) noexcept {
  const db::Rect& u = kMaskUniverse;
  return db::Rect{std::max(area.x0, u.x0), std::max(area.y0, u.y0), std::min(area.x1, u.x1), std::min(area.y1, u.y1)};
}

diag::Status paint_rect(db::Plane& plane, const db::Rect& area, const db::PaintRow& row) {
  const db::Rect clipped = clip_to_plane(area);
  if (clipped.is_null()) {
    return diag::Done{};
  }
  return plane.paint(clipped, row);
}

diag::Status paint_tri(db::Plane& plane, const db::Rect& area, db::Diagonal diagonal, bool right_side,
                       const db::PaintRow& row, bool magic_compat) {
  if (area.is_null() || !db::surround(kMaskUniverse, area)) {
    // A triangle cannot be clipped to the plane without changing its
    // slope; one beyond the plane contributes nothing (as for any shape
    // the database rejects).
    return diag::Done{};
  }
  return plane.paint_triangle(area, diagonal, right_side, row, magic_compat);
}

/// Paints or erases every solid tile of `from` into `into`.
diag::Status copy_tiles(db::Plane& into, const db::Plane& from, const db::PaintRow& row) {
  std::vector<db::TileView> tiles;
  from.enumerate(db::whole_plane(), solid_mask(), [&](const db::TileView& t) {
    tiles.push_back(t);
    return true;
  });
  for (const db::TileView& t : tiles) {
    const diag::Status status =
        t.split ? paint_tri(into, t.area, t.diagonal, t.right_side, row, true) : paint_rect(into, t.area, row);
    if (!status.ok()) {
      return status;
    }
  }
  return diag::Done{};
}

}  // namespace

const db::PaintRow& cif_paint_table() noexcept {
  static const db::PaintRow row = filled_row(kMaskSolid);
  return row;
}

const db::PaintRow& cif_erase_table() noexcept {
  static const db::PaintRow row = filled_row(tech::kSpace);
  return row;
}

diag::Status MaskPlane::paint(const db::Rect& area) { return paint_rect(plane_, area, cif_paint_table()); }

diag::Status MaskPlane::erase(const db::Rect& area) { return paint_rect(plane_, area, cif_erase_table()); }

diag::Status MaskPlane::paint_triangle(const db::Rect& area, db::Diagonal diagonal, bool right_side,
                                       bool magic_compat) {
  return paint_tri(plane_, area, diagonal, right_side, cif_paint_table(), magic_compat);
}

diag::Status MaskPlane::erase_triangle(const db::Rect& area, db::Diagonal diagonal, bool right_side,
                                       bool magic_compat) {
  return paint_tri(plane_, area, diagonal, right_side, cif_erase_table(), magic_compat);
}

diag::Status MaskPlane::subtract(const MaskPlane& other) {
  if (&other == this) {
    clear();
    return diag::Done{};
  }
  return copy_tiles(plane_, other.plane_, cif_erase_table());
}

diag::Status MaskPlane::unite(const MaskPlane& other) {
  if (&other == this) {
    return diag::Done{};
  }
  return copy_tiles(plane_, other.plane_, cif_paint_table());
}

void MaskPlane::clip(const db::Rect& clip) {
  // cifClipPlane: the four sides in Magic's order (right, top, left, bottom).
  const db::Rect all = db::whole_plane();
  if (clip.x1 < all.x1) {
    (void)erase(db::Rect{clip.x1, all.y0, all.x1, all.y1});
  }
  if (clip.y1 < all.y1) {
    (void)erase(db::Rect{all.x0, clip.y1, all.x1, all.y1});
  }
  if (clip.x0 > all.x0) {
    (void)erase(db::Rect{all.x0, all.y0, clip.x0, all.y1});
  }
  if (clip.y0 > all.y0) {
    (void)erase(db::Rect{all.x0, all.y0, all.x1, clip.y0});
  }
}

void MaskPlane::clear() { plane_ = db::Plane{}; }

void MaskPlane::copy_from(const MaskPlane& other) {
  if (&other != this) {
    plane_ = other.plane_.clone();
  }
}

void MaskPlane::swap(MaskPlane& other) noexcept { std::swap(plane_, other.plane_); }

bool MaskPlane::empty() const { return plane_.empty(); }

std::vector<db::TileView> MaskPlane::solid_tiles() const {
  std::vector<db::TileView> tiles;
  plane_.enumerate(db::whole_plane(), solid_mask(), [&](const db::TileView& t) {
    tiles.push_back(t);
    return true;
  });
  return tiles;
}

std::vector<db::Rect> MaskPlane::solid_rects() const {
  std::vector<db::Rect> rects;
  for (const db::TileView& t : solid_tiles()) {
    rects.push_back(t.area);
  }
  return rects;
}

double MaskPlane::solid_area() const {
  double total = 0.0;
  for (const db::TileView& t : solid_tiles()) {
    const double a = static_cast<double>(t.area.width()) * static_cast<double>(t.area.height());
    total += t.split ? a / 2.0 : a;
  }
  return total;
}

MaskPlane& PlanePool::acquire() {
  if (!free_.empty()) {
    MaskPlane* plane = free_.back();
    free_.pop_back();
    plane->clear();
    return *plane;
  }
  planes_.push_back(std::make_unique<MaskPlane>());
  return *planes_.back();
}

void PlanePool::release(MaskPlane& plane) {
  if (std::find(free_.begin(), free_.end(), &plane) == free_.end()) {
    free_.push_back(&plane);
  }
}

}  // namespace hefesto::mask
