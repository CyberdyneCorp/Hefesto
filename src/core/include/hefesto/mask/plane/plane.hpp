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
// Owner: INTEGRATION (this header, created by the M5 scaffold); OPS fills in
// the implementation in src/core/src/mask/plane/ (design D2). The CIF paint
// table moves here from hints/region (the hints API is unchanged).

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

  /// Unions `area` into the plane (DBPaintPlane with CIFPaintTable).
  [[nodiscard]] diag::Status paint(const db::Rect& area);
  /// Removes `area` from the plane (DBPaintPlane with CIFEraseTable).
  [[nodiscard]] diag::Status erase(const db::Rect& area);
  /// AND-NOT: removes every tile of `other` from this plane.
  [[nodiscard]] diag::Status subtract(const MaskPlane& other);
  /// Makes the whole plane space again (DBClearPaintPlane).
  void clear();

  /// True when the plane holds no mask tile.
  [[nodiscard]] bool empty() const;

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

}  // namespace hefesto::mask
