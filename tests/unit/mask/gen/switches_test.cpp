// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The output switches and flat CIF output (mask-hierarchical-generation
// "Hierarchy and array output switches", "Flat CIF output", "Mask-hint
// output operator" of mask-hints), with both magic-compat settings for
// Q-MASK-410 and Q-MASK-411.
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "gen_fixture.hpp"
#include "hefesto/hints/hints.hpp"

namespace hefesto::mask::test {

namespace {

struct GapLayout {
  GenFixture& f;
  std::size_t layer;
  db::CellDef& top;

  explicit GapLayout(GenFixture& fx)
      : f(fx),
        layer(fx.layer("M1X", {types_op(OpKind::op_or, {fx.m1}), op(OpKind::grow, 30), op(OpKind::shrink, 30)})),
        top(fx.cell("top")) {
    db::CellDef& left = f.cell("left");
    db::CellDef& right = f.cell("right");
    f.paint(left, f.m1, Rect{90, 10, 100, 90});
    f.paint(left, f.m2, Rect{0, 0, 100, 100});
    f.paint(right, f.m1, Rect{0, 10, 10, 90});
    f.paint(right, f.m2, Rect{0, 0, 100, 100});
    db::Label label;
    label.text = "pin";
    label.type = f.m1;
    label.rect = Rect{95, 50, 95, 50};
    left.labels().push_back(label);
    f.place(top, left);
    f.place(top, right, translate(105, 0));
  }
};

}  // namespace

TEST_CASE("Disabling subcell interactions", "[cap:mask-hierarchical-generation][req:Hierarchy and array output switches]") {
  GenFixture f;
  GapLayout g(f);
  HierarchyOptions off;
  off.subcells = false;
  CHECK(f.hierarchical(g.top, off)[g.layer]->empty());
  // The editor overload reads `cif *hier`.
  f.ed.mask.cif.hier = false;
  CHECK(generate_hierarchical(f.ed, g.top, f.style, f.ctx)[g.layer]->empty());
  f.ed.mask.cif.hier = true;
  CHECK(GenFixture::rects(generate_hierarchical(f.ed, g.top, f.style, f.ctx), g.layer) ==
        Rects{Rect{1000, 100, 1050, 900}});
}

TEST_CASE("CIFhier overrides the switches", "[cap:mask-hierarchical-generation][req:Hierarchy and array output switches]") {
  for (const bool on : {true, false}) {
    GenFixture f(on);
    GapLayout g(f);
    g.top.properties().put("CIFhier", std::string{"true"});
    HierarchyOptions disabled;
    disabled.subcells = false;
    disabled.arrays = false;
    const HierarchyOptions gds = hierarchy_options(g.top, disabled, true, f.compat);
    CHECK(gds.subcells);
    CHECK(gds.arrays);
    CHECK(GenFixture::rects(f.hierarchical(g.top, gds), g.layer) == Rects{Rect{1000, 100, 1050, 900}});
    const HierarchyOptions cif = hierarchy_options(g.top, disabled, false, f.compat);
    CHECK(cif.subcells == !on);  // Q-MASK-410
    CHECK(cif.arrays == !on);
    db::CellDef& plain = f.cell("plain");
    CHECK_FALSE(hierarchy_options(plain, disabled, true, f.compat).subcells);
  }
}

TEST_CASE("Flat CIF output flattens the hierarchy into one cell", "[cap:mask-hierarchical-generation][req:Flat CIF output]") {
  for (const bool on : {true, false}) {
    GenFixture f(on);
    GapLayout g(f);
    hints::HintRegion region;
    region.paint(Rect{0, 0, 5, 5});
    g.top.properties().put("MASKHINTS_SRAM", region);
    db::CellDef scratch(on ? "__CIF2__" : "top", f.tech->plane_count());
    flatten_for_output(f.source(), g.top, f.style, scratch);
    CHECK(scratch.uses().empty());
    CHECK(scratch.bbox() == Rect{0, 0, 205, 100});
    const LayerPlanes out = f.hierarchical(scratch);
    CHECK(GenFixture::rects(out, g.layer) == Rects{Rect{900, 100, 1150, 900}});
    // Q-MASK-411: labels and hints only with magic-compat OFF.
    CHECK(scratch.labels().size() == (on ? 0U : 1U));
    CHECK((scratch.properties().find("MASKHINTS_SRAM") != nullptr) == !on);
    if (!on) {
      CHECK(scratch.labels().front().text == "left_0/pin");
    }
  }
}

TEST_CASE("A mask hint adds geometry to an output layer", "[cap:mask-hints][req:Mask-hint output operator]") {
  GenFixture f;
  LayerOp hint = op(OpKind::mask_hints);
  hint.text = "SRAM";
  const std::size_t layer = f.layer("SRAMID", {hint});
  db::CellDef& cell = f.cell("c");
  hints::HintRegion region;
  region.paint(Rect{0, 0, 10, 10});
  cell.properties().put("MASKHINTS_SRAM", region);
  // A cell with only a hint has no bounding box from paint; give it one.
  f.paint(cell, f.m2, Rect{0, 0, 10, 10});
  CHECK(GenFixture::rects(f.hierarchical(cell), layer) == Rects{Rect{0, 0, 100, 100}});
}

TEST_CASE("A mask hint combined with paint", "[cap:mask-hints][req:Mask-hint output operator]") {
  GenFixture f;
  f.style.scale.scale_factor = 1;
  LayerOp bloat = op(OpKind::bloat_or);
  bloat.types.set(f.m1);
  bloat.bloat = BloatTable{};
  bloat.bloat->plane = static_cast<std::int32_t>(*f.tech->home_plane(f.m1));
  LayerOp hint = op(OpKind::mask_hints);
  hint.text = "MET1";
  const std::size_t layer = f.layer("MET1", {bloat, hint});
  db::CellDef& cell = f.cell("c");
  f.paint(cell, f.m1, Rect{0, 0, 4, 4});
  hints::HintRegion region;
  region.paint(Rect{4, 0, 6, 4});
  cell.properties().put("MASKHINTS_MET1", region);
  f.paint(cell, f.m2, Rect{0, 0, 6, 4});
  CHECK(GenFixture::rects(f.hierarchical(cell), layer) == Rects{Rect{0, 0, 6, 4}});
}

TEST_CASE("Interaction areas see the hints of the flattened subcells", "[cap:mask-hints][req:Mask-hint output operator]") {
  // The hint of each child joins the total, and also each child's own
  // component, so the hint adds no residual.
  GenFixture f;
  LayerOp hint = op(OpKind::mask_hints);
  hint.text = "H";
  const std::size_t layer =
      f.layer("X", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 30), op(OpKind::shrink, 30), hint});
  db::CellDef& a = f.cell("a");
  db::CellDef& b = f.cell("b");
  f.paint(a, f.m1, Rect{90, 10, 100, 90});
  f.paint(b, f.m1, Rect{0, 10, 10, 90});
  hints::HintRegion region;
  region.paint(Rect{0, 0, 10, 10});
  b.properties().put("MASKHINTS_H", region);
  db::CellDef& top = f.cell("top");
  f.place(top, a);
  f.place(top, b, translate(105, 0));
  CHECK(GenFixture::rects(f.hierarchical(top), layer) == Rects{Rect{1000, 100, 1050, 900}});
}

}  // namespace hefesto::mask::test
