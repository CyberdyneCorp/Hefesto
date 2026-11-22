// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::StitchEditor, the restricted split/join header for extresist's
// fracture planes (openspec/changes/m7-extraction design D6, Open Question
// 1 (a); task 3.4; Magic's TiSplitX and TiJoinY1 as ResSplitX uses them,
// resis/ResFract.c:436-488). Invalid edits change nothing.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/db/plane_stitch.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Rect;
using db::StitchTile;
using db::test::paint_row_of;

namespace {

constexpr tech::TileType kM1 = 20;

/// An L: (0,0)-(20,10) with (0,10)-(10,20) on top.
db::Plane l_shape() {
  db::Plane p;
  REQUIRE(p.paint({0, 0, 20, 10}, paint_row_of(kM1)).ok());
  REQUIRE(p.paint({0, 10, 10, 20}, paint_row_of(kM1)).ok());
  REQUIRE(db::test::rects(p) == std::vector<Rect>{{0, 10, 10, 20}, {0, 0, 20, 10}});
  return p;
}

}  // namespace

#define GEO "[cap:layout-geometry-model][req:Geometry data structure is informative]"

TEST_CASE("split_x and join_y fracture an L into vertical strips", GEO) {
  db::Plane p = l_shape();
  {
    db::StitchEditor ed(p);
    const StitchTile bottom = ed.tile_at({0, 0});
    const StitchTile top = ed.tile_at({0, 10});
    REQUIRE(ed.area(bottom) == Rect{0, 0, 20, 10});
    ed.set_client(bottom, 3);
    // ResSplitX: TiSplitX, then the new tile gets the old type.
    const StitchTile right = ed.split_x(bottom, 10);
    REQUIRE(right);
    CHECK(ed.type(right) == tech::kSpace);
    CHECK(ed.client(right) == db::kClientDefault);
    CHECK(ed.client(bottom) == 3);
    ed.set_type(right, kM1);
    CHECK(ed.area(bottom) == Rect{0, 0, 10, 10});
    CHECK(ed.area(right) == Rect{10, 0, 20, 10});
    CHECK(ed.stitch(bottom, db::Stitch::tr) == right);
    CHECK(ed.stitch(bottom, db::Stitch::rt) == top);
    // TiJoinY1 with the tile above: same LEFT and RIGHT now.
    REQUIRE(ed.join_y(bottom, top));
    CHECK(ed.area(bottom) == Rect{0, 0, 10, 20});
    CHECK(ed.client(bottom) == 3);
  }
  CHECK(db::test::rects(p) == std::vector<Rect>{{0, 0, 10, 20}, {10, 0, 20, 10}});
}

TEST_CASE("invalid splits and joins change nothing", GEO) {
  db::Plane p = l_shape();
  const auto before = db::test::tiles(p, tech::TypeMask::all());
  db::StitchEditor ed(p);
  const StitchTile bottom = ed.tile_at({0, 0});
  const StitchTile top = ed.tile_at({0, 10});
  CHECK_FALSE(ed.split_x(bottom, 0));   // at LEFT
  CHECK_FALSE(ed.split_x(bottom, 20));  // at RIGHT
  CHECK_FALSE(ed.split_x(bottom, 25));  // beyond
  CHECK_FALSE(ed.split_x(StitchTile{}, 5));
  CHECK_FALSE(ed.join_y(bottom, top));  // different RIGHT
  CHECK_FALSE(ed.join_y(bottom, bottom));
  CHECK_FALSE(ed.join_y(StitchTile{}, top));
  const StitchTile beside = ed.tile_at({30, 0});  // space to the right, not stacked
  CHECK_FALSE(ed.join_y(bottom, beside));
  CHECK(db::test::tiles(p, tech::TypeMask::all()) == before);
}

TEST_CASE("the boundary tiles are reachable and nothing beyond them", GEO) {
  db::Plane p;
  db::StitchEditor ed(p);
  const StitchTile center = ed.tile_at({0, 0});
  // The central tile reaches the boundary tiles at +-kInfinity (TiNewPlane).
  REQUIRE(ed.area(center) == Rect{db::kPlaneMin, db::kPlaneMin, db::kInfinity, db::kInfinity});
  CHECK_FALSE(ed.boundary(center));
  const StitchTile right = ed.stitch(center, db::Stitch::tr);
  REQUIRE(right);
  CHECK(ed.boundary(right));
  CHECK(ed.type(right) == 0x3fff);  // body -1 masked with TT_LEFTMASK
  CHECK(ed.area(right).x0 == db::kInfinity);
  CHECK_FALSE(ed.stitch(right, db::Stitch::tr));  // the corner tile beyond
  CHECK_FALSE(ed.split_x(right, db::kInfinity + 1));
  ed.set_type(right, kM1);  // ignored
  CHECK(ed.type(right) == 0x3fff);
  std::vector<StitchTile> out{center};
  ed.neighbors(right, db::Side::left, out);
  CHECK(out.empty());
  CHECK(ed.view(right) == db::TileView{});
  const StitchTile left = ed.stitch(center, db::Stitch::bl);
  REQUIRE(ed.boundary(left));
  CHECK_FALSE(ed.stitch(left, db::Stitch::bl));  // a null stitch
}

