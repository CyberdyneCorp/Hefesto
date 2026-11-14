// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators: grow and shrink (square and Euclidean), grow-grid
// (Q-MASK-1 in both modes) and grow-min (Q-MASK-15).

#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define CAP "[cap:mask-output-operators]"

namespace {

bool solid_at(const MaskPlane& p, db::Coord x, db::Coord y) { return p.plane().type_at(db::Point{x, y}) == kMaskSolid; }

}  // namespace

TEST_CASE("grow merges, shrink keeps the bridge", CAP "[req:grow and shrink (default, square metric)]") {
  OpsFixture f;
  const Rects cur{{0, 0, 10, 10}, {14, 0, 24, 10}};
  CHECK(f.apply(cur, {op(OpKind::grow, {2})}) == Rects{{-2, -2, 26, 12}});
  CHECK(f.apply(cur, {op(OpKind::grow, {2}), op(OpKind::shrink, {2})}) == Rects{{0, 0, 24, 10}});
}

TEST_CASE("shrink removes features narrower than twice the distance",
          CAP "[req:grow and shrink (default, square metric)]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 3, 30}, {10, 0, 30, 30}}, {op(OpKind::shrink, {2})}) == Rects{{12, 2, 28, 28}});
}

TEST_CASE("grow a 45-degree triangle", CAP "[req:grow and shrink (default, square metric)]") {
  OpsFixture f;
  REQUIRE(db::paint_triangle(*f.cell, *f.tech, Rect{0, 0, 10, 10}, db::Diagonal::nw_se, false, f.m1).ok());
  MaskPlane& p = f.eval({with_types(op(OpKind::op_or), {f.m1}), op(OpKind::grow, {2})});
  // The polygon (-2,-2),(12,-2),(12,2),(2,12),(-2,12): area 146.
  CHECK(p.solid_area() == 146.0);
  CHECK(solid_at(p, -2, -2));
  CHECK(solid_at(p, 11, -2));
  CHECK(solid_at(p, -2, 11));
  CHECK(solid_at(p, 6, 6));
  CHECK_FALSE(solid_at(p, 8, 8));
  CHECK_FALSE(solid_at(p, 12, 0));
}

TEST_CASE("Euclidean grow equals the square grow on Manhattan geometry", CAP "[req:Euclidean grow and shrink]") {
  OpsFixture f;
  f.style.options = 0x08;  // grow-euclidean
  const Rects cur{{0, 0, 10, 10}, {14, 0, 24, 10}};
  CHECK(f.apply(cur, {op(OpKind::grow, {2})}) == Rects{{-2, -2, 26, 12}});
  CHECK(f.apply(cur, {op(OpKind::grow, {2}), op(OpKind::shrink, {2})}) == Rects{{0, 0, 24, 10}});
  CHECK(f.apply({{0, 0, 10, 10}, {10, 0, 30, 4}}, {op(OpKind::grow, {3})}) ==
        strips({{-3, -3, 13, 13}, {10, -3, 33, 7}}));
}

TEST_CASE("Euclidean grow moves a 45-degree edge by the on-grid offset", CAP "[req:Euclidean grow and shrink]") {
  OpsFixture f;
  f.style.options = 0x08;
  REQUIRE(db::paint_triangle(*f.cell, *f.tech, Rect{0, 0, 10, 10}, db::Diagonal::nw_se, false, f.m1).ok());
  MaskPlane& p = f.eval({with_types(op(OpKind::op_or), {f.m1}), op(OpKind::grow, {10})});
  // dx = dy = ceil(10 / (1 + sqrt 2)) = 5. Magic's construction: the legs
  // (south and west) move by 10 with their corner; the leg rectangles
  // [0,5]x[0,20] and [0,20]x[0,5]; the triangle from (5,5) to (20,5) and
  // (5,20), so the hypotenuse x + y = 10 becomes x + y = 25.
  CHECK(euclidean_offsets(10, 10, 10).dx == 5);
  CHECK(euclidean_offsets(10, 10, 10).dy == 5);
  CHECK(solid_at(p, -10, -10));
  CHECK(solid_at(p, 9, -10));
  CHECK_FALSE(solid_at(p, 10, -10));
  CHECK(solid_at(p, 19, 0));
  CHECK_FALSE(solid_at(p, 20, 0));
  CHECK(solid_at(p, 0, 19));
  CHECK_FALSE(solid_at(p, -1, 19));
  CHECK(solid_at(p, 11, 11));
  CHECK_FALSE(solid_at(p, 13, 13));
  CHECK(p.solid_area() == 20.0 * 10 + 10 * 10 + 5 * 20 + 15 * 5 + 15.0 * 15 / 2 - 0.0);
}

