// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The corner-stitched plane: canonical decomposition, Magic's enumeration
// order, split tiles (layout-geometry-model), triangle painting and split
// merging (layout-paint-semantics), and the M0 testing-strategy property
// tests (paint order independence with canonical decomposition, erase after
// paint), checked against the reference model of db_fixture.hpp.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/plane.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;
using db::test::paint_row_of;
using db::test::rects;

namespace {

constexpr tech::TileType kM1 = 20;
constexpr tech::TileType kM2 = 21;

void paint(db::Plane& p, const Rect& r, tech::TileType t = kM1) {
  REQUIRE(p.paint(r, paint_row_of(t)).ok());
}

void tri(db::Plane& p, const Rect& r, db::TriangleCorner corner, tech::TileType t = kM1) {
  const db::TriangleSide side = db::triangle_side(corner);
  REQUIRE(p.paint_triangle(r, side.diagonal, side.right_side, paint_row_of(t)).ok());
}

std::vector<db::TileView> split_tiles(const db::Plane& p) {
  std::vector<db::TileView> out;
  for (const auto& t : db::test::tiles(p)) {
    if (t.split) {
      out.push_back(t);
    }
  }
  return out;
}

}  // namespace

#define GEO "[cap:layout-geometry-model]"
#define NM "[cap:layout-paint-semantics]"

TEST_CASE("adjacent rectangles merge horizontally", GEO "[req:Canonical maximal-horizontal-strip decomposition]") {
  db::Plane p;
  paint(p, {0, 0, 10, 10});
  paint(p, {10, 0, 20, 10});
  CHECK(rects(p) == std::vector<Rect>{{0, 0, 20, 10}});
}

TEST_CASE("stacked rectangles merge vertically only when aligned",
          GEO "[req:Canonical maximal-horizontal-strip decomposition]") {
  db::Plane a;
  paint(a, {0, 0, 10, 10});
  paint(a, {0, 10, 10, 20});
  CHECK(rects(a) == std::vector<Rect>{{0, 0, 10, 20}});
  db::Plane b;
  paint(b, {0, 0, 10, 10});
  paint(b, {5, 10, 15, 20});
  CHECK(rects(b) == std::vector<Rect>{{5, 10, 15, 20}, {0, 0, 10, 10}});
}

TEST_CASE("decomposition is independent of paint history",
          GEO "[req:Canonical maximal-horizontal-strip decomposition]") {
  db::Plane a;
  paint(a, {0, 0, 20, 10});
  paint(a, {0, 10, 10, 20});
  db::Plane b;
  paint(b, {0, 0, 10, 20});
  paint(b, {10, 0, 20, 10});
  const std::vector<Rect> expected{{0, 10, 10, 20}, {0, 0, 20, 10}};
  CHECK(rects(a) == expected);
  CHECK(rects(b) == expected);
}

TEST_CASE("enumeration is a tree walk, not a raster scan", GEO "[req:Tile enumeration order]") {
  db::Plane p;
  paint(p, {20, 0, 30, 30});  // X
  paint(p, {0, 20, 10, 30});  // Y
  paint(p, {0, 0, 10, 10});   // Z
  CHECK(rects(p) == std::vector<Rect>{{0, 20, 10, 30}, {0, 0, 10, 10}, {20, 0, 30, 30}});
}

TEST_CASE("simple raster-like enumeration", GEO "[req:Tile enumeration order]") {
  db::Plane p;
  paint(p, {0, 20, 10, 30});
  paint(p, {0, 0, 10, 10});
  paint(p, {20, 0, 30, 10});
  CHECK(rects(p) == std::vector<Rect>{{0, 20, 10, 30}, {0, 0, 10, 10}, {20, 0, 30, 10}});
}

TEST_CASE("the type filter never changes the traversal", GEO "[req:Tile enumeration order]") {
  db::Plane p;
  paint(p, {20, 0, 30, 30}, kM1);
  paint(p, {0, 20, 10, 30}, kM2);
  paint(p, {0, 0, 10, 10}, kM1);
  CHECK(rects(p, tech::TypeMask::of(kM1)) == std::vector<Rect>{{0, 0, 10, 10}, {20, 0, 30, 30}});
  CHECK(rects(p, tech::TypeMask::of(kM2)) == std::vector<Rect>{{0, 20, 10, 30}});
}

// The reference is db_fixture.hpp's raster model of the spec's enumeration
// order (written from the spec text), not Magic itself; the comparison with
// Magic's own output is golden case MODEL-G-005 (CI capture).
TEST_CASE("30-rectangle stress order matches the reference tree walk", GEO "[req:Tile enumeration order]") {
  db::test::Lcg rng(12345);  // portable fixed seed: same history on every platform
  db::Plane p;
  db::test::Raster ref(Rect{-2, -2, 50, 50});
  for (int i = 0; i < 30; ++i) {
    const int x = rng.uniform(0, 36);
    const int y = rng.uniform(0, 36);
    const int w = rng.uniform(1, 9);
    const int h = rng.uniform(1, 9);
    const Rect r{x, y, x + w, y + h};
    paint(p, r);
    ref.paint(r, kM1);
  }
  CHECK(rects(p) == ref.non_space_order());
}

