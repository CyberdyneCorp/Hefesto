// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators: the contact strip pre-pass, squares (Q-MASK-27 in
// both modes), squares-grid and slots.

#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define CAP "[cap:mask-output-operators]"

TEST_CASE("guard-ring strip", CAP "[req:Contact strip pre-pass for squares and slots]") {
  OpsFixture f;
  const Rects cuts = f.apply({{0, 0, 200, 30}}, {op(OpKind::squares, {5, 20, 20})});
  CHECK(cuts == strips({{10, 5, 30, 25}, {50, 5, 70, 25}, {90, 5, 110, 25}, {130, 5, 150, 25}, {170, 5, 190, 25}}));
}

TEST_CASE("a ring is processed as strips, each with its own cuts",
          CAP "[req:Contact strip pre-pass for squares and slots]") {
  OpsFixture f;
  // A 30-wide square ring: two horizontal strips (top and bottom rows) and
  // two vertical ones; every strip gets cuts.
  const Rects ring{{0, 0, 130, 30}, {0, 100, 130, 130}, {0, 30, 30, 100}, {100, 30, 130, 100}};
  const Rects cuts = f.apply(ring, {op(OpKind::squares, {5, 20, 20})});
  MaskPlane p;
  for (const Rect& r : cuts) {
    REQUIRE(p.paint(r).ok());
  }
  CHECK_FALSE(cuts.empty());
  for (const Rect& r : cuts) {
    CHECK(r.width() == 20);
    CHECK(r.height() == 20);
  }
  // No cut lands in the ring's hole.
  p.clip(Rect{30, 30, 100, 100});
  CHECK(p.empty());
}

TEST_CASE("a strip exactly one cut in size becomes the cut; a too small one is an error",
          CAP "[req:Contact strip pre-pass for squares and slots]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 20, 20}}, {op(OpKind::squares, {0, 20, 20})}) == Rects{{0, 0, 20, 20}});
  CHECK(f.sink.take().empty());
  // 25 wide with border 5: S = 30 > 25, so no strip; the region pass finds
  // no room.
  CHECK(f.apply({{0, 0, 25, 25}}, {op(OpKind::squares, {5, 20, 20})}).empty());
  const auto diags = f.sink.take();
  REQUIRE(diags.size() == 1);
  CHECK(diags.front().code == "MASK-OPER-004");
}

TEST_CASE("2x2 array centred", CAP "[req:squares]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 90, 90}}, {op(OpKind::squares, {5, 20, 20})}) ==
        strips({{15, 15, 35, 35}, {55, 15, 75, 35}, {15, 55, 35, 75}, {55, 55, 75, 75}}));
}

TEST_CASE("grid-limit shift", CAP "[req:squares]") {
  OpsFixture f;
  f.style.grid_limit = 5;
  // A strip: cols = 2, cx = 16, delta 1, x2 becomes 90, cx = 15.
  const Rects cuts = f.apply({{0, 0, 92, 30}}, {op(OpKind::squares, {5, 20, 20})});
  CHECK(cuts == strips({{15, 5, 35, 25}, {55, 5, 75, 25}}));
}

TEST_CASE("grid-limit shift at negative coordinates (Q-MASK-27)", CAP "[req:squares]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.style.grid_limit = 5;
    const Rects cuts = f.apply({{-92, 0, 0, 30}}, {op(OpKind::squares, {5, 20, 20})});
    if (on) {
      // x2 becomes -2: cx = -77, off the grid again, and Magic keeps
      // shrinking (-6, -14, ...) until no column is left: the strip loses
      // its cuts and is reported.
      CHECK(cuts.empty());
      CHECK(f.sink.take().front().code == "MASK-OPER-002");
    } else {
      // x2 becomes 2: cx = -75, on the grid.
      CHECK(cuts == strips({{-75, 5, -55, 25}, {-35, 5, -15, 25}}));
    }
  }
}

TEST_CASE("Q-MASK-27 helper", CAP "[req:squares]") {
  compat::CompatSettings on;
  compat::CompatSettings off;
  off.magic_compat = false;
  CHECK(q_mask_27_shift(0, -76, 1, on) == -2);
  CHECK(q_mask_27_shift(0, -76, 1, off) == 2);
  CHECK(q_mask_27_shift(92, 16, 1, off) == 90);
}

TEST_CASE("squares keeps only cuts whose border lies inside the region", CAP "[req:squares]") {
  OpsFixture f;
  // An L-shaped region too wide for strips: the cut array is laid on the
  // bounding box and the cut in the notch is dropped.
  // The lower arm's right part [45,90]x[0,45] is a strip (shortened by
  // sep - 2*border = 10 at its left end, which touches the rest); the
  // remaining [0,45]x[0,90] is one region.
  const Rects cuts = f.apply({{0, 0, 90, 45}, {0, 45, 45, 90}}, {op(OpKind::squares, {5, 20, 20})});
  CHECK(cuts == strips({{62, 12, 82, 32}, {12, 15, 32, 35}, {12, 55, 32, 75}}));
}

TEST_CASE("grid-aligned cuts", CAP "[req:squares-grid]") {
  OpsFixture f;
  // L.x = [5,88], left = 10, cols = 2, right = 70, mx = 0: first cut x = 10
  // (y grid 5: bottom 5, one row).
  const Rects cuts = f.apply({{0, 0, 93, 30}}, {op(OpKind::squares_grid, {5, 20, 20, 10, 5})});
  CHECK(cuts == strips({{10, 5, 30, 25}, {50, 5, 70, 25}}));
  // With the y grid 10 too there is no room for a row in the strip.
  CHECK(f.apply({{0, 0, 93, 30}}, {op(OpKind::squares_grid, {5, 20, 20, 10})}).empty());
  CHECK(f.sink.take().front().code == "MASK-OPER-002");
}

TEST_CASE("squares-grid with separate x and y grids and negative coordinates", CAP "[req:squares-grid]") {
  OpsFixture f;
  const Rects cuts = f.apply({{-93, -93, 0, 0}}, {op(OpKind::squares_grid, {5, 20, 20, 10, 5})});
  REQUIRE_FALSE(cuts.empty());
  for (const Rect& r : cuts) {
    CHECK(r.x0 % 10 == 0);
    CHECK(r.y0 % 5 == 0);
    CHECK(r.x0 >= -88);
    CHECK(r.x1 <= -5);
  }
}

TEST_CASE("stripe slots", CAP "[req:slots]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 100, 10}}, {op(OpKind::slots, {2, 4, 4})}) == Rects{{0, 3, 100, 7}});
}

TEST_CASE("slots with long-side sizes in a region", CAP "[req:slots]") {
  OpsFixture f;
  // A 40x100 region (vertical): short side across x, long side along y.
  const Rects cuts = f.apply({{0, 0, 40, 100}}, {op(OpKind::slots, {5, 10, 10, 5, 30, 10})});
  CHECK_FALSE(cuts.empty());
  for (const Rect& r : cuts) {
    CHECK(r.width() == 10);
    CHECK(r.height() == 30);
    CHECK(r.x0 >= 5);
    CHECK(r.x1 <= 35);
    CHECK(r.y0 >= 5);
    CHECK(r.y1 <= 95);
  }
}