TEST_CASE("grow-grid with positive coordinates (magic-compat ON)", CAP "[req:grow-grid]") {
  OpsFixture f;
  CHECK(f.apply({{3, 3, 12, 12}}, {op(OpKind::grow_grid, {5})}) == Rects{{0, 0, 14, 14}});
}

TEST_CASE("grow-grid with negative coordinates (magic-compat ON)", CAP "[req:grow-grid]") {
  OpsFixture f;
  CHECK(f.apply({{-7, 0, -3, 5}}, {op(OpKind::grow_grid, {5})}) == Rects{{-9, 0, 0, 5}});
}

TEST_CASE("grow-grid rounds outward with magic-compat OFF", CAP "[req:grow-grid]") {
  OpsFixture f;
  f.compat(false);
  CHECK(f.apply({{-7, 3, 12, 12}}, {op(OpKind::grow_grid, {5})}) == Rects{{-10, 0, 15, 15}});
  f.compat(true);
  CHECK(f.apply({{-7, 3, 12, 12}}, {op(OpKind::grow_grid, {5})}) == Rects{{-9, 0, 14, 14}});
}

TEST_CASE("Q-MASK-1 helper: sides at the plane's edge stay in both modes", CAP "[req:grow-grid]") {
  compat::CompatSettings on;
  compat::CompatSettings off;
  off.magic_compat = false;
  const Rect r{db::kPlaneMin, -7, 12, db::kPlaneMax};
  CHECK(q_mask_1_grow_grid(r, 5, on) == Rect{db::kPlaneMin, -9, 14, db::kPlaneMax});
  CHECK(q_mask_1_grow_grid(r, 5, off) == Rect{db::kPlaneMin, -10, 15, db::kPlaneMax});
}

TEST_CASE("grow-min widens a narrow isolated line", CAP "[req:grow-min]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 4, 100}}, {op(OpKind::grow_min, {10})}) == Rects{{-3, 0, 7, 100}});
}

TEST_CASE("grow-min leaves wide features and grows a small square in both directions", CAP "[req:grow-min]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 20, 20}}, {op(OpKind::grow_min, {10})}) == Rects{{0, 0, 20, 20}});
  // A 4x4 square: widened by 3 each side, then heightened by 3 each side.
  CHECK(f.apply({{0, 0, 4, 4}}, {op(OpKind::grow_min, {10})}) == Rects{{-3, -3, 7, 7}});
}

TEST_CASE("grow-min extends a short stub away from the solid below", CAP "[req:grow-min]") {
  OpsFixture f;
  // A 4-wide, 4-tall stub on top of a wide bar: solid below, free above, so
  // the stub's rectangle grows downward by h = (int) sqrt(100 - 0.25*196 + 0.5) = 7.
  const Rects out = f.apply({{0, 0, 40, 10}, {10, 10, 14, 14}}, {op(OpKind::grow_min, {10})});
  CHECK(grow_min_height(10, 4) == 7);
  MaskPlane check;
  for (const Rect& r : out) {
    REQUIRE(check.paint(r).ok());
  }
  CHECK(solid_at(check, 7, 13));  // widened to [7,17]
  CHECK(solid_at(check, 16, 13));
  CHECK_FALSE(solid_at(check, 17, 13));
}
