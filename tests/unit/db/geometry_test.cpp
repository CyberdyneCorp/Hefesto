// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Rectangles, predicates and Manhattan transforms (layout-geometry-model).

#include <catch2/catch_test_macros.hpp>

#include <array>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/transform.hpp"

namespace db = hefesto::db;
using db::Point;
using db::Rect;
using db::Transform;

#define GEO "[cap:layout-geometry-model]"

TEST_CASE("overlap versus touch", GEO "[req:Rectangle semantics and predicates]") {
  STATIC_CHECK_FALSE(db::overlap(Rect{0, 0, 10, 10}, Rect{10, 0, 20, 10}));
  STATIC_CHECK(db::touch(Rect{0, 0, 10, 10}, Rect{10, 0, 20, 10}));
}

TEST_CASE("corner-only contact touches", GEO "[req:Rectangle semantics and predicates]") {
  STATIC_CHECK(db::touch(Rect{0, 0, 10, 10}, Rect{10, 10, 20, 20}));
  STATIC_CHECK_FALSE(db::overlap(Rect{0, 0, 10, 10}, Rect{10, 10, 20, 20}));
}

TEST_CASE("point label inside a zero-size search area", GEO "[req:Rectangle semantics and predicates]") {
  STATIC_CHECK(db::label_in_area(Rect{5, 5, 5, 5}, Rect{5, 5, 5, 5}));
  STATIC_CHECK(db::surround(Rect{0, 0, 10, 10}, Rect{0, 0, 10, 10}));
  STATIC_CHECK_FALSE(db::strong_surround(Rect{0, 0, 10, 10}, Rect{0, 0, 10, 10}));
  STATIC_CHECK(db::encloses(Rect{0, 0, 10, 10}, Point{10, 10}));
  STATIC_CHECK(db::canonical(Rect{10, 10, 0, 0}) == Rect{0, 0, 10, 10});
  STATIC_CHECK(db::include(Rect{}, Rect{1, 1, 2, 2}) == Rect{1, 1, 2, 2});
  STATIC_CHECK(db::include(Rect{1, 1, 2, 2}, Rect{5, 5, 5, 5}) == Rect{1, 1, 2, 2});
  STATIC_CHECK(db::expand(Rect{1, 1, 2, 2}, 1) == Rect{0, 0, 3, 3});
}

TEST_CASE("whole-plane rectangle", GEO "[req:Integer database coordinate space]") {
  STATIC_CHECK(db::whole_plane() == Rect{-1073741818, -1073741818, 1073741818, 1073741818});
  STATIC_CHECK(db::kInfinity == 1073741820);
  STATIC_CHECK_FALSE(db::in_usable_plane(Rect{0, 0, 1073741820, 10}));
}

TEST_CASE("rotating a rectangle", GEO "[req:Manhattan transforms]") {
  constexpr Transform rot90{0, 1, 0, -1, 0, 0};
  STATIC_CHECK(db::apply(rot90, Rect{0, 0, 10, 20}) == Rect{0, -10, 20, 0});
}

TEST_CASE("composition and inverse", GEO "[req:Manhattan transforms]") {
  constexpr Transform t{0, 1, 100, -1, 0, 0};
  STATIC_CHECK(db::apply(t, Point{10, 0}) == Point{100, -10});
  STATIC_CHECK(db::invert(t) == Transform{0, -1, 0, 1, 0, -100});
  STATIC_CHECK(db::apply(db::invert(t), Point{100, -10}) == Point{10, 0});
  // "first then second" = second o first, Magic's GeoTransTrans formulas.
  constexpr Transform first{1, 0, 5, 0, -1, 7};
  constexpr Transform second{0, 1, 3, -1, 0, 4};
  constexpr Transform both = db::compose(second, first);
  STATIC_CHECK(db::apply(both, Point{2, 9}) == db::apply(second, db::apply(first, Point{2, 9})));
  STATIC_CHECK(both.c == first.c * second.a + first.f * second.b + second.c);
}

TEST_CASE("orientation names", GEO "[req:Manhattan transforms]") {
  CHECK(db::orientation_name(db::orientation(Transform{1, 0, 5, 0, -1, 7})) == "FS");
  CHECK(db::orientation_name(db::orientation(Transform{1, 0, 0, 0, 1, 0})) == "N");
  CHECK(db::orientation_name(db::orientation(Transform{0, 1, 0, -1, 0, 0})) == "E");
  CHECK(db::orientation_name(db::orientation(Transform{-1, 0, 0, 0, -1, 0})) == "S");
  CHECK(db::orientation_name(db::orientation(Transform{0, -1, 0, 1, 0, 0})) == "W");
  CHECK(db::orientation_name(db::orientation(Transform{-1, 0, 0, 0, 1, 0})) == "FN");
  CHECK(db::orientation_name(db::orientation(Transform{0, 1, 0, 1, 0, 0})) == "FW");
  CHECK(db::orientation_name(db::orientation(Transform{0, -1, 0, -1, 0, 0})) == "FE");
}

