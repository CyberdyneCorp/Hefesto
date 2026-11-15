// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::Plane::merge_split_tiles (DBMergeNMTiles, layout-paint-semantics
// "Split tile merging") over planes painted without the merge
// (PaintMerge::none: DBPaintPlane / DBNMPaintPlane, as mask generation
// paints). The expected decompositions follow DBMergeNMTiles0
// (database/DBpaint.c:1047-1333): the walk starts at each split tile and
// looks for its partner below the diagonal corner, so the upper tile of a
// pair drives the merge.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/plane.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;
using db::test::paint_row_of;

namespace {

constexpr tech::TileType kM1 = 20;
constexpr tech::TileType kM2 = 21;

void paint(db::Plane& p, const Rect& r, tech::TileType t = kM1) {
  REQUIRE(p.paint(r, paint_row_of(t), db::PaintMerge::none).ok());
}

void tri(db::Plane& p, const Rect& r, db::TriangleCorner corner, tech::TileType t = kM1) {
  const db::TriangleSide side = db::triangle_side(corner);
  REQUIRE(p.paint_triangle(r, side.diagonal, side.right_side, paint_row_of(t), true, nullptr, db::PaintMerge::none)
              .ok());
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

/// The type of every unit square of `box`.
std::vector<tech::TileType> raster(const db::Plane& p, const Rect& box) {
  std::vector<tech::TileType> out;
  for (db::Coord y = box.y0; y < box.y1; ++y) {
    for (db::Coord x = box.x0; x < box.x1; ++x) {
      out.push_back(p.type_at({x, y}));
    }
  }
  return out;
}

/// The split tiles' areas, sorted by bottom then left edge.
std::vector<Rect> split_areas(const db::Plane& p) {
  std::vector<Rect> out;
  for (const auto& t : split_tiles(p)) {
    out.push_back(t.area);
  }
  std::sort(out.begin(), out.end(), [](const Rect& a, const Rect& b) { return a.y0 != b.y0 ? a.y0 < b.y0 : a.x0 < b.x0; });
  return out;
}

db::TriangleCorner corner_of(const db::TileView& t) { return db::triangle_corner(t.diagonal, t.right_side); }

/// The spec's pair: the left sides of `/` splits of (0,0)-(5,5) and
/// (5,5)-(10,10), and the left gap (0,5)-(5,10) filled.
db::Plane slash_pair() {
  db::Plane p;
  tri(p, {0, 0, 5, 5}, db::TriangleCorner::nw);
  tri(p, {5, 5, 10, 10}, db::TriangleCorner::nw);
  paint(p, {0, 5, 5, 10});
  return p;
}

}  // namespace

#define MERGE "[cap:layout-paint-semantics][req:Split tile merging]"

TEST_CASE("a bare plane paint leaves collinear triangles apart", MERGE) {
  const db::Plane p = slash_pair();
  CHECK(split_areas(p) == std::vector<Rect>{{0, 0, 5, 5}, {5, 5, 10, 10}});
}

TEST_CASE("merge_split_tiles joins collinear '/' triangles", MERGE) {
  db::Plane p = slash_pair();
  const std::vector<tech::TileType> before = raster(p, {-2, -2, 12, 12});
  p.merge_split_tiles({0, 0, 10, 10});
  const auto s = split_tiles(p);
  REQUIRE(s.size() == 1);
  CHECK(s[0].area == Rect{0, 0, 10, 10});
  CHECK(corner_of(s[0]) == db::TriangleCorner::nw);
  CHECK(raster(p, {-2, -2, 12, 12}) == before);
  // The whole plane: one split tile and the space around it.
  CHECK(db::test::tiles(p).size() == 1);
}

TEST_CASE("merge_split_tiles joins collinear '\\' triangles", MERGE) {
  // The lower-left sides of '\' splits of (0,5)-(5,10) and (5,0)-(10,5):
  // the lower tile's top-left corner is the upper tile's bottom-right one.
  db::Plane p;
  tri(p, {0, 5, 5, 10}, db::TriangleCorner::sw);
  tri(p, {5, 0, 10, 5}, db::TriangleCorner::sw);
  paint(p, {0, 0, 5, 5});
  REQUIRE(split_tiles(p).size() == 2);
  const std::vector<tech::TileType> before = raster(p, {-2, -2, 12, 12});
  p.merge_split_tiles({0, 0, 10, 10});
  const auto s = split_tiles(p);
  REQUIRE(s.size() == 1);
  CHECK(s[0].area == Rect{0, 0, 10, 10});
  CHECK(corner_of(s[0]) == db::TriangleCorner::sw);
  CHECK(raster(p, {-2, -2, 12, 12}) == before);
}

TEST_CASE("merge_split_tiles keeps triangles of different slopes", MERGE) {
  db::Plane p;
  tri(p, {0, 0, 5, 5}, db::TriangleCorner::nw);
  tri(p, {5, 5, 15, 10}, db::TriangleCorner::nw);
  paint(p, {0, 5, 5, 10});
  p.merge_split_tiles(db::whole_plane());
  CHECK(split_tiles(p).size() == 2);
}

TEST_CASE("merge_split_tiles needs both gaps filled", MERGE) {
  SECTION("left gap empty") {
    db::Plane p;
    tri(p, {0, 0, 5, 5}, db::TriangleCorner::nw);
    tri(p, {5, 5, 10, 10}, db::TriangleCorner::nw);
    p.merge_split_tiles(db::whole_plane());
    CHECK(split_tiles(p).size() == 2);
  }
  SECTION("right gap holds another type") {
    db::Plane p = slash_pair();
    paint(p, {5, 0, 10, 5}, kM2);
    p.merge_split_tiles(db::whole_plane());
    CHECK(split_tiles(p).size() == 2);
  }
  SECTION("different split types") {
    db::Plane p;
    tri(p, {0, 0, 5, 5}, db::TriangleCorner::nw, kM2);
    tri(p, {5, 5, 10, 10}, db::TriangleCorner::nw);
    paint(p, {0, 5, 5, 10});
    p.merge_split_tiles(db::whole_plane());
    CHECK(split_tiles(p).size() == 2);
  }
}

TEST_CASE("merge_split_tiles is driven by the upper tile of a pair", MERGE) {
  SECTION("an area over the lower tile only merges nothing") {
    db::Plane p = slash_pair();
    p.merge_split_tiles({0, 0, 5, 5});
    CHECK(split_tiles(p).size() == 2);
  }
  SECTION("an area over the upper tile only merges the pair") {
    db::Plane p = slash_pair();
    p.merge_split_tiles({6, 6, 9, 9});
    REQUIRE(split_tiles(p).size() == 1);
    CHECK(split_tiles(p)[0].area == Rect{0, 0, 10, 10});
  }
  SECTION("an area elsewhere changes nothing") {
    db::Plane p = slash_pair();
    p.merge_split_tiles({20, 20, 30, 30});
    CHECK(split_tiles(p).size() == 2);
  }
}

TEST_CASE("merge_split_tiles repeats along a fractured diagonal", MERGE) {
  // Five '/' pieces of a 20x20 diagonal and the staircase above them: the
  // merges repeat until one triangle is left.
  db::Plane p;
  for (db::Coord i = 0; i < 5; ++i) {
    tri(p, {4 * i, 4 * i, 4 * i + 4, 4 * i + 4}, db::TriangleCorner::nw);
    if (i > 0) {
      paint(p, {0, 4 * i, 4 * i, 4 * i + 4});
    }
  }
  REQUIRE(split_tiles(p).size() == 5);
  const std::vector<tech::TileType> before = raster(p, {-1, -1, 21, 21});
  p.merge_split_tiles(db::whole_plane());
  const auto s = split_tiles(p);
  REQUIRE(s.size() == 1);
  CHECK(s[0].area == Rect{0, 0, 20, 20});
  CHECK(db::test::tiles(p).size() == 1);
  CHECK(raster(p, {-1, -1, 21, 21}) == before);
}

TEST_CASE("the cell paint merges as before", MERGE) {
  // PaintMerge::split_tiles (the default) still merges after each paint.
  db::Plane p;
  const db::TriangleSide nw = db::triangle_side(db::TriangleCorner::nw);
  REQUIRE(p.paint_triangle({0, 0, 5, 5}, nw.diagonal, nw.right_side, paint_row_of(kM1)).ok());
  REQUIRE(p.paint_triangle({5, 5, 10, 10}, nw.diagonal, nw.right_side, paint_row_of(kM1)).ok());
  REQUIRE(p.paint({0, 5, 5, 10}, paint_row_of(kM1)).ok());
  REQUIRE(split_tiles(p).size() == 1);
  CHECK(split_tiles(p)[0].area == Rect{0, 0, 10, 10});
}

TEST_CASE("merge_split_tiles with degenerate and extreme areas", MERGE) {
  SECTION("inverted and empty areas merge nothing") {
    db::Plane p = slash_pair();
    p.merge_split_tiles(Rect{10, 10, 0, 0});
    p.merge_split_tiles(Rect{});
    p.merge_split_tiles(Rect{6, 6, 6, 9});
    CHECK(split_tiles(p).size() == 2);
  }
  SECTION("an area out to the infinities is clamped to the plane") {
    db::Plane p = slash_pair();
    p.merge_split_tiles(Rect{db::kMinusInfinity, db::kMinusInfinity, db::kInfinity, db::kInfinity});
    CHECK(split_tiles(p).size() == 1);
  }
  SECTION("pairs at the corners of the plane") {
    db::Plane p;
    const db::Coord lo = db::kPlaneMin;
    const db::Coord hi = db::kPlaneMax;
    tri(p, {lo, lo, lo + 5, lo + 5}, db::TriangleCorner::nw);
    tri(p, {lo + 5, lo + 5, lo + 10, lo + 10}, db::TriangleCorner::nw);
    paint(p, {lo, lo + 5, lo + 5, lo + 10});
    tri(p, {hi - 10, hi - 5, hi - 5, hi}, db::TriangleCorner::ne);
    tri(p, {hi - 5, hi - 10, hi, hi - 5}, db::TriangleCorner::ne);
    paint(p, {hi - 5, hi - 5, hi, hi});
    REQUIRE(split_tiles(p).size() == 4);
    p.merge_split_tiles(db::whole_plane());
    CHECK(split_areas(p) == std::vector<Rect>{{lo, lo, lo + 10, lo + 10}, {hi - 10, hi - 10, hi, hi}});
  }
}

TEST_CASE("property: merge_split_tiles never changes a point's type", MERGE) {
  // Deterministic pseudo-random triangles and rectangles on a small grid,
  // painted without the merge; merging over the whole plane keeps every
  // unit square's type and never adds split tiles. (The total tile count
  // may grow: a merge cuts the Manhattan gap tiles to the pair's box.)
  std::uint32_t seed = 12345;
  const auto next = [&seed](std::uint32_t n) {
    seed = seed * 1103515245U + 12345U;
    return static_cast<db::Coord>((seed >> 16) % n);
  };
  constexpr db::TriangleCorner kCorners[] = {db::TriangleCorner::nw, db::TriangleCorner::sw,
                                             db::TriangleCorner::se, db::TriangleCorner::ne};
  for (int round = 0; round < 40; ++round) {
    db::Plane p;
    for (int k = 0; k < 12; ++k) {
      const db::Coord x = next(24);
      const db::Coord y = next(24);
      const db::Coord s = 1 + next(8);
      const tech::TileType type = next(3) == 0 ? kM2 : kM1;
      if (next(3) == 0) {
        paint(p, {x, y, x + s, y + 1 + next(8)}, type);
      } else {
        tri(p, {x, y, x + s, y + s}, kCorners[static_cast<std::size_t>(next(4))], type);
      }
    }
    const Rect box{-1, -1, 34, 34};
    const std::vector<tech::TileType> before = raster(p, box);
    const std::size_t splits_before = split_tiles(p).size();
    p.merge_split_tiles(db::whole_plane());
    CHECK(raster(p, box) == before);
    CHECK(split_tiles(p).size() <= splits_before);
  }
}
