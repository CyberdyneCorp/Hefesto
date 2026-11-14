// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Array interactions (mask-hierarchical-generation "Array interaction
// geometry parameters", "Array interaction regions", "Array interaction
// correction", "Array correction replication", "Hierarchical layer
// preview"), with both magic-compat settings for Q-MASK-406.
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "gen_fixture.hpp"

namespace hefesto::mask::test {

namespace {

// The spec's 1-D array: a child with bbox (0,0)-(10,20) and m1
// (0,5)-(10,15), arrayed x 0..3 at 12, under `or m1; grow 10; shrink 10`
// with S = 10 (r = 3).
struct RowLayout {
  GenFixture& f;
  std::size_t layer;
  db::CellDef& child;
  db::CellDef& top;

  explicit RowLayout(GenFixture& fx, db::ArrayInfo array = db::ArrayInfo{0, 3, 0, 0, 12, 0})
      : f(fx),
        layer(fx.layer("M", {types_op(OpKind::op_or, {fx.m1}), op(OpKind::grow, 10), op(OpKind::shrink, 10)})),
        child(fx.cell("child")),
        top(fx.cell("top")) {
    f.paint(child, f.m1, Rect{0, 5, 10, 15});
    f.paint(child, f.m2, Rect{0, 0, 10, 20});
    f.place(top, child, db::kIdentity, array);
  }
};

const Rects kSeams{Rect{100, 50, 120, 150}, Rect{220, 50, 240, 150}, Rect{340, 50, 360, 150}};

// The union of every element's own output (in parent coordinates) and the
// parent's output equals the flat output: the array corrections fill the
// seams exactly.
Rects assembled(GenFixture& f, const db::CellDef& top, const db::CellUse& use, std::size_t layer) {
  const LayerPlanes child = f.hierarchical(*use.def);
  const LayerPlanes parent = f.hierarchical(top);
  MaskPlane all;
  const std::int32_t s = f.style.scale.scale_factor;
  for (std::int32_t y = std::min(use.array.ylo, use.array.yhi); y <= std::max(use.array.ylo, use.array.yhi); ++y) {
    for (std::int32_t x = std::min(use.array.xlo, use.array.xhi); x <= std::max(use.array.xlo, use.array.xhi); ++x) {
      const db::Transform t = db::element_transform(use, x, y);
      for (const Rect& r : child[layer]->solid_rects()) {
        const Rect internal{r.x0 / s, r.y0 / s, r.x1 / s, r.y1 / s};
        const Rect moved = db::apply(t, internal);
        REQUIRE(all.paint(Rect{moved.x0 * s, moved.y0 * s, moved.x1 * s, moved.y1 * s}).ok());
      }
    }
  }
  REQUIRE(all.unite(*parent[layer]).ok());
  return all.solid_rects();
}

Rects flat(GenFixture& f, const db::CellDef& top, std::size_t layer) {
  const LayerPlanes planes = generate_flat(f.source(), top, top.bbox(), f.style, f.ctx);
  return GenFixture::rects(planes, layer);
}

}  // namespace

TEST_CASE("Seams of a 1-D array are replicated along the row",
          "[cap:mask-hierarchical-generation][req:Array interaction correction]"
          "[cap:mask-hierarchical-generation][req:Array correction replication]") {
  for (const bool on : {true, false}) {
    GenFixture f(on);
    RowLayout row(f);
    REQUIRE(f.style.halo() == 3);
    CHECK(row.top.bbox() == Rect{0, 0, 46, 20});
    const LayerPlanes out = f.hierarchical(row.top);
    CHECK(GenFixture::rects(out, row.layer) == kSeams);
    CHECK(f.codes().empty());
  }
}

TEST_CASE("cif *array shows only the array contribution",
          "[cap:mask-hierarchical-generation][req:Hierarchical layer preview]") {
  GenFixture f;
  RowLayout row(f);
  auto tiles = see_hierarchical(f.source(), f.style, row.top, "M", row.top.bbox(), true, false, f.ctx);
  REQUIRE(tiles.ok());
  Rects rects;
  for (const db::TileView& t : tiles.value()) {
    rects.push_back(t.area);
  }
  CHECK(strips(rects) == kSeams);
  auto subcells = see_hierarchical(f.source(), f.style, row.top, "M", row.top.bbox(), false, true, f.ctx);
  REQUIRE(subcells.ok());
  CHECK(subcells.value().empty());
  auto bad = see_hierarchical(f.source(), f.style, row.top, "X", row.top.bbox(), true, true, f.ctx);
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == "MASK-GEN-001");
}

TEST_CASE("A rotated 1-D array takes its pitch from child coordinates",
          "[cap:mask-hierarchical-generation][req:Array interaction geometry parameters]") {
  GenFixture f;
  // The child arrayed y 0..2 at 25, rotated by 90 degrees: the y pitch
  // becomes the parent's x separation (25) and r + child width the y one.
  const db::Transform rot90{0, -1, 0, 1, 0, 0};
  RowLayout row(f, db::ArrayInfo{0, 0, 0, 2, 0, 25});
  db::CellDef& top = f.cell("rotated");
  db::CellUse& use = f.place(top, row.child, rot90, db::ArrayInfo{0, 0, 0, 2, 0, 25});
  CHECK(use.bbox.width() == 70);  // nx = (70 - 20) / 25 + 1 = 3
  // The elements are 5 apart: the m1 (y 5..15 in the child) runs along x
  // after the rotation, so nothing closes and no array correction exists.
  const LayerPlanes out = f.hierarchical(top);
  CHECK(out[row.layer]->empty());
  CHECK(assembled(f, top, use, row.layer) == flat(f, top, row.layer));
}

