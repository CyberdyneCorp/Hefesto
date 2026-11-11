// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Plane::tile_at() and Plane::neighbors(), the stitch walks behind the DRC
// kernel's corner probes and edge walks (openspec/changes/m3-drc design
// D16, the DRC speed work): every result must equal what an area
// enumeration of the same unit square or one-unit strip reports first
// (layout-geometry-model "Tile enumeration order"), split tiles included,
// whatever tile the walk starts from. Regression test of the walk rewrite.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>
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

/// What the kernel did before the walks: the first view of an area search.
db::TileView reference_at(const db::Plane& p, db::Point pt) {
  db::TileView found;
  p.enumerate(Rect{pt.x, pt.y, pt.x + 1, pt.y + 1}, tech::TypeMask::all(), [&](const db::TileView& t) {
    found = t;
    return false;
  });
  return found;
}

Rect strip_of(const Rect& r, db::Side side, db::Coord lo, db::Coord hi) {
  switch (side) {
    case db::Side::left:
      return Rect{r.x0 - 1, lo, r.x0, hi};
    case db::Side::right:
      return Rect{r.x1, lo, r.x1 + 1, hi};
    case db::Side::bottom:
      return Rect{lo, r.y0 - 1, hi, r.y0};
    case db::Side::top:
      break;
  }
  return Rect{lo, r.y1, hi, r.y1 + 1};
}

/// The distinct tiles of the strip, in enumeration order, sorted along it.
std::vector<db::TileView> reference_neighbors(const db::Plane& p, const db::TileView& t, db::Side side, db::Coord lo,
                                              db::Coord hi) {
  std::vector<db::TileView> out;
  if (lo >= hi) {
    return out;
  }
  p.enumerate(strip_of(t.area, side, lo, hi), tech::TypeMask::all(), [&](const db::TileView& n) {
    if (out.empty() || !(out.back().area == n.area)) {
      out.push_back(n);
    }
    return true;
  });
  const bool vertical = side == db::Side::left || side == db::Side::right;
  std::stable_sort(out.begin(), out.end(), [&](const db::TileView& a, const db::TileView& b) {
    return vertical ? a.area.y0 < b.area.y0 : a.area.x0 < b.area.x0;
  });
  return out;
}

/// A plane of random rectangles and triangles of two types in [0, 64)^2.
db::Plane random_plane(std::mt19937& rng) {
  db::Plane p;
  std::uniform_int_distribution<int> coord(0, 60);
  std::uniform_int_distribution<int> size(1, 12);
  std::uniform_int_distribution<int> kind(0, 5);
  for (int i = 0; i < 30; ++i) {
    const int x = coord(rng);
    const int y = coord(rng);
    const Rect r{x, y, x + size(rng), y + size(rng)};
    const tech::TileType type = (i % 2) == 0 ? kM1 : kM2;
    const int k = kind(rng);
    if (k < 4) {
      const db::TriangleSide side = db::triangle_side(static_cast<db::TriangleCorner>(k));
      REQUIRE(p.paint_triangle(r, side.diagonal, side.right_side, paint_row_of(type)).ok());
    } else {
      REQUIRE(p.paint(r, paint_row_of(type)).ok());
    }
  }
  return p;
}

std::vector<db::TileView> all_tiles(const db::Plane& p) {
  return db::test::tiles(p, tech::TypeMask::all());
}

}  // namespace

#define WALK "[cap:layout-geometry-model][req:Tile enumeration order]"

TEST_CASE("tile_at reports the tile an enumeration of the unit square reports first", WALK) {
  std::mt19937 rng(20261111);
  for (int round = 0; round < 20; ++round) {
    const db::Plane p = random_plane(rng);
    const std::vector<db::TileView> tiles = all_tiles(p);
    REQUIRE(std::any_of(tiles.begin(), tiles.end(), [](const db::TileView& t) { return t.split; }));
    std::uniform_int_distribution<int> coord(-4, 76);
    std::uniform_int_distribution<std::size_t> pick(0, tiles.size() - 1);
    for (int i = 0; i < 300; ++i) {
      const db::Point pt{coord(rng), coord(rng)};
      const db::TileView& near = tiles[pick(rng)];
      const db::TileView expected = reference_at(p, pt);
      CHECK(p.tile_at(pt, &near) == expected);
      CHECK(p.tile_at(pt) == expected);
    }
  }
}

TEST_CASE("tile_at falls back to the search at the plane's limits", WALK) {
  db::Plane p;
  REQUIRE(p.paint({0, 0, 10, 10}, paint_row_of(kM1)).ok());
  const Rect plane = db::whole_plane();
  for (const db::Point pt : {db::Point{plane.x0, 5}, db::Point{plane.x1 - 1, 5}, db::Point{plane.x1, 5},
                             db::Point{5, plane.y1}, db::Point{plane.x0 - 1, plane.y0 - 1}}) {
    CHECK(p.tile_at(pt) == reference_at(p, pt));
  }
}

TEST_CASE("neighbors walks the stitches of each side as the strip search reports them", WALK) {
  std::mt19937 rng(11112026);
  std::vector<db::TileView> got;
  for (int round = 0; round < 20; ++round) {
    const db::Plane p = random_plane(rng);
    for (const db::TileView& t : all_tiles(p)) {
      for (const db::Side side : {db::Side::left, db::Side::right, db::Side::bottom, db::Side::top}) {
        const bool vertical = side == db::Side::left || side == db::Side::right;
        const db::Coord lo = vertical ? t.area.y0 : t.area.x0;
        const db::Coord hi = vertical ? t.area.y1 : t.area.x1;
        // The whole side, then a random part of it (clamped to the test area
        // for the space tiles that reach the plane's limits).
        const db::Coord a = std::max(lo, -8);
        const db::Coord b = std::min(hi, 80);
        std::uniform_int_distribution<db::Coord> in(a, b);
        db::Coord sub_lo = in(rng);
        db::Coord sub_hi = in(rng);
        if (sub_lo > sub_hi) {
          std::swap(sub_lo, sub_hi);
        }
        for (const auto& [l, h] : {std::pair{lo, hi}, std::pair{sub_lo, sub_hi}, std::pair{sub_hi, sub_lo}}) {
          p.neighbors(t, side, l, h, got);
          CHECK(got == reference_neighbors(p, t, side, l, h));
        }
      }
    }
  }
}

TEST_CASE("neighbors of a view without a tile handle searches the strip", WALK) {
  db::Plane p;
  REQUIRE(p.paint({0, 0, 10, 10}, paint_row_of(kM1)).ok());
  REQUIRE(p.paint({-5, 2, 0, 4}, paint_row_of(kM2)).ok());
  db::TileView t = db::test::tiles(p, tech::TypeMask::of(kM1)).front();
  t.tile = nullptr;
  std::vector<db::TileView> got;
  p.neighbors(t, db::Side::left, 0, 10, got);
  CHECK(got == reference_neighbors(p, t, db::Side::left, 0, 10));
  REQUIRE(got.size() == 3);
  CHECK(got[1].type == kM2);
}
