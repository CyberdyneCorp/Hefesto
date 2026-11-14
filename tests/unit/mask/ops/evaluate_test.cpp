// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-output-operators: the layer evaluation pipeline, the geometric model
// (scaling), the boolean operators, hierarchical-mode truncation and the
// non-Manhattan merge default.

#include "ops_fixture.hpp"

using namespace hefesto;
using namespace hefesto::mask;
using namespace hefesto::mask::test;

#define CAP "[cap:mask-output-operators]"

TEST_CASE("paint scaling: a layout tile becomes its scaled rectangle", CAP "[req:Geometric model of mask planes]") {
  OpsFixture f;
  f.scale(10);
  f.paint(f.m1, Rect{3, 0, 7, 5});
  CHECK(f.run({with_types(op(OpKind::op_or), {f.m1})}) == Rects{{30, 0, 70, 50}});
}

TEST_CASE("a mask plane is the canonical maximal-horizontal-strip decomposition",
          CAP "[req:Geometric model of mask planes]") {
  OpsFixture f;
  f.paint(f.m1, Rect{0, 0, 10, 10});
  f.paint(f.m1, Rect{10, 0, 20, 5});
  // Enumerated top to bottom, left to right; vertically adjacent strips of
  // equal x extent merge.
  CHECK(f.run({with_types(op(OpKind::op_or), {f.m1})}) == Rects{{0, 5, 10, 10}, {0, 0, 20, 5}});
}

TEST_CASE("whole tile contributes, then the result is clipped", CAP "[req:Layer evaluation pipeline for an area]") {
  OpsFixture f;
  f.paint(f.m1, Rect{0, 0, 100, 10});
  OutputLayer l;
  l.name = "M";
  l.ops = {with_types(op(OpKind::op_or), {f.m1}), op(OpKind::shrink, {4})};
  f.style.layers.push_back(l);
  const std::vector<bool> requested{true};
  AreaOptions options;
  options.radius = 2;
  const std::vector<MaskPlane*> planes =
      evaluate_area(f.style, requested, f.inputs(), Rect{0, 0, 50, 10}, f.ctx, options);
  REQUIRE(planes.size() == 1);
  REQUIRE(planes[0] != nullptr);
  CHECK(planes[0]->solid_rects() == Rects{{4, 4, 50, 6}});
}

TEST_CASE("layers evaluate in order and read earlier unclipped layers",
          CAP "[req:Layer evaluation pipeline for an area]") {
  OpsFixture f;
  f.paint(f.m1, Rect{0, 0, 100, 10});
  OutputLayer a;
  a.name = "A";
  a.temporary = true;
  a.ops = {with_types(op(OpKind::op_or), {f.m1})};
  OutputLayer b;
  b.name = "B";
  b.ops = {with_layers(op(OpKind::op_or), {0}), op(OpKind::shrink, {2})};
  f.style.layers = {a, b};
  AreaOptions options;
  options.radius = 0;
  options.all_planes = true;
  const std::vector<MaskPlane*> only_b =
      evaluate_area(f.style, std::vector<bool>{true, true}, f.inputs(), Rect{0, 0, 50, 10}, f.ctx, options);
  // B shrinks A's whole (unclipped) tile, then both are clipped to the area.
  CHECK(only_b[0]->solid_rects() == Rects{{0, 0, 50, 10}});
  CHECK(only_b[1]->solid_rects() == Rects{{2, 2, 50, 8}});
  const std::vector<MaskPlane*> none = evaluate_area(f.style, std::vector<bool>{false, true}, f.inputs(),
                                                     Rect{0, 0, 50, 10}, f.ctx, AreaOptions{false, 0});
  CHECK(none[0] == nullptr);  // not requested, no plane
}

TEST_CASE("the clip area keeps infinite sides and the halo grows finite ones",
          CAP "[req:Layer evaluation pipeline for an area]") {
  const GenerationClip c = generation_clip(Rect{0, 0, 10, 20}, 5, 3);
  CHECK(c.expanded == Rect{-3, -3, 13, 23});
  CHECK(c.clip == Rect{0, 0, 50, 100});
  const GenerationClip w = generation_clip(db::whole_plane(), 5, 3);
  CHECK(w.expanded == db::whole_plane());
  CHECK(w.clip == db::whole_plane());
}

