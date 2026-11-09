// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Integer layout geometry: coordinates, points and rectangles with Magic's
// exact predicates (capability layout-geometry-model, requirements "Integer
// database coordinate space" and "Rectangle semantics and predicates").
// (Magic ref: tiles/tile.h:149-165, utils/geometry.h:103-171)
//
// Header-only and constexpr: every area (MODEL, CELL, VIEWER) uses it.
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <algorithm>
#include <cstdint>

namespace hefesto::db {

/// A coordinate in internal database units.
using Coord = std::int32_t;

inline constexpr Coord kInfinity = (Coord{1} << 30) - 4;  // 1073741820
inline constexpr Coord kMinusInfinity = -kInfinity;
/// Bounds of the usable plane ("whole plane rectangle").
inline constexpr Coord kPlaneMin = kMinusInfinity + 2;
inline constexpr Coord kPlaneMax = kInfinity - 2;

struct Point {
  Coord x = 0;
  Coord y = 0;

  friend constexpr bool operator==(const Point&, const Point&) = default;
};

/// Lower-left (x0, y0), upper-right (x1, y1). The area is the half-open set
/// x0 <= x < x1, y0 <= y < y1; zero-size rectangles are legal (point and
/// line labels) but have no area.
struct Rect {
  Coord x0 = 0;
  Coord y0 = 0;
  Coord x1 = 0;
  Coord y1 = 0;

  [[nodiscard]] constexpr bool is_null() const noexcept { return x0 >= x1 || y0 >= y1; }
  [[nodiscard]] constexpr std::int64_t width() const noexcept { return std::int64_t{x1} - x0; }
  [[nodiscard]] constexpr std::int64_t height() const noexcept { return std::int64_t{y1} - y0; }

  friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

[[nodiscard]] constexpr Rect whole_plane() noexcept {
  return Rect{kPlaneMin, kPlaneMin, kPlaneMax, kPlaneMax};
}

/// Shared area > 0 (GEO_OVERLAP).
[[nodiscard]] constexpr bool overlap(const Rect& a, const Rect& b) noexcept {
  return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

/// Overlap, shared edge or shared corner (GEO_TOUCH).
[[nodiscard]] constexpr bool touch(const Rect& a, const Rect& b) noexcept {
  return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1;
}

/// `inner` inside `outer`, touching allowed (GEO_SURROUND).
[[nodiscard]] constexpr bool surround(const Rect& outer, const Rect& inner) noexcept {
  return inner.x0 >= outer.x0 && inner.x1 <= outer.x1 && inner.y0 >= outer.y0 && inner.y1 <= outer.y1;
}

/// `inner` strictly inside `outer` (GEO_SURROUND_STRONG).
[[nodiscard]] constexpr bool strong_surround(const Rect& outer, const Rect& inner) noexcept {
  return inner.x0 > outer.x0 && inner.x1 < outer.x1 && inner.y0 > outer.y0 && inner.y1 < outer.y1;
}

/// Closed point enclosure (GEO_ENCLOSE).
[[nodiscard]] constexpr bool encloses(const Rect& r, const Point& p) noexcept {
  return r.x0 <= p.x && p.x <= r.x1 && r.y0 <= p.y && p.y <= r.y1;
}

/// label-in-area (layout-geometry-model).
[[nodiscard]] constexpr bool label_in_area(const Rect& label, const Rect& area) noexcept {
  return surround(area, label) || (area.is_null() && touch(label, area) && !strong_surround(label, area));
}

/// Swaps coordinates so that x0 <= x1 and y0 <= y1 (GeoCanonicalRect).
[[nodiscard]] constexpr Rect canonical(const Rect& r) noexcept {
  return Rect{std::min(r.x0, r.x1), std::min(r.y0, r.y1), std::max(r.x0, r.x1), std::max(r.y0, r.y1)};
}

/// Bounding-box accumulation (GeoIncludeAll semantics for area rectangles):
/// a null source is ignored; a null destination is replaced.
[[nodiscard]] constexpr Rect include(const Rect& bbox, const Rect& r) noexcept {
  if (r.is_null()) {
    return bbox;
  }
  if (bbox.is_null()) {
    return r;
  }
  return Rect{std::min(bbox.x0, r.x0), std::min(bbox.y0, r.y0), std::max(bbox.x1, r.x1), std::max(bbox.y1, r.y1)};
}

/// Grows by n on every side.
[[nodiscard]] constexpr Rect expand(const Rect& r, Coord n) noexcept {
  return Rect{r.x0 - n, r.y0 - n, r.x1 + n, r.y1 + n};
}

/// True when the rectangle lies inside the usable plane (Q-MODEL-24: Hefesto
/// rejects geometry outside it).
[[nodiscard]] constexpr bool in_usable_plane(const Rect& r) noexcept {
  return surround(whole_plane(), r);
}

}  // namespace hefesto::db
