// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators: close (Q-MASK-2), maxrect, orthogonal, not-square
// (Q-MASK-7), the interaction filters (Q-MASK-21), net (Q-MASK-22), tagged
// (Q-MASK-6), bbox (Q-MASK-23), boundary and mask-hints.

#include "hefesto/db/label.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/hints/region.hpp"
#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define CAP "[cap:mask-output-operators]"

namespace {

const Rects kRing{{0, 0, 30, 10}, {0, 20, 30, 30}, {0, 10, 10, 20}, {20, 10, 30, 20}};

/// A square ring with a `w` x `h` hole at (10, 10).
Rects ring_with_hole(db::Coord w, db::Coord h) {
  return {{0, 0, 20 + w, 10}, {0, 10 + h, 20 + w, 20 + h}, {0, 10, 10, 10 + h}, {10 + w, 10, 20 + w, 10 + h}};
}

void label(OpsFixture& f, const std::string& text, tech::TileType type, const Rect& r) {
  db::Label l;
  l.text = text;
  l.type = type;
  l.rect = r;
  f.cell->labels().push_back(l);
}

}  // namespace

TEST_CASE("small hole filled", CAP "[req:close]") {
  OpsFixture f;
  CHECK(f.apply(kRing, {op(OpKind::close, {101})}) == Rects{{0, 0, 30, 30}});
  CHECK(f.apply(kRing, {op(OpKind::close, {100})}) == strips(kRing));
  CHECK(f.apply(kRing, {op(OpKind::close)}) == Rects{{0, 0, 30, 30}});  // every enclosed area
  // The outer space is not enclosed: nothing else is filled.
  CHECK(f.apply({{0, 0, 10, 10}}, {op(OpKind::close)}) == Rects{{0, 0, 10, 10}});
}

TEST_CASE("threshold after a unit reduction with magic-compat OFF (Q-MASK-2)", CAP "[req:close]") {
  // `close 100` declared, then a reduction by L = 2: Magic's threshold is 50.
  const LayerOp c = op(OpKind::close, {50, 100, 1, 2});
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    // A hole of 24 reduced units² (96 declared): filled in both modes.
    CHECK(f.apply(ring_with_hole(4, 6), {c}) == Rects{{0, 0, 24, 26}});
    // A hole of 26 reduced units² (104 declared): filled with ON only.
    const Rects out = f.apply(ring_with_hole(2, 13), {c});
    if (on) {
      CHECK(out == Rects{{0, 0, 22, 33}});
    } else {
      CHECK(out == strips(ring_with_hole(2, 13)));
    }
  }
}

TEST_CASE("Q-MASK-2 helper", CAP "[req:close]") {
  compat::CompatSettings on;
  compat::CompatSettings off;
  off.magic_compat = false;
  const CloseThreshold t{50, 100, 1, 2};
  CHECK(q_mask_2_below(26, t, on));
  CHECK_FALSE(q_mask_2_below(26, t, off));
  CHECK(q_mask_2_below(24, t, off));
}

TEST_CASE("maxrect of an L-shape", CAP "[req:maxrect]") {
  OpsFixture f;
  const Rects l{{0, 0, 10, 30}, {10, 0, 40, 10}};
  CHECK(f.apply(l, {op(OpKind::maxrect)}) == Rects{{0, 0, 40, 10}});
  CHECK(f.apply(l, {with_text(op(OpKind::maxrect), "external")}) == Rects{{0, 0, 40, 30}});
  // A single isolated tile is its own bounding box.
  CHECK(f.apply({{5, 5, 8, 9}}, {op(OpKind::maxrect)}) == Rects{{5, 5, 8, 9}});
}

TEST_CASE("orthogonal removes or fills triangles", CAP "[req:orthogonal]") {
  OpsFixture f;
  f.paint(f.m1, Rect{0, 0, 10, 10});
  REQUIRE(db::paint_triangle(*f.cell, *f.tech, Rect{10, 0, 20, 10}, db::Diagonal::nw_se, false, f.m1).ok());
  const LayerOp m1 = with_types(op(OpKind::op_or), {f.m1});
  CHECK(f.run({m1, op(OpKind::orthogonal)}) == Rects{{0, 0, 10, 10}});
  CHECK(f.run({m1, with_text(op(OpKind::orthogonal), "fill")}) == Rects{{0, 0, 20, 10}});
}