TEST_CASE("property: paint order independence with canonical decomposition",
          GEO "[req:Canonical maximal-horizontal-strip decomposition]") {
  db::test::Lcg rng(777);
  for (int trial = 0; trial < 40; ++trial) {
    std::vector<Rect> shapes;
    for (int i = 0; i < 12; ++i) {
      const int x = rng.uniform(0, 24);
      const int y = rng.uniform(0, 24);
      const int w = rng.uniform(1, 8);
      const int h = rng.uniform(1, 8);
      shapes.push_back(Rect{x, y, x + w, y + h});
    }
    db::Plane forward;
    db::test::Raster ref(Rect{-2, -2, 36, 36});
    for (const Rect& r : shapes) {
      paint(forward, r);
      ref.paint(r, kM1);
    }
    rng.shuffle(shapes);
    db::Plane shuffled;
    for (const Rect& r : shapes) {
      paint(shuffled, r);
    }
    const auto expected = ref.non_space_order();
    REQUIRE(rects(forward) == expected);
    REQUIRE(rects(shuffled) == expected);
  }
}

TEST_CASE("property: erase after paint restores the canonical plane",
          GEO "[req:Canonical maximal-horizontal-strip decomposition]") {
  db::test::Lcg rng(4242);
  for (int trial = 0; trial < 40; ++trial) {
    db::Plane p;
    db::test::Raster ref(Rect{-2, -2, 38, 38});
    for (int i = 0; i < 10; ++i) {
      const int x = rng.uniform(0, 24);
      const int y = rng.uniform(0, 24);
      const int w = rng.uniform(1, 10);
      const int h = rng.uniform(1, 10);
      const Rect r{x, y, x + w, y + h};
      const bool erase = (i % 3) == 2;
      REQUIRE(p.paint(r, erase ? db::test::erase_row_of(kM1) : paint_row_of(kM1)).ok());
      erase ? ref.erase(r, kM1) : ref.paint(r, kM1);
    }
    REQUIRE(rects(p) == ref.non_space_order());
    // Erasing everything that was painted leaves the single space tile.
    REQUIRE(p.paint(Rect{-5, -5, 40, 40}, db::test::erase_row_of(kM1)).ok());
    CHECK(p.empty());
    CHECK(p.tile_count() == 1);
  }
}

TEST_CASE("whole-plane rectangle and plane limits", GEO "[req:Integer database coordinate space]") {
  static_assert(db::whole_plane() == Rect{-1073741818, -1073741818, 1073741818, 1073741818});
  db::Plane p;
  const auto status = p.paint(Rect{0, 0, 1073741820, 10}, paint_row_of(kM1));
  REQUIRE_FALSE(status.ok());
  CHECK(status.error().code == db::codes::kGeometryOutsidePlane);
  CHECK(p.empty());
  CHECK(p.tile_count() == 1);
}

TEST_CASE("a fresh plane is one space tile", GEO "[req:Plane model]") {
  db::Plane p;
  const auto all = db::test::tiles(p, tech::TypeMask::all());
  REQUIRE(all.size() == 1);
  // As in Magic (DBNewPlane): from the usable-plane corner up to INFINITY,
  // where the boundary tiles start.
  CHECK(all[0].area == Rect{db::kPlaneMin, db::kPlaneMin, db::kInfinity, db::kInfinity});
  CHECK(all[0].type == tech::kSpace);
  CHECK(p.type_at({5, 5}) == tech::kSpace);
}

TEST_CASE("triangle written with its right-angle corner", GEO "[req:Non-Manhattan (split tile) geometry]") {
  db::Plane p;
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::nw);
  const auto s = split_tiles(p);
  REQUIRE(s.size() == 1);
  CHECK(s[0].area == Rect{0, 0, 10, 10});
  CHECK(s[0].left_type == kM1);
  CHECK(s[0].right_type == tech::kSpace);
  CHECK(db::triangle_corner(s[0].diagonal, s[0].right_side) == db::TriangleCorner::nw);
  CHECK(p.type_at({1, 8}) == kM1);
  CHECK(p.type_at({8, 1}) == tech::kSpace);
}

TEST_CASE("both halves equal collapse to a rectangle", GEO "[req:Non-Manhattan (split tile) geometry]") {
  db::Plane p;
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::nw);
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::se);
  CHECK(split_tiles(p).empty());
  CHECK(rects(p) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("triangle on an empty plane", NM "[req:Non-Manhattan painting]") {
  db::Plane p;
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::nw);
  const auto all = db::test::tiles(p);
  REQUIRE(all.size() == 1);
  CHECK(all[0].split);
  CHECK(all[0].diagonal == db::Diagonal::ne_sw);
  CHECK_FALSE(all[0].right_side);
}

