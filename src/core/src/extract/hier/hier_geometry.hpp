// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Magic's rectangle helpers and hierarchy searches as extract/hier uses them
// (utils/geometry.c GeoClip, GeoIncludeAll; database/DBcellsrch.c
// DBCellSrArea, DBArraySr). Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

#include "hefesto/db/array_overlap.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/db/use_index.hpp"

namespace hefesto::extract::hier {

/// GeoNullRect.
inline constexpr db::Rect kNullRect{0, 0, 0, 0};

/// GeoClip / GEOCLIP: may leave an inverted rectangle (null by is_null()).
[[nodiscard]] constexpr db::Rect clip(const db::Rect& r, const db::Rect& area) noexcept {
  return db::Rect{r.x0 < area.x0 ? area.x0 : r.x0, r.y0 < area.y0 ? area.y0 : r.y0, r.x1 > area.x1 ? area.x1 : r.x1,
                  r.y1 > area.y1 ? area.y1 : r.y1};
}

/// GeoIncludeAll: like db::include, but a zero-width or zero-height source
/// (a point or line label) still counts; only an inverted one is ignored.
[[nodiscard]] constexpr db::Rect include_all(const db::Rect& dst, const db::Rect& src) noexcept {
  if (dst.x0 > dst.x1 || dst.y0 > dst.y1) {
    return src;
  }
  if (src.x0 > src.x1 || src.y0 > src.y1) {
    return dst;
  }
  return db::Rect{std::min(dst.x0, src.x0), std::min(dst.y0, src.y0), std::max(dst.x1, src.x1),
                  std::max(dst.y1, src.y1)};
}

/// One array element: the use, its indices and the transform from the
/// element's child coordinates to the parent's.
struct Element {
  const db::CellUse* use = nullptr;
  std::int32_t x = 0;
  std::int32_t y = 0;
  db::Transform to_parent;
};

/// The uses of `def` overlapping `area`, in the use index's search order
/// (DBSrCellPlaneArea), skipping uses without a definition.
[[nodiscard]] inline std::vector<const db::CellUse*> uses_in(const db::CellDef& def, const db::Rect& area) {
  std::vector<const db::CellUse*> out;
  (void)db::uses_overlapping(def, area, [&](db::CellUse& u) {
    if (u.def != nullptr) {
      out.push_back(&u);
    }
    return true;
  });
  return out;
}

/// DBArraySr: the elements of `use` whose images may overlap `area`
/// (parent coordinates), y outer and x inner; stops when `visit` returns
/// true and then returns true.
inline bool array_elements(const db::CellUse& use, const db::Rect& area, const std::function<bool(const Element&)>& visit) {
  const db::ElementRange r = db::array_overlap(use, area);
  for (std::int32_t y = r.ylo; y <= r.yhi; ++y) {
    for (std::int32_t x = r.xlo; x <= r.xhi; ++x) {
      if (visit(Element{&use, x, y, db::element_transform(use, x, y)})) {
        return true;
      }
    }
  }
  return false;
}

/// Whether `use` is an array along x / along y.
[[nodiscard]] constexpr bool has_x(const db::CellUse& use) noexcept {
  return use.array.xlo != use.array.xhi;
}
[[nodiscard]] constexpr bool has_y(const db::CellUse& use) noexcept {
  return use.array.ylo != use.array.yhi;
}

}  // namespace hefesto::extract::hier