TEST_CASE("isolated square removed", CAP "[req:not-square]") {
  OpsFixture f;
  CHECK(f.apply({{0, 0, 10, 10}, {20, 0, 30, 15}}, {op(OpKind::not_square)}) == Rects{{20, 0, 30, 15}});
  // A square touching another tile is kept.
  CHECK(f.apply({{0, 0, 10, 10}, {10, 0, 30, 5}}, {op(OpKind::not_square)}) ==
        strips({{0, 0, 10, 10}, {10, 0, 30, 5}}));
}

TEST_CASE("triangle kept with magic-compat OFF (Q-MASK-7)", CAP "[req:not-square]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    REQUIRE(db::paint_triangle(*f.cell, *f.tech, Rect{0, 0, 10, 10}, db::Diagonal::nw_se, false, f.m1).ok());
    f.paint(f.m1, Rect{20, 0, 30, 15});
    const MaskPlane& p = f.eval({with_types(op(OpKind::op_or), {f.m1}), op(OpKind::not_square)});
    if (on) {
      CHECK(p.solid_rects() == Rects{{20, 0, 30, 15}});
    } else {
      CHECK(p.solid_area() == 50.0 + 150.0);
      CHECK(p.plane().type_at(db::Point{1, 1}) == kMaskSolid);
    }
  }
}

TEST_CASE("touching counts only for interacting",
          CAP "[req:overlapping, nonoverlapping, interacting, noninteracting]") {
  OpsFixture f;
  const std::size_t m = f.mask_layer({{10, 0, 20, 10}});
  const Rects cur{{0, 0, 10, 10}, {50, 0, 60, 10}};
  LayerOp overlapping = with_layers(op(OpKind::interacting), {m});
  LayerOp interacting = overlapping;
  interacting.interact.touching = true;
  LayerOp noninteracting = interacting;
  noninteracting.interact.inverted = true;
  LayerOp nonoverlapping = overlapping;
  nonoverlapping.interact.inverted = true;
  CHECK(f.apply(cur, {overlapping}).empty());
  CHECK(f.apply(cur, {interacting}) == Rects{{0, 0, 10, 10}});
  CHECK(f.apply(cur, {noninteracting}) == Rects{{50, 0, 60, 10}});
  CHECK(f.apply(cur, {nonoverlapping}) == strips(cur));
}

TEST_CASE("database test at negative coordinates (Q-MASK-21)",
          CAP "[req:overlapping, nonoverlapping, interacting, noninteracting]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.scale(10);
    f.paint(f.poly, Rect{-3, 0, -2, 1});
    const Rects out = f.apply({{-25, 0, -15, 10}}, {with_types(op(OpKind::interacting), {f.poly})});
    if (on) {
      CHECK(out.empty());  // r converts to [-2,-1]x[0,1]
    } else {
      CHECK(out == Rects{{-25, 0, -15, 10}});  // r converts to [-3,-1]x[0,1]
    }
  }
}

TEST_CASE("Q-MASK-21 helper", CAP "[req:overlapping, nonoverlapping, interacting, noninteracting]") {
  compat::CompatSettings on;
  compat::CompatSettings off;
  off.magic_compat = false;
  CHECK(q_mask_21_to_internal(Rect{-25, 0, -15, 10}, 10, on) == Rect{-2, 0, -1, 1});
  CHECK(q_mask_21_to_internal(Rect{-25, 0, -15, 10}, 10, off) == Rect{-3, 0, -1, 1});
}

TEST_CASE("net extraction", CAP "[req:net]") {
  OpsFixture f;
  f.paint(f.m1, Rect{0, 0, 10, 4});
  f.paint(f.via, Rect{10, 0, 14, 4});
  f.paint(f.m2, Rect{14, 0, 30, 4});
  f.paint(f.m1, Rect{50, 0, 60, 4});  // another net
  label(f, "VDD", f.m1, Rect{2, 2, 2, 2});
  CHECK(f.run({with_text(with_types(op(OpKind::net), {f.m1, f.m2}), "VDD")}) ==
        strips({{0, 0, 10, 4}, {14, 0, 30, 4}}));
  // Without layers (Q-MASK-22) and for an unknown name nothing is added.
  CHECK(f.run({with_text(op(OpKind::net), "VDD")}).empty());
  CHECK(f.run({with_text(with_types(op(OpKind::net), {f.m1, f.m2}), "GND")}).empty());
}

