// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators: bloat-or (Q-MASK-20), bloat-max and bloat-min, and
// bloat-all (Q-MASK-230, 231, 232 in both modes).

#include <utility>

#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define CAP "[cap:mask-output-operators]"

namespace {

/// A bloat-or/min/max operator: bloated types, bloat plane, per-type distances.
LayerOp bloat(OpKind kind, std::initializer_list<tech::TileType> types, tech::PlaneId plane,
              std::initializer_list<std::pair<tech::TileType, std::int64_t>> distances) {
  LayerOp o = with_types(op(kind), types);
  o.values.assign(1 + tech::kMaxTileTypes, 0);
  o.values[0] = plane;
  for (const auto& [t, d] : distances) {
    o.values[1 + t] = d;
  }
  return o;
}

/// A bloat-all over layout types: triggers, bloat plane, bloated types, clip.
LayerOp bloat_all(LayerOp triggers, int plane, std::initializer_list<std::size_t> bloated, std::int64_t d = 0) {
  triggers.kind = OpKind::bloat_all;
  triggers.values.assign(2 + tech::kMaxTileTypes, 0);
  triggers.values[0] = d;
  triggers.values[1] = plane;
  for (const std::size_t i : bloated) {
    triggers.values[2 + i] = 1;
  }
  return triggers;
}

}  // namespace

TEST_CASE("selective bloat with corner fill", CAP "[req:bloat-or]") {
  OpsFixture f;
  f.paint(f.ndiff, Rect{0, 0, 10, 10});
  f.paint(f.poly, Rect{-5, 0, 0, 10});
  const Rects out = f.run({bloat(OpKind::bloat_or, {f.ndiff}, f.active, {{f.poly, 3}, {tech::kSpace, 1}})});
  CHECK(out == strips({{0, 0, 10, 10}, {-3, -1, 0, 10}, {-3, 10, 10, 11}, {10, 0, 11, 11}, {0, -1, 11, 0}}));
}

TEST_CASE("bloat-or scales the tile and its strips", CAP "[req:bloat-or]") {
  OpsFixture f;
  f.scale(10);
  f.paint(f.ndiff, Rect{0, 0, 1, 1});
  const Rects out = f.run({bloat(OpKind::bloat_or, {f.ndiff}, f.active, {{tech::kSpace, 2}})});
  CHECK(out == strips({{-2, -2, 12, 12}}));
}

TEST_CASE("bloat-or of a split tile moves the triangle along its hypotenuse (Q-MASK-20)", CAP "[req:bloat-or]") {
  OpsFixture f;
  REQUIRE(db::paint_triangle(*f.cell, *f.tech, Rect{0, 0, 10, 10}, db::Diagonal::nw_se, false, f.ndiff).ok());
  const MaskPlane& p = f.eval({bloat(OpKind::bloat_or, {f.ndiff}, f.active, {{tech::kSpace, 2}})});
  // The legs' rectangles and the triangle moved by 2 towards the
  // hypotenuse; the legs are not bloated (no strips on their sides beyond
  // what the corner fill adds).
  CHECK(p.plane().type_at(db::Point{11, 0}) == kMaskSolid);
  CHECK(p.plane().type_at(db::Point{0, 11}) == kMaskSolid);
  CHECK(p.plane().type_at(db::Point{7, 7}) != kMaskSolid);
}

TEST_CASE("bloat-max against bloat-min", CAP "[req:bloat-max and bloat-min]") {
  OpsFixture f;
  f.paint(f.pdiff, Rect{0, 0, 10, 10});
  f.paint(f.poly, Rect{10, 4, 20, 10});
  CHECK(f.run({bloat(OpKind::bloat_max, {f.pdiff}, f.active, {{tech::kSpace, 2}, {f.poly, 6}})}) ==
        Rects{{-2, -2, 16, 12}});
  CHECK(f.run({bloat(OpKind::bloat_min, {f.pdiff}, f.active, {{tech::kSpace, 2}, {f.poly, 6}})}) ==
        Rects{{-2, -2, 12, 12}});
}

TEST_CASE("bloat-min with negative distances shrinks, and an inverted tile is reported",
          CAP "[req:bloat-max and bloat-min]") {
  OpsFixture f;
  f.paint(f.pdiff, Rect{0, 0, 10, 10});
  CHECK(f.run({bloat(OpKind::bloat_min, {f.pdiff}, f.active, {{tech::kSpace, -2}})}) == Rects{{2, 2, 8, 8}});
  CHECK(f.sink.take().empty());
  CHECK(f.run({bloat(OpKind::bloat_min, {f.pdiff}, f.active, {{tech::kSpace, -6}})}).empty());
  const auto diags = f.sink.take();
  REQUIRE(diags.size() == 1);
  CHECK(diags.front().code == "MASK-OPER-001");
  CHECK(diags.front().text == "CIF error in cell top, layer OUT: tile inverted by shrink");
  CHECK(diags.front().native_only);
}

