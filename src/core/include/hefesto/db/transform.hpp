// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Manhattan transforms (capability layout-geometry-model, requirements
// "Manhattan transforms" and "Transforming positions, angles and
// triangles"). Same layout as Magic's Transform and the `.mag` `transform`
// line: x' = a*x + b*y + c, y' = d*x + e*y + f, with a,b,d,e in {-1,0,1}.
// (Magic ref: utils/geometry.h:46-60, utils/geometry.c)
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/db/geometry.hpp"

namespace hefesto::db {

struct Transform {
  std::int32_t a = 1;
  std::int32_t b = 0;
  Coord c = 0;
  std::int32_t d = 0;
  std::int32_t e = 1;
  Coord f = 0;

  friend constexpr bool operator==(const Transform&, const Transform&) = default;
};

inline constexpr Transform kIdentity{};

[[nodiscard]] constexpr Point apply(const Transform& t, const Point& p) noexcept {
  return Point{t.a * p.x + t.b * p.y + t.c, t.d * p.x + t.e * p.y + t.f};
}

/// Transforms both corners and canonicalizes (GeoTransRect).
[[nodiscard]] constexpr Rect apply(const Transform& t, const Rect& r) noexcept {
  const Point p0 = apply(t, Point{r.x0, r.y0});
  const Point p1 = apply(t, Point{r.x1, r.y1});
  return canonical(Rect{p0.x, p0.y, p1.x, p1.y});
}

/// `outer` after `inner`: apply(compose(outer, inner), p) == apply(outer, apply(inner, p)).
[[nodiscard]] constexpr Transform compose(const Transform& outer, const Transform& inner) noexcept {
  return Transform{outer.a * inner.a + outer.b * inner.d,
                   outer.a * inner.b + outer.b * inner.e,
                   outer.a * inner.c + outer.b * inner.f + outer.c,
                   outer.d * inner.a + outer.e * inner.d,
                   outer.d * inner.b + outer.e * inner.e,
                   outer.d * inner.c + outer.e * inner.f + outer.f};
}

/// Inverse of a Manhattan transform (the 2x2 part is orthogonal).
[[nodiscard]] constexpr Transform invert(const Transform& t) noexcept {
  return Transform{t.a, t.d, -(t.a * t.c + t.d * t.f), t.b, t.e, -(t.b * t.c + t.e * t.f)};
}

[[nodiscard]] constexpr Transform translation(Coord dx, Coord dy) noexcept {
  return Transform{1, 0, dx, 0, 1, dy};
}

/// Justification of a label position, 0 = center, 1..8 = N, NE, E, SE, S, SW, W, NW
/// (GEO_CENTER .. GEO_NORTHWEST).
using Position = std::int32_t;

inline constexpr Position kCenter = 0;
inline constexpr Position kNorth = 1;
inline constexpr Position kNorthEast = 2;
inline constexpr Position kEast = 3;
inline constexpr Position kSouthEast = 4;
inline constexpr Position kSouth = 5;
inline constexpr Position kSouthWest = 6;
inline constexpr Position kWest = 7;
inline constexpr Position kNorthWest = 8;

/// "Transforming positions": the direction `pos` becomes under `t` (GeoTransPos).
[[nodiscard]] Position transform_position(const Transform& t, Position pos) noexcept;

/// The opposite position (N<->S, NE<->SW, E<->W, SE<->NW, center<->center).
[[nodiscard]] Position opposite_position(Position pos) noexcept;

/// GeoNameToPos: unique-prefix lookup of a position name ("Position names
/// accepted on input"); `manhattan` restricts to the Manhattan names.
/// Returns the position, -1 when ambiguous, -2 when invalid.
[[nodiscard]] int parse_position(std::string_view name, bool manhattan) noexcept;

/// The message GeoNameToPos prints in verbose mode when `name` is ambiguous,
/// invalid or (with `manhattan`) not Manhattan, ending with the list of
/// legal names (no final line feed).
[[nodiscard]] std::string position_error_text(std::string_view name, bool manhattan);

/// GeoPosToName: "CENTER", "NORTH", ... "NORTHWEST", "*ILLEGAL*" otherwise.
[[nodiscard]] std::string_view position_long_name(Position pos) noexcept;

/// "Transforming positions, angles and triangles": a label rotation in
/// degrees under `t` (GeoTransAngle). `magic_compat` true keeps Magic's
/// behavior; false reduces a sum of 360 to 0 (Q-MODEL-2) and makes the
/// clockwise-90 transform subtract 90 (Q-MODEL-1).
[[nodiscard]] std::int32_t transform_angle(const Transform& t, std::int32_t degrees, bool magic_compat) noexcept;

/// DEF-style orientation of a transform ("Manhattan transforms", GeoTransOrient).
enum class Orientation : std::uint8_t { N, S, E, W, FN, FS, FE, FW };
[[nodiscard]] Orientation orientation(const Transform& t) noexcept;
[[nodiscard]] std::string_view orientation_name(Orientation o) noexcept;

/// Decomposition into an upside-down flag and an angle (0, 90, 180, 270)
/// used by display and output writers ("Manhattan transforms").
struct RotationDecomposition {
  bool upside_down = false;
  std::int32_t angle = 0;
  friend constexpr bool operator==(const RotationDecomposition&, const RotationDecomposition&) = default;
};
[[nodiscard]] RotationDecomposition decompose(const Transform& t) noexcept;

}  // namespace hefesto::db