TEST_CASE("tile_at outside the usable plane is empty", GEO) {
  db::Plane p;
  db::StitchEditor ed(p);
  CHECK_FALSE(ed.tile_at({db::kPlaneMax, 0}));
  CHECK_FALSE(ed.tile_at({db::kInfinity, db::kInfinity}));
  CHECK(ed.tile_at({db::kPlaneMax - 1, 0}));
  // A boundary start falls back to the hint.
  const StitchTile right = ed.stitch(ed.tile_at({0, 0}), db::Stitch::tr);
  CHECK(ed.tile_at({0, 0}, right) == ed.tile_at({0, 0}));
}

TEST_CASE("neighbors and views agree with the plane", GEO) {
  db::Plane p = l_shape();
  db::StitchEditor ed(p);
  const StitchTile bottom = ed.tile_at({0, 0});
  const db::TileView v = ed.view(bottom);
  CHECK(v.area == Rect{0, 0, 20, 10});
  CHECK(v.type == kM1);
  CHECK(db::StitchEditor::handle(v) == bottom);
  std::vector<db::TileView> expected;
  p.neighbors(v, db::Side::top, 0, 20, expected);
  std::vector<StitchTile> got;
  ed.neighbors(bottom, db::Side::top, got);
  REQUIRE(got.size() == expected.size());
  for (std::size_t i = 0; i < got.size(); ++i) {
    CHECK(ed.area(got[i]) == expected[i].area);
  }
  CHECK(ed.hint());
  ed.set_hint(bottom);
  CHECK(ed.hint() == bottom);
}

TEST_CASE("a joined tile stays readable until release", GEO) {
  db::Plane p;
  REQUIRE(p.paint({0, 0, 10, 10}, paint_row_of(kM1)).ok());
  db::StitchEditor ed(p);
  const StitchTile tile = ed.tile_at({0, 0});
  const StitchTile right = ed.split_x(tile, 5);
  ed.set_type(right, kM1);
  // Cut the left half and stack it: split the space above and join.
  const StitchTile above = ed.tile_at({0, 10});
  const StitchTile above_right = ed.split_x(above, 5);
  REQUIRE(above_right);
  const StitchTile above_far = ed.split_x(above_right, 10);
  REQUIRE(above_far);
  REQUIRE(ed.area(above_right) == Rect{5, 10, 10, db::kInfinity});
  REQUIRE(ed.join_y(right, above_right));
  CHECK(ed.area(above_right).x0 == 5);  // still readable
  ed.release();
  CHECK(ed.area(right) == Rect{5, 0, 10, db::kInfinity});
}

TEST_CASE("split tiles are neither split nor joined", GEO) {
  // Regression (review of PR #80): TiSplitX only rewires the stitches, so a
  // diagonal tile cut at x kept its diagonal over the narrower rectangle
  // and the triangle moved.
  db::Plane p;
  REQUIRE(p.paint_triangle({0, 0, 10, 10}, db::Diagonal::ne_sw, false, paint_row_of(kM1), true, nullptr,
                           db::PaintMerge::none)
              .ok());
  REQUIRE(p.paint({0, 10, 10, 20}, paint_row_of(kM1)).ok());
  const auto before = db::test::tiles(p, tech::TypeMask::all());
  db::StitchEditor ed(p);
  const StitchTile split = ed.tile_at({0, 0});
  REQUIRE(ed.view(split).split);
  REQUIRE(ed.area(split) == Rect{0, 0, 10, 10});
  const StitchTile above = ed.tile_at({0, 10});
  REQUIRE(ed.area(above) == Rect{0, 10, 10, 20});
  CHECK_FALSE(ed.join_y(above, split));
  CHECK_FALSE(ed.join_y(split, above));
  CHECK_FALSE(ed.split_x(split, 5));
  ed.release();
  CHECK(db::test::tiles(p, tech::TypeMask::all()) == before);
}