TEST_CASE("Widely spaced arrays get no correction",
          "[cap:mask-hierarchical-generation][req:Array interaction regions]") {
  GenFixture f;
  const std::size_t m = f.layer("M", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 10), op(OpKind::shrink, 10)});
  db::CellDef& child = f.cell("child");
  f.paint(child, f.m1, Rect{0, 0, 10, 10});
  db::CellDef& top = f.cell("top");
  f.place(top, child, db::kIdentity, db::ArrayInfo{0, 2, 0, 2, 20, 20});
  const LayerPlanes out = f.hierarchical(top);
  CHECK(out[m]->empty());
  CHECK(f.ctx.statistics.hier_tiles == 0);
}

TEST_CASE("2-D arrays assemble to the flat result", "[cap:mask-hierarchical-generation][req:Array interaction regions]"
                                                  "[cap:mask-hierarchical-generation][req:Array correction replication]") {
  for (const bool on : {true, false}) {
    for (const std::int32_t n : {1, 2, 4}) {
      GenFixture f(on);
      const std::size_t m =
          f.layer("M", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 10), op(OpKind::shrink, 10)});
      db::CellDef& child = f.cell("child");
      f.paint(child, f.m1, Rect{1, 1, 9, 9});
      f.paint(child, f.m2, Rect{0, 0, 10, 10});
      db::CellDef& top = f.cell("top");
      db::CellUse& use = f.place(top, child, db::kIdentity, db::ArrayInfo{0, n, 0, 2, 10, 10});
      CAPTURE(on, n);
      CHECK(assembled(f, top, use, m) == flat(f, top, m));
    }
  }
}

TEST_CASE("The element filter of a reversed array (Q-MASK-406)",
          "[cap:mask-hierarchical-generation][req:Array interaction correction]") {
  // r = 11 against a pitch of 12: region C grown by r reaches a third
  // element, which only the signed filter of magic-compat ON admits for a
  // reversed index range (OFF keeps the corner pair).
  std::uint64_t tiles[2] = {0, 0};
  for (const bool on : {true, false}) {
    GenFixture f(on);
    const std::size_t m = f.layer("M", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 50), op(OpKind::shrink, 50)});
    REQUIRE(f.style.halo() == 11);
    db::CellDef& child = f.cell("child");
    f.paint(child, f.m1, Rect{0, 5, 10, 15});
    f.paint(child, f.m2, Rect{0, 0, 10, 20});
    db::CellDef& top = f.cell("top");
    f.place(top, child, db::kIdentity, db::ArrayInfo{3, 0, 0, 0, 12, 0});
    const LayerPlanes out = f.hierarchical(top);
    CHECK(GenFixture::rects(out, m) == kSeams);
    tiles[on ? 0 : 1] = f.ctx.statistics.flat_tiles;
  }
  CHECK(tiles[0] > tiles[1]);
}

TEST_CASE("An array with a zero pitch along a repeated axis is skipped",
          "[cap:mask-hierarchical-generation][req:Array interaction geometry parameters]") {
  GenFixture f;
  const std::size_t m = f.layer("M", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 10), op(OpKind::shrink, 10)});
  db::CellDef& child = f.cell("child");
  f.paint(child, f.m1, Rect{0, 0, 10, 10});
  db::CellDef& top = f.cell("top");
  f.place(top, child, db::kIdentity, db::ArrayInfo{0, 3, 0, 0, 0, 0});
  const LayerPlanes out = f.hierarchical(top);  // Magic divides by zero here
  CHECK(out[m]->empty());
}

TEST_CASE("Later layers reuse Magic's core windows with magic-compat ON",
          "[cap:mask-hierarchical-generation][req:Array correction replication]") {
  // Two identical layers over a 3x3 array: with magic-compat OFF both are
  // assembled to the flat result; ON copies the second layer's edges from
  // the first layer's core windows, as Magic does.
  for (const bool on : {true, false}) {
    GenFixture f(on);
    const std::vector<LayerOp> ops{types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 10), op(OpKind::shrink, 10)};
    const std::size_t first = f.layer("M", ops);
    const std::size_t second = f.layer("N", ops);
    db::CellDef& child = f.cell("child");
    f.paint(child, f.m1, Rect{1, 1, 9, 9});
    f.paint(child, f.m2, Rect{0, 0, 10, 10});
    db::CellDef& top = f.cell("top");
    db::CellUse& use = f.place(top, child, db::kIdentity, db::ArrayInfo{0, 2, 0, 2, 10, 10});
    CAPTURE(on);
    CHECK(assembled(f, top, use, first) == flat(f, top, first));
    if (!on) {
      CHECK(assembled(f, top, use, second) == flat(f, top, second));
    }
  }
}

}  // namespace hefesto::mask::test