TEST_CASE("tagged with scale 1", CAP "[req:tagged]") {
  OpsFixture f;
  label(f, "A", f.m1, Rect{25, 5, 25, 5});
  CHECK(f.apply({{0, 0, 10, 10}, {20, 0, 30, 10}}, {with_text(op(OpKind::tagged), "A")}) == Rects{{20, 0, 30, 10}});
  CHECK(f.apply({{0, 0, 10, 10}, {20, 0, 30, 10}}, {with_text(op(OpKind::tagged), "B")}).empty());
}

TEST_CASE("tagged with scale 10 (Q-MASK-6)", CAP "[req:tagged]") {
  for (const bool on : {true, false}) {
    OpsFixture f;
    f.compat(on);
    f.scale(10);
    label(f, "A", f.m1, Rect{25, 5, 25, 5});
    const Rects out = f.apply({{0, 0, 100, 100}, {200, 0, 300, 100}}, {with_text(op(OpKind::tagged), "A")});
    if (on) {
      CHECK(out == Rects{{0, 0, 100, 100}});
    } else {
      CHECK(out == Rects{{200, 0, 300, 100}});
    }
  }
}

TEST_CASE("tagged over layout types ORs the touched regions", CAP "[req:tagged]") {
  OpsFixture f;
  f.paint(f.m1, Rect{0, 0, 10, 10});
  f.paint(f.m1, Rect{10, 0, 20, 30});
  f.paint(f.m1, Rect{40, 0, 50, 10});
  label(f, "N", f.m1, Rect{15, 25, 15, 25});
  CHECK(f.run({with_text(with_types(op(OpKind::tagged), {f.m1}), "N")}) == strips({{0, 0, 10, 10}, {10, 0, 20, 30}}));
}

TEST_CASE("bbox scales the cell's bounding box", CAP "[req:bbox]") {
  OpsFixture f;
  f.scale(5);
  f.cell->set_bboxes(Rect{0, 0, 100, 40}, Rect{0, 0, 100, 40});
  CHECK(f.run({op(OpKind::bbox)}) == Rects{{0, 0, 500, 200}});
  // `bbox top`: nothing for a cell placed in a non-internal parent.
  db::CellDef parent{"parent", f.tech->plane_count()};
  db::CellUse use;
  use.def = f.cell.get();
  use.parent = &parent;
  f.cell->instances().push_back(&use);
  CHECK(f.run({with_text(op(OpKind::bbox), "top")}).empty());
  parent.flags().internal = true;
  CHECK(f.run({with_text(op(OpKind::bbox), "top")}) == Rects{{0, 0, 500, 200}});
  f.cell->instances().clear();
  // No cell being generated: nothing.
  f.ctx.cell = nullptr;
  CHECK(f.run({op(OpKind::bbox)}).empty());
}

TEST_CASE("fixed bbox", CAP "[req:boundary]") {
  OpsFixture f;
  f.scale(2);
  CHECK(f.run({op(OpKind::boundary)}).empty());
  db::set_property(*f.cell, "FIXED_BBOX", db::DimensionList{{0, 0, 50, 20}});
  CHECK(f.run({op(OpKind::boundary)}) == Rects{{0, 0, 100, 40}});
}

TEST_CASE("mask hints", CAP "[req:mask-hints]") {
  OpsFixture f;
  CHECK(f.run({with_text(op(OpKind::mask_hints), "HVI")}).empty());
  db::set_property(*f.cell, "MASKHINTS_HVI", hints::HintRegion{Rect{0, 0, 10, 10}, Rect{20, 0, 30, 10}});
  CHECK(f.run({with_text(op(OpKind::mask_hints), "HVI")}) == Rects{{0, 0, 10, 10}, {20, 0, 30, 10}});
  f.scale(3);
  CHECK(f.run({with_text(op(OpKind::mask_hints), "HVI")}) == Rects{{0, 0, 30, 30}, {60, 0, 90, 30}});
}
