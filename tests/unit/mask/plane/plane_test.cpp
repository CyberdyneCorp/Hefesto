// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators "Geometric model of mask planes": the mask plane
// (CIF paint and erase tables, canonical strips, AND-NOT, OR, clipping) and
// the scratch pool.

#include "hefesto/mask/plane/plane.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "hefesto/hints/region.hpp"

using namespace hefesto;
using db::Rect;
using mask::MaskPlane;
using Rects = std::vector<Rect>;

#define MODEL "[cap:mask-output-operators][req:Geometric model of mask planes]"

TEST_CASE("the CIF paint table paints the mask type over anything; the erase table space", MODEL) {
  for (const tech::TileType t : {tech::TileType{0}, tech::TileType{1}, tech::TileType{9}, tech::TileType{255}}) {
    CHECK(mask::cif_paint_table()[t] == mask::kMaskSolid);
    CHECK(mask::cif_erase_table()[t] == tech::kSpace);
  }
}

TEST_CASE("paint and erase keep the canonical maximal-horizontal-strip decomposition", MODEL) {
  MaskPlane p;
  REQUIRE(p.paint(Rect{0, 0, 10, 10}).ok());
  REQUIRE(p.paint(Rect{5, 5, 15, 15}).ok());
  CHECK(p.solid_rects() == Rects{{5, 10, 15, 15}, {0, 5, 15, 10}, {0, 0, 10, 5}});
  REQUIRE(p.erase(Rect{5, 5, 15, 15}).ok());
  CHECK(p.solid_rects() == Rects{{0, 5, 5, 10}, {0, 0, 10, 5}});
  REQUIRE(p.paint(Rect{5, 5, 10, 10}).ok());
  CHECK(p.solid_rects() == Rects{{0, 0, 10, 10}});  // erase after paint restores
  // A rectangle without area changes nothing.
  REQUIRE(p.paint(Rect{20, 20, 20, 30}).ok());
  CHECK(p.solid_rects() == Rects{{0, 0, 10, 10}});
}

TEST_CASE("triangles are painted and erased exactly", MODEL) {
  MaskPlane p;
  REQUIRE(p.paint_triangle(Rect{0, 0, 10, 10}, db::Diagonal::nw_se, false).ok());
  CHECK(p.solid_area() == 50.0);
  REQUIRE(p.paint_triangle(Rect{0, 0, 10, 10}, db::Diagonal::nw_se, true).ok());
  CHECK(p.solid_rects() == Rects{{0, 0, 10, 10}});
  REQUIRE(p.erase_triangle(Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, false).ok());
  CHECK(p.solid_area() == 50.0);
}

TEST_CASE("subtract (AND-NOT) and unite (OR) of mask planes", MODEL) {
  MaskPlane a;
  MaskPlane b;
  REQUIRE(a.paint(Rect{0, 0, 100, 100}).ok());
  REQUIRE(b.paint(Rect{40, -10, 60, 200}).ok());
  MaskPlane u;
  REQUIRE(u.unite(a).ok());
  REQUIRE(u.unite(b).ok());
  REQUIRE(a.subtract(b).ok());
  CHECK(a.solid_rects() == Rects{{0, 0, 40, 100}, {60, 0, 100, 100}});
  CHECK(u.solid_area() == 100.0 * 100 + 20 * 110);
  REQUIRE(u.subtract(u).ok());
  CHECK(u.empty());
}

TEST_CASE("clip erases everything outside the clip area", MODEL) {
  MaskPlane p;
  REQUIRE(p.paint(Rect{-50, -50, 50, 50}).ok());
  p.clip(Rect{0, 0, 10, 100});
  CHECK(p.solid_rects() == Rects{{0, 0, 10, 50}});
  p.clip(db::whole_plane());
  CHECK(p.solid_rects() == Rects{{0, 0, 10, 50}});
}

TEST_CASE("paint reaching the plane's edge stops one unit inside it", MODEL) {
  MaskPlane p;
  REQUIRE(p.paint(db::whole_plane()).ok());
  REQUIRE(p.erase(Rect{db::kPlaneMin, db::kPlaneMin, db::kPlaneMax, 0}).ok());
  REQUIRE(p.erase(Rect{db::kPlaneMin, 10, db::kPlaneMax, db::kPlaneMax}).ok());
  REQUIRE(p.erase(Rect{db::kPlaneMin, 0, 0, 10}).ok());
  REQUIRE(p.erase(Rect{5, 0, db::kPlaneMax, 10}).ok());
  CHECK(p.solid_rects() == Rects{{0, 0, 5, 10}});
  p.clip(Rect{1, 1, 4, 4});
  CHECK(p.solid_rects() == Rects{{1, 1, 4, 4}});
}

TEST_CASE("copy and swap", MODEL) {
  MaskPlane a;
  MaskPlane b;
  REQUIRE(a.paint(Rect{0, 0, 5, 5}).ok());
  b.copy_from(a);
  CHECK(b.solid_rects() == a.solid_rects());
  REQUIRE(b.paint(Rect{10, 0, 15, 5}).ok());
  a.swap(b);
  CHECK(a.solid_rects() == Rects{{0, 0, 5, 5}, {10, 0, 15, 5}});
  CHECK(b.solid_rects() == Rects{{0, 0, 5, 5}});
}

TEST_CASE("the pool reuses cleared planes and scratch planes return to it", MODEL) {
  mask::PlanePool pool;
  {
    mask::ScratchPlane s{pool};
    REQUIRE(s->paint(Rect{0, 0, 1, 1}).ok());
    CHECK_FALSE(s->empty());
  }
  CHECK(pool.size() == 1);
  MaskPlane& again = pool.acquire();
  CHECK(again.empty());
  CHECK(pool.size() == 1);
  MaskPlane& other = pool.acquire();
  CHECK(&other != &again);
  CHECK(pool.size() == 2);
}

TEST_CASE("hint regions are painted with the mask plane's CIF table", MODEL) {
  // The table moved from hints/region to mask/plane; a region's strips are
  // exactly a mask plane's.
  const Rects rects{{0, 0, 10, 10}, {5, 5, 15, 15}};
  const hints::HintRegion region{rects[0], rects[1]};
  MaskPlane p;
  for (const Rect& r : rects) {
    REQUIRE(p.paint(r).ok());
  }
  CHECK(region.strips() == p.solid_rects());
}