TEST_CASE("decomposition into flip and angle", GEO "[req:Manhattan transforms]") {
  CHECK(db::decompose(Transform{1, 0, 0, 0, 1, 0}) == db::RotationDecomposition{false, 0});
  CHECK(db::decompose(Transform{0, 1, 0, -1, 0, 0}) == db::RotationDecomposition{false, 90});
  CHECK(db::decompose(Transform{-1, 0, 0, 0, -1, 0}) == db::RotationDecomposition{false, 180});
  CHECK(db::decompose(Transform{0, -1, 0, 1, 0, 0}) == db::RotationDecomposition{false, 270});
  CHECK(db::decompose(Transform{1, 0, 0, 0, -1, 0}) == db::RotationDecomposition{true, 0});
}

TEST_CASE("justification under rotation and mirror", GEO "[req:Transforming positions, angles and triangles]") {
  CHECK(db::transform_position(Transform{0, 1, 0, -1, 0, 0}, db::kNorth) == db::kEast);
  CHECK(db::transform_position(Transform{1, 0, 0, 0, -1, 0}, db::kNorthEast) == db::kSouthEast);
  CHECK(db::transform_position(Transform{1, 0, 0, 0, 1, 0}, db::kCenter) == db::kCenter);
  CHECK(db::transform_position(Transform{0, 1, 0, -1, 0, 0}, 9) == 9);  // out of range: unchanged
  CHECK(db::opposite_position(db::kNorthEast) == db::kSouthWest);
  CHECK(db::opposite_position(db::kEast) == db::kWest);
}

TEST_CASE("triangle corner under rotation", GEO "[req:Transforming positions, angles and triangles]") {
  constexpr Transform rot90{0, 1, 0, -1, 0, 0};
  const db::TriangleSide nw = db::triangle_side(db::TriangleCorner::nw);
  const db::TriangleSide out = db::transform_triangle(rot90, nw.diagonal, nw.right_side);
  CHECK(db::triangle_corner(out.diagonal, out.right_side) == db::TriangleCorner::ne);
  CHECK(db::apply(rot90, Rect{0, 0, 10, 10}) == Rect{0, -10, 10, 0});
  // The right-angle corner transforms as a position for every orientation.
  constexpr std::array<Transform, 8> all = {{{1, 0, 0, 0, 1, 0},
                                             {0, 1, 0, -1, 0, 0},
                                             {-1, 0, 0, 0, -1, 0},
                                             {0, -1, 0, 1, 0, 0},
                                             {1, 0, 0, 0, -1, 0},
                                             {-1, 0, 0, 0, 1, 0},
                                             {0, 1, 0, 1, 0, 0},
                                             {0, -1, 0, -1, 0, 0}}};
  const auto corner_pos = [](db::TriangleCorner c) {
    switch (c) {
      case db::TriangleCorner::nw:
        return db::kNorthWest;
      case db::TriangleCorner::sw:
        return db::kSouthWest;
      case db::TriangleCorner::se:
        return db::kSouthEast;
      case db::TriangleCorner::ne:
        return db::kNorthEast;
    }
    return db::kCenter;
  };
  for (const Transform& t : all) {
    for (const auto c : {db::TriangleCorner::nw, db::TriangleCorner::sw, db::TriangleCorner::se,
                         db::TriangleCorner::ne}) {
      const db::TriangleSide in = db::triangle_side(c);
      const db::TriangleSide moved = db::transform_triangle(t, in.diagonal, in.right_side);
      CHECK(corner_pos(db::triangle_corner(moved.diagonal, moved.right_side)) ==
            db::transform_position(t, corner_pos(c)));
    }
  }
}

TEST_CASE("angle wrap quirk and magic-compat off", GEO "[req:Transforming positions, angles and triangles]") {
  constexpr Transform rot90{0, 1, 0, -1, 0, 0};
  CHECK(db::transform_angle(rot90, 270, true) == 360);  // Q-MODEL-2: kept
  CHECK(db::transform_angle(rot90, 0, true) == 90);     // Q-MODEL-1: +90 in compat
  CHECK(db::transform_angle(rot90, 0, false) == 270);   // -90 with compat off
  CHECK(db::transform_angle(Transform{0, -1, 0, 1, 0, 0}, 270, false) == 180);
  CHECK(db::transform_angle(Transform{-1, 0, 0, 0, 1, 0}, 45, true) == 135);  // flip
  CHECK(db::transform_angle(Transform{-1, 0, 0, 0, 1, 0}, 135, true) == 45);
}

TEST_CASE("position names by unique prefix", GEO "[req:Transforming positions, angles and triangles]") {
  CHECK(db::parse_position("north", false) == db::kNorth);
  CHECK(db::parse_position("ne", false) == db::kNorthEast);
  CHECK(db::parse_position("NE", false) == db::kNorthEast);
  CHECK(db::parse_position("center", false) == db::kCenter);
  CHECK(db::parse_position("nort", false) == -1);  // north/northeast/northwest
  CHECK(db::parse_position("ne", true) == -2);    // not a Manhattan name
  CHECK(db::parse_position("so", false) == -1);   // south/southeast/southwest
  CHECK(db::parse_position("xyz", false) == -2);
}
