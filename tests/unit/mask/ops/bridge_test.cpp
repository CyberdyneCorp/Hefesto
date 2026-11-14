// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators: bridge (Q-MASK-3, Q-MASK-227) and bridge-lim
// (Q-MASK-3, 5, 228, 229 in both modes).

#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define CAP "[cap:mask-output-operators]"

namespace {

Rects with(Rects base, std::initializer_list<Rect> extra) {
  base.insert(base.end(), extra.begin(), extra.end());
  return strips(base);
}

/// `base` minus `cut` as canonical strips.
Rects minus(const Rects& base, const Rects& cut) {
  MaskPlane p;
  for (const Rect& r : base) {
    REQUIRE(p.paint(r).ok());
  }
  for (const Rect& r : cut) {
    REQUIRE(p.erase(r).ok());
  }
  return p.solid_rects();
}

const Rects kCatecorner{{0, 0, 10, 10}, {12, 12, 22, 22}};

}  // namespace

TEST_CASE("catecorner squares", CAP "[req:bridge]") {
  OpsFixture f;
  CHECK(f.apply(kCatecorner, {op(OpKind::bridge, {5, 4})}) == with(kCatecorner, {{6, 9, 16, 13}}));
}

TEST_CASE("bridge snapped to the grid limit", CAP "[req:bridge]") {
  OpsFixture f;
  f.style.grid_limit = 5;
  CHECK(f.apply(kCatecorner, {op(OpKind::bridge, {5, 4})}) == with(kCatecorner, {{5, 5, 20, 15}}));
}

TEST_CASE("pinch between overlapping corners", CAP "[req:bridge]") {
  OpsFixture f;
  const Rects cur{{0, 0, 10, 10}, {9, 9, 20, 20}};
  CHECK(f.apply(cur, {op(OpKind::bridge, {5, 4})}) == with(cur, {{7, 7, 12, 12}}));
}

TEST_CASE("gap at or beyond the spacing distance", CAP "[req:bridge]") {
  OpsFixture f;
  const Rects cur{{0, 0, 10, 10}, {15, 15, 25, 25}};
  CHECK(f.apply(cur, {op(OpKind::bridge, {5, 4})}) == strips(cur));
}

TEST_CASE("the pass-1 SE corner's bottom neighbour (Q-MASK-3)", CAP "[req:bridge]") {
  // T = [0,10]x[10,20] sits on [0,5]x[0,10]; V = [12,22]x[0,8] is SE of T.
  // T's bottom-left neighbour is solid, its bottom-right one space.
  const Rects cur{{0, 10, 10, 20}, {0, 0, 5, 10}, {12, 0, 22, 8}};
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    const Rects out = f.apply(cur, {op(OpKind::bridge, {5, 4})});
    if (on) {
      CHECK(out == strips(cur));  // Magic tests the bottom-left neighbour: no corner
    } else {
      CHECK(out == with(cur, {{6, 7, 16, 11}}));
    }
  }
}

TEST_CASE("bridge expansion with a gap wider than the width (Q-MASK-227 clamp)", CAP "[req:bridge]") {
  // W = 8 > w = 4 along the long axis: dx = ceil(-2) = -2 narrows the
  // bridge to w; sqrtf(16 - 64) would be NaN, clamped to 0 it gives
  // dy2 = ceil(-0.5) = 0, so dy = ceil(sqrt(12)) = 4 on every platform.
  CHECK(bridge_expand(Rect{10, 10, 18, 11}, 4, false, 0) == Rect{12, 6, 16, 15});
}

TEST_CASE("narrow tile widened", CAP "[req:bridge-lim]") {
  OpsFixture f;
  const std::size_t l = f.mask_layer({});
  CHECK(f.apply({{0, 0, 2, 20}}, {with_layers(op(OpKind::bridge_lim, {4, 6}), {l})}) == Rects{{-4, 0, 2, 20}});
}