TEST_CASE("and-not removes the layer's geometry", CAP "[req:or, and, and-not]") {
  OpsFixture f;
  const std::size_t m = f.mask_layer({Rect{40, -10, 60, 200}});
  CHECK(f.apply({Rect{0, 0, 100, 100}}, {with_layers(op(OpKind::and_not), {m})}) ==
        strips({Rect{0, 0, 40, 100}, Rect{60, 0, 100, 100}}));
}

TEST_CASE("or unites and and intersects layout types and mask layers", CAP "[req:or, and, and-not]") {
  OpsFixture f;
  f.paint(f.ndiff, Rect{0, 0, 10, 10});
  f.paint(f.poly, Rect{20, 0, 30, 10});
  const std::size_t m = f.mask_layer({Rect{5, 5, 25, 15}});
  CHECK(f.run({with_types(op(OpKind::op_or), {f.ndiff, f.poly})}) == strips({{0, 0, 10, 10}, {20, 0, 30, 10}}));
  CHECK(f.run({with_types(op(OpKind::op_or), {f.ndiff, f.poly}), with_layers(op(OpKind::op_and), {m})}) ==
        strips({{5, 5, 10, 10}, {20, 5, 25, 10}}));
  // Only layout tiles overlapping the search area are read (each in full).
  CHECK(f.run({with_types(op(OpKind::op_or), {f.ndiff, f.poly})}, Rect{8, 2, 12, 4}) == Rects{{0, 0, 10, 10}});
}

TEST_CASE("a contact on several planes is read once per plane, with the same result", CAP "[req:or, and, and-not]") {
  OpsFixture f;
  f.paint(f.ndc, Rect{0, 0, 4, 4});
  CHECK(f.run({with_types(op(OpKind::op_or), {f.ndc})}) == Rects{{0, 0, 4, 4}});
}

TEST_CASE("hierarchical evaluation stops at squares not followed by grow", CAP "[req:Hierarchical-mode truncation]") {
  OpsFixture f;
  f.paint(f.poly, Rect{0, 0, 40, 40});
  const std::vector<LayerOp> cont{with_types(op(OpKind::op_or), {f.poly}), op(OpKind::squares, {5, 17, 17})};
  f.ctx.hierarchical = true;
  CHECK(f.run(cont) == Rects{{0, 0, 40, 40}});  // the unsquared poly
  f.ctx.hierarchical = false;
  CHECK(f.run(cont) != Rects{{0, 0, 40, 40}});
}

TEST_CASE("the hierarchical stop index", CAP "[req:Hierarchical-mode truncation]") {
  const auto stop = [](std::initializer_list<OpKind> kinds) {
    std::vector<LayerOp> ops;
    for (const OpKind k : kinds) {
      ops.push_back(op(k));
    }
    return hierarchical_stop(ops);
  };
  CHECK(stop({OpKind::op_or, OpKind::grow}) == std::nullopt);
  CHECK(stop({OpKind::op_or, OpKind::squares, OpKind::grow}) == std::nullopt);
  CHECK(stop({OpKind::op_or, OpKind::squares}) == 1);
  CHECK(stop({OpKind::slots, OpKind::shrink}) == 0);
  CHECK(stop({OpKind::squares_grid}) == 0);
  for (const OpKind k : {OpKind::net, OpKind::tagged, OpKind::boundary, OpKind::bbox}) {
    CHECK(stop({OpKind::op_or, k, OpKind::grow}) == 1);
  }
}

TEST_CASE("operator evaluation applies no non-Manhattan merge pass", CAP "[req:Non-Manhattan merge option]") {
  // The `unfracture` switch belongs to generation (mask-hierarchical-
  // generation); evaluating a layer from split tiles keeps their triangles.
  OpsFixture f;
  REQUIRE(db::paint_triangle(*f.cell, *f.tech, Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, f.m1).ok());
  const MaskPlane& p = f.eval({with_types(op(OpKind::op_or), {f.m1})});
  CHECK(p.solid_area() == 50.0);
  CHECK(p.solid_tiles().size() == 1);
  CHECK(p.solid_tiles().front().split);
}
