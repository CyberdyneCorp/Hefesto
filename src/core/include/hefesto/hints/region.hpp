// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hints::HintRegion: the value of a `MASKHINTS_<name>` cell property
// (mask-hints "Mask-hint property data model"; openspec/changes/m3-drc
// design.md D4/D10). It is the plane-property type of the property store:
// db::PlaneValue is an alias of it, so db::PropertyValue holds it and the
// property API keeps its shape.
//
// A region is the union of the rectangles painted into it, held as Magic
// holds it: a tile plane of one hint type painted with the CIF paint table,
// whose tiles are the canonical maximal horizontal strips, enumerated in
// Magic's area order (top to bottom, left to right). The compat inside test
// (Q-DRC-11/Q-MISC-13) walks that plane and the `.mag` strip order follows
// it. The decomposition is unique for a given region, so two regions are
// equal exactly when their strips are. A rectangle with zero or negative
// width or height contributes nothing (DBPaintPlane); geometry is Manhattan.
//
// Values are immutable once built: every mutation rebuilds the plane, and
// copies share it, so a region may be read from several threads (the DRC
// background checker) while the editor replaces the property value.
//
// (Magic ref: database/DBprop.c, database/database.h.in PROPERTY_TYPE_PLANE,
// cif/CIFint.h CIFPaintTable, database/DBcellsrch.c DBScaleValue and
// dbScalePlane, cif/CIFgen.c CIFOP_ANDNOT)
//
// Owner: HINTS (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/transform.hpp"

namespace hefesto::db {
class Plane;
}

namespace hefesto::hints {

class HintRegion {
 public:
  HintRegion() = default;
  /// The union of `rects` (painted in order).
  HintRegion(std::initializer_list<db::Rect> rects);  // NOLINT(google-explicit-constructor)
  explicit HintRegion(std::span<const db::Rect> rects);

  /// Unions `area` into the region (no-op for a rectangle without area).
  void paint(const db::Rect& area);
  /// Unions every rectangle of `areas`.
  void paint(std::span<const db::Rect> areas);
  /// Unions another region.
  void paint(const HintRegion& other);

  [[nodiscard]] bool empty() const noexcept { return strips_.empty(); }
  /// The maximal horizontal strips in Magic's enumeration order.
  [[nodiscard]] const std::vector<db::Rect>& strips() const noexcept { return strips_; }
  /// Bounding box of the region; a null rectangle when empty.
  [[nodiscard]] db::Rect bounding_box() const noexcept;
  /// The hint plane (hint type 1 on space), or nullptr when the region is
  /// empty. Shared by copies; never modified.
  [[nodiscard]] const db::Plane* plane() const noexcept { return plane_.get(); }

  /// The region under a Manhattan transform (flatten, copy).
  [[nodiscard]] HintRegion transformed(const db::Transform& transform) const;
  /// The region translated by (dx, dy) (origin change).
  [[nodiscard]] HintRegion translated(db::Coord dx, db::Coord dy) const;
  /// The region with every strip corner multiplied by num / den, rounding
  /// down (Magic's DBScaleValue); strips scaled to no area vanish (grid
  /// rescale, output scaling; num, den > 0).
  [[nodiscard]] HintRegion scaled(std::int64_t num, std::int64_t den) const;
  /// The part of the region inside `clip`.
  [[nodiscard]] HintRegion clipped(const db::Rect& clip) const;
  /// The part of the region outside `other` (CIF AND-NOT).
  [[nodiscard]] HintRegion minus(const HintRegion& other) const;

  friend bool operator==(const HintRegion& a, const HintRegion& b) noexcept { return a.strips_ == b.strips_; }

 private:
  /// Rebuilds the plane and the strips from strips_ plus `extra`, erasing
  /// `erase` afterwards.
  void rebuild(std::span<const db::Rect> extra, std::span<const db::Rect> erase = {});

  std::vector<db::Rect> strips_;
  std::shared_ptr<const db::Plane> plane_;
};

}  // namespace hefesto::hints