TEST_CASE("implant covering device and contacts", CAP "[req:bloat-all]") {
  OpsFixture f;
  f.paint(f.nfet, Rect{10, 0, 20, 10});
  f.paint(f.ndiff, Rect{0, 0, 10, 10});
  f.paint(f.ndiff, Rect{20, 0, 30, 10});
  f.paint(f.ndc, Rect{30, 0, 35, 10});
  CHECK(f.run({bloat_all(with_types(op(OpKind::bloat_all), {f.nfet}), f.active, {f.ndiff, f.ndc})}) ==
        Rects{{0, 0, 35, 10}});
}

TEST_CASE("clipped bloat-all", CAP "[req:bloat-all]") {
  OpsFixture f;
  f.paint(f.nfet, Rect{10, 0, 20, 10});
  f.paint(f.ndiff, Rect{0, 0, 10, 10});
  f.paint(f.ndiff, Rect{20, 0, 30, 10});
  f.paint(f.ndc, Rect{30, 0, 35, 10});
  CHECK(f.run({bloat_all(with_types(op(OpKind::bloat_all), {f.nfet}), f.active, {f.ndiff, f.ndc}, 5)}) ==
        Rects{{5, 0, 25, 10}});
}

TEST_CASE("mask-layer trigger with the unit conversion of Q-MASK-232", CAP "[req:bloat-all]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.scale(10);
    f.paint(f.ndiff, Rect{-2, 0, 3, 1});
    const std::size_t m = f.mask_layer({Rect{-17, 0, 23, 10}});
    const LayerOp trig = with_layers(op(OpKind::bloat_all), {m});
    const Rects clipped = f.run({bloat_all(trig, f.active, {f.ndiff}, 5)});
    const Rects whole = f.run({bloat_all(trig, f.active, {f.ndiff})});
    CHECK(whole == Rects{{-20, 0, 30, 10}});
    if (on) {
      CHECK(clipped == Rects{{-15, 0, 25, 10}});  // seed [-1,2]x[0,1], clip [-15,25]x[-5,15]
    } else {
      CHECK(clipped == Rects{{-20, 0, 30, 10}});  // seed [-2,3]x[0,1], clip [-25,35]x[-5,15]
    }
  }
}

TEST_CASE("order-independent seeding with magic-compat OFF (Q-MASK-231)", CAP "[req:bloat-all]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.paint(f.ndiff, Rect{0, 0, 10, 10});
    f.paint(f.ndiff, Rect{20, 0, 30, 10});
    const std::size_t m = f.mask_layer({Rect{0, 0, 30, 10}});
    const Rects out = f.run({bloat_all(with_layers(op(OpKind::bloat_all), {m}), f.active, {f.ndiff})});
    if (on) {
      CHECK(out == Rects{{0, 0, 10, 10}});  // only the first unmarked tile
    } else {
      CHECK(out == Rects{{0, 0, 10, 10}, {20, 0, 30, 10}});
    }
  }
}

TEST_CASE("bloat-all into mask layers (cases B and E)", CAP "[req:bloat-all]") {
  OpsFixture f;
  const std::size_t t2 = f.mask_layer({Rect{0, 0, 10, 10}, Rect{10, 0, 20, 30}, Rect{100, 0, 110, 10}});
  const std::size_t trig = f.mask_layer({Rect{15, 25, 16, 26}});
  LayerOp b = with_layers(op(OpKind::bloat_all), {trig});
  b.values.assign(2 + t2 + 1, 0);
  b.values[1] = -1;
  b.values[2 + t2] = 1;
  CHECK(f.run({b}) == strips({{0, 0, 10, 10}, {10, 0, 20, 30}}));
  f.paint(f.poly, Rect{105, 5, 106, 6});
  LayerOp e = with_types(op(OpKind::bloat_all), {f.poly});
  e.values = b.values;
  CHECK(f.run({e}) == Rects{{100, 0, 110, 10}});
}

TEST_CASE("stale trigger marks of Q-MASK-230", CAP "[req:bloat-all]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.paint(f.ndc, Rect{0, 0, 4, 4});
    f.paint(f.ndiff, Rect{4, 0, 20, 4});
    f.paint(f.m1, Rect{-10, 0, 0, 4});
    const LayerOp to_m1 = bloat_all(with_types(op(OpKind::bloat_all), {f.ndc}), f.metal1, {f.m1});
    const LayerOp to_ndiff = bloat_all(with_types(op(OpKind::bloat_all), {f.ndc}), f.active, {f.ndiff});
    const Rects out = f.run({to_m1, to_ndiff});
    if (on) {
      // The first bloat-all leaves the active-plane image of ndc marked, so
      // the second one starts only from its metal1 image.
      CHECK(out == Rects{{-10, 0, 4, 4}});
      CHECK_FALSE(f.ctx.stale_marks.tiles.empty());
    } else {
      CHECK(out == Rects{{-10, 0, 20, 4}});
      CHECK(f.ctx.stale_marks.tiles.empty());
    }
  }
}
