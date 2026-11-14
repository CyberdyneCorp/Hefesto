// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask planes (capability mask-output-operators "Geometric model of mask
// planes"; openspec/changes/m5-mask-gds-cif/design.md D4, D6): a mask plane
// is a db::Plane painted with Magic's CIF paint and erase tables
// (CIFPaintTable, CIFEraseTable: one mask type on space), so painting order
// and tile structure follow Magic. Planes come from a PlanePool that is
// reused across generations, so no plane is allocated per square.
//
// Coordinates are mask units. Every paint and erase is clipped to the usable
// plane (db::whole_plane(), Magic's TiPlaneRect) less a one-unit rim, which
// stays space: a rectangle reaching "infinity" stops there, as Magic's
// operators keep infinite sides unchanged, and db::Plane's split-tile merge
// after each paint (over the area grown by 1) never reaches the plane's
// boundary tiles.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); OPS fills in
// the implementation in src/core/src/mask/plane/ (design D2). The CIF paint
// table moved here from hints/region (the hints API is unchanged).

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::mask {

/// A closed polygon, vertices in order (the last joins the first); used by
/// the readers (ReadSink) and the writers' merged-polygon output.
using Polygon = std::vector<db::Point>;

/// The mask type painted into a mask plane (CIF_SOLIDTYPE); space is 0.
inline constexpr tech::TileType kMaskSolid = 1;

/// CIFPaintTable: any type painted over anything gives kMaskSolid.
[[nodiscard]] const db::PaintRow& cif_paint_table() noexcept;
/// CIFEraseTable: erasing anything gives space.
[[nodiscard]] const db::PaintRow& cif_erase_table() noexcept;

class MaskPlane {
 public:
  MaskPlane() = default;

  /// Unions `area` into the plane (DBPaintPlane with CIFPaintTable). An
  /// area without extent changes nothing.
  [[nodiscard]] diag::Status paint(const db::Rect& area);
  /// Removes `area` from the plane (DBPaintPlane with CIFEraseTable).
  [[nodiscard]] diag::Status erase(const db::Rect& area);
  /// Unions the triangle of `area` on side `right_side` of `diagonal`
  /// (DBNMPaintPlane with CIFPaintTable). `magic_compat` false rounds both
  /// diagonals half up (Q-MODEL-3).
  [[nodiscard]] diag::Status paint_triangle(const db::Rect& area, db::Diagonal diagonal, bool right_side,
                                            bool magic_compat = true);
  /// Removes that triangle (DBNMPaintPlane with CIFEraseTable).
  [[nodiscard]] diag::Status erase_triangle(const db::Rect& area, db::Diagonal diagonal, bool right_side,
                                            bool magic_compat = true);
  /// AND-NOT: removes every solid tile of `other` from this plane, in
  /// `other`'s enumeration order (CIFOP_ANDNOT over a mask layer).
  [[nodiscard]] diag::Status subtract(const MaskPlane& other);
  /// OR: paints every solid tile of `other` into this plane, in `other`'s
  /// enumeration order (cifPaintFunc with CIFPaintTable, scale 1).
  [[nodiscard]] diag::Status unite(const MaskPlane& other);
  /// cifClipPlane: erases everything outside `clip`; a side of `clip` at or
  /// beyond the plane's edge clips nothing.
  void clip(const db::Rect& clip);
  /// Makes the whole plane space again (DBClearPaintPlane).
  void clear();
  /// Replaces this plane by a copy of `other` (same tiles).
  void copy_from(const MaskPlane& other);
  /// Exchanges the contents of two planes (CIFGenLayer's curPlane and
  /// nextPlane swap).
  void swap(MaskPlane& other) noexcept;

  /// True when the plane holds no mask tile.
  [[nodiscard]] bool empty() const;
  /// The solid tiles' rectangles (bounding rectangles for split tiles) in
  /// Magic's area enumeration order.
  [[nodiscard]] std::vector<db::Rect> solid_rects() const;
  /// The solid tiles as enumerated (split tiles once per solid side).
  [[nodiscard]] std::vector<db::TileView> solid_tiles() const;
  /// Total solid area in mask units² (a split tile counts its triangle).
  [[nodiscard]] double solid_area() const;

  [[nodiscard]] const db::Plane& plane() const noexcept { return plane_; }
  [[nodiscard]] db::Plane& plane() noexcept { return plane_; }

 private:
  db::Plane plane_;
};

/// Scratch planes reused across generations (design D4/D14). Not
/// thread-safe: each caller (command thread, DRC worker) owns its pool.
class PlanePool {
 public:
  /// A cleared plane, from the free list or new. The pool keeps ownership;
  /// the plane stays valid until release() or the pool is destroyed.
  [[nodiscard]] MaskPlane& acquire();
  /// Returns `plane` (acquired from this pool) to the free list.
  void release(MaskPlane& plane);

  /// Planes created so far (in use and free).
  [[nodiscard]] std::size_t size() const noexcept { return planes_.size(); }

 private:
  std::vector<std::unique_ptr<MaskPlane>> planes_;
  std::vector<MaskPlane*> free_;
};

/// A plane from a pool, released when the handle goes out of scope.
class ScratchPlane {
 public:
  explicit ScratchPlane(PlanePool& pool) : pool_(pool), plane_(pool.acquire()) {}
  ~ScratchPlane() { pool_.release(plane_); }
  ScratchPlane(const ScratchPlane&) = delete;
  ScratchPlane& operator=(const ScratchPlane&) = delete;

  [[nodiscard]] MaskPlane& get() noexcept { return plane_; }
  [[nodiscard]] MaskPlane* operator->() noexcept { return &plane_; }
  [[nodiscard]] MaskPlane& operator*() noexcept { return plane_; }

 private:
  PlanePool& pool_;
  MaskPlane& plane_;
};

}  // namespace hefesto::mask