TEST_CASE("catecorner gap without limits", CAP "[req:bridge-lim]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    const std::size_t l = f.mask_layer({});
    CHECK(f.apply(kCatecorner, {with_layers(op(OpKind::bridge_lim, {5, 4}), {l})}) ==
          with(kCatecorner, {{10, 6, 16, 10}, {12, 6, 16, 12}}));
  }
}

TEST_CASE("first candidate blocked by a limit", CAP "[req:bridge-lim]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    const std::size_t l = f.mask_layer({{13, 0, 30, 8}});
    CHECK(f.apply(kCatecorner, {with_layers(op(OpKind::bridge_lim, {5, 4}), {l})}) ==
          with(kCatecorner, {{6, 12, 12, 16}, {6, 10, 10, 16}}));
  }
}

TEST_CASE("all candidates blocked: the limits are erased (Q-MASK-229)", CAP "[req:bridge-lim]") {
  const Rects limits{{13, 0, 30, 8}, {6, 15, 14, 30}};
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    const std::size_t l = f.mask_layer(limits);
    const Rects out = f.apply(kCatecorner, {with_layers(op(OpKind::bridge_lim, {5, 4}), {l})});
    const Rects painted = with(kCatecorner, {{6, 6, 16, 16}});
    if (on) {
      // Pass 1 paints c3 and erases the whole limiting tiles, with
      // [12,14]x[15,22] of the square. Pass 2 then scans that plane (Magic
      // paints the plane it enumerates) and adds its own bridges, so only
      // pass 1's effects are checked exactly.
      MaskPlane got;
      for (const Rect& r : out) {
        REQUIRE(got.paint(r).ok());
      }
      for (const Rect& r : minus(painted, limits)) {
        MaskPlane missing;
        REQUIRE(missing.paint(r).ok());
        REQUIRE(missing.subtract(got).ok());
        CHECK(missing.empty());
      }
      for (const Rect& lim : {Rect{13, 0, 30, 8}, Rect{12, 15, 14, 22}}) {
        MaskPlane inside;
        REQUIRE(inside.unite(got).ok());
        inside.clip(lim);
        CHECK(inside.empty());
      }
    } else {
      CHECK(out == minus(painted, {{13, 6, 16, 8}, {6, 15, 14, 16}}));
    }
  }
}

TEST_CASE("bridge-lim limit searches on layout planes use mask units with magic-compat ON (Q-MASK-5)",
          CAP "[req:bridge-lim]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.scale(10);
    f.paint(f.poly, Rect{-30, 0, -20, 20});  // at -300..-200 in mask units
    const Rects out = f.apply({{0, 0, 20, 200}}, {with_types(op(OpKind::bridge_lim, {40, 60}), {f.poly})});
    if (on) {
      // [-40,20] read as internal units overlaps the poly tile (right edge -20).
      CHECK(out == Rects{{-20, 0, 40, 200}});
    } else {
      CHECK(out == Rects{{-40, 0, 20, 200}});
    }
  }
}

TEST_CASE("Q-MASK-5 and Q-MASK-229 helpers", CAP "[req:bridge-lim]") {
  compat::CompatSettings on;
  compat::CompatSettings off;
  off.magic_compat = false;
  CHECK(q_mask_5_limit_search_area(Rect{-41, 5, 21, 200}, 10, on) == Rect{-41, 5, 21, 200});
  CHECK(q_mask_5_limit_search_area(Rect{-41, 5, 21, 200}, 10, off) == Rect{-5, 0, 3, 20});
  CHECK(q_mask_5_tile_edge(-2, 10, on) == -2);
  CHECK(q_mask_5_tile_edge(-2, 10, off) == -20);
  CHECK_FALSE(q_mask_229_clip_erase(on));
  CHECK(q_mask_229_clip_erase(off));
  CHECK_FALSE(q_mask_228_snapshot(on));
  CHECK(q_mask_228_snapshot(off));
}