TEST_CASE("triangle over existing paint splits at integer crossings", NM "[req:Non-Manhattan painting]") {
  db::Plane p;
  paint(p, {0, 5, 10, 10});
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::nw);
  const auto all = db::test::tiles(p);
  std::vector<Rect> plain;
  std::vector<db::TileView> triangles;
  for (const auto& t : all) {
    (t.split ? triangles.push_back(t) : plain.push_back(t.area));
  }
  CHECK(plain == std::vector<Rect>{{0, 5, 10, 10}});
  REQUIRE(triangles.size() == 1);
  CHECK(triangles[0].area == Rect{0, 0, 5, 5});
  CHECK(db::triangle_corner(triangles[0].diagonal, triangles[0].right_side) == db::TriangleCorner::nw);
}

TEST_CASE("rounding of non-integer crossings follows Magic", NM "[req:Non-Manhattan painting]") {
  // MODEL-G-021: a stripe at y 1..2 cuts the triangle (0,0)-(10,3); the '/'
  // diagonal crosses y=1 at 10/3 -> 3 and y=2 at 20/3 -> 7 (rounded).
  db::Plane p;
  paint(p, {0, 1, 10, 2}, kM2);
  tri(p, {0, 0, 10, 3}, db::TriangleCorner::se, kM1);  // right side of '/'
  std::vector<Rect> plain;
  std::vector<Rect> triangles;
  for (const auto& t : db::test::tiles(p, tech::TypeMask::of(kM1))) {
    if (t.split) {
      CHECK(db::triangle_corner(t.diagonal, t.right_side) == db::TriangleCorner::se);
      triangles.push_back(t.area);
    } else {
      plain.push_back(t.area);
    }
  }
  const auto sorted = [](std::vector<Rect> v) {
    std::sort(v.begin(), v.end(), [](const Rect& a, const Rect& b) { return a.y0 != b.y0 ? a.y0 < b.y0 : a.x0 < b.x0; });
    return v;
  };
  CHECK(sorted(plain) == std::vector<Rect>{{3, 0, 10, 1}, {7, 1, 10, 2}});
  CHECK(sorted(triangles) == std::vector<Rect>{{0, 0, 3, 1}, {3, 1, 7, 2}, {7, 2, 10, 3}});
  CHECK(p.type_at({0, 1}) == kM2);
  CHECK(p.type_at({0, 2}) == tech::kSpace);
}

TEST_CASE("opposite-diagonal overlap quarters the area", NM "[req:Non-Manhattan painting]") {
  db::Plane p;
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::nw);
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::ne);
  // Both triangles cover the upper half; the lower-left/lower-right corners
  // keep space.
  CHECK(p.type_at({5, 9}) == kM1);
  CHECK(p.type_at({0, 9}) == kM1);
  CHECK(p.type_at({9, 9}) == kM1);
  CHECK(p.type_at({5, 0}) == tech::kSpace);
  // A 1-unit-high box cannot be quartered: the rectangle is filled (Q-MODEL-4).
  db::Plane q;
  tri(q, {0, 0, 5, 1}, db::TriangleCorner::nw);
  tri(q, {0, 0, 5, 1}, db::TriangleCorner::ne);
  CHECK(rects(q) == std::vector<Rect>{{0, 0, 5, 1}});
}

TEST_CASE("two collinear triangles merge into one", NM "[req:Split tile merging]") {
  db::Plane p;
  tri(p, {0, 0, 5, 5}, db::TriangleCorner::nw);
  tri(p, {5, 5, 10, 10}, db::TriangleCorner::nw);
  paint(p, {0, 5, 5, 10});
  const auto s = split_tiles(p);
  REQUIRE(s.size() == 1);
  CHECK(s[0].area == Rect{0, 0, 10, 10});
  CHECK(db::triangle_corner(s[0].diagonal, s[0].right_side) == db::TriangleCorner::nw);
}

TEST_CASE("enumeration reports a split tile once per matching side", GEO "[req:Tile enumeration order]") {
  db::Plane p;
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::sw, kM1);
  tri(p, {0, 0, 10, 10}, db::TriangleCorner::ne, kM2);
  const auto both = db::test::tiles(p);
  REQUIRE(both.size() == 2);
  CHECK_FALSE(both[0].right_side);  // left part first
  CHECK(both[0].type == kM1);
  CHECK(both[1].right_side);
  CHECK(both[1].type == kM2);
  // A search area entirely beyond the diagonal skips that side.
  std::vector<db::TileView> found;
  p.enumerate(Rect{8, 8, 10, 10}, tech::TypeMask::of(kM1), [&](const db::TileView& t) {
    found.push_back(t);
    return true;
  });
  CHECK(found.empty());
}

TEST_CASE("clone copies every tile", GEO "[req:Plane model]") {
  db::Plane p;
  paint(p, {0, 0, 10, 10});
  tri(p, {20, 0, 30, 10}, db::TriangleCorner::ne);
  const db::Plane c = p.clone();
  CHECK(db::test::tiles(c) == db::test::tiles(p));
  CHECK(c.bounding_box() == Rect{0, 0, 30, 10});
}
