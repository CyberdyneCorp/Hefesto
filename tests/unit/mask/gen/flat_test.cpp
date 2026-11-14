// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flat generation and its views (mask-hierarchical-generation "Flat region
// generation", "Halo radius", "Flattening for interaction processing",
// "Generation statistics"; cif-format-io "cif see, paint, coverage and
// statistics options").
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "gen_fixture.hpp"
#include "hefesto/hints/hints.hpp"

namespace hefesto::mask::test {

namespace {

// Two children with m1 2 internal units apart under a grow/shrink layer
// (the spec's "Gap closed by grow/shrink across two children").
struct GapLayout {
  GenFixture& f;
  std::size_t m1x;
  db::CellDef& top;

  explicit GapLayout(GenFixture& fx)
      : f(fx),
        m1x(fx.layer("M1X", {types_op(OpKind::op_or, {fx.m1}), op(OpKind::grow, 30), op(OpKind::shrink, 30)})),
        top(fx.cell("top")) {
    db::CellDef& left = f.cell("left");
    db::CellDef& right = f.cell("right");
    f.paint(left, f.m1, Rect{90, 10, 100, 90});
    f.paint(left, f.m2, Rect{0, 0, 100, 100});
    f.paint(right, f.m1, Rect{0, 10, 10, 90});
    f.paint(right, f.m2, Rect{0, 0, 100, 100});
    f.place(top, left);
    f.place(top, right, translate(105, 0));
  }
};

}  // namespace

TEST_CASE("Flat generation shows the interaction geometry",
          "[cap:mask-hierarchical-generation][req:Flat region generation]") {
  GenFixture f;
  GapLayout g(f);
  REQUIRE(f.style.halo() == 7);
  const LayerPlanes planes = generate_flat(f.source(), g.top, Rect{0, 0, 205, 100}, f.style, f.ctx);
  CHECK(GenFixture::rects(planes, g.m1x) == Rects{Rect{900, 100, 1150, 900}});
  CHECK(f.codes().empty());
}

TEST_CASE("Flat generation through the editor generates every layer",
          "[cap:mask-hierarchical-generation][req:Flat region generation]") {
  GenFixture f;
  GapLayout g(f);
  const std::size_t plain = f.layer("CM2", {types_op(OpKind::op_or, {f.m2})});
  const LayerPlanes planes = generate_flat(f.ed, g.top, Rect{0, 0, 205, 100}, f.style, f.ctx);
  REQUIRE(planes.size() == 2);
  CHECK(GenFixture::rects(planes, plain) == Rects{Rect{0, 0, 1000, 1000}, Rect{1050, 0, 2050, 1000}});
}

TEST_CASE("Flat generation copies whole tiles", "[cap:mask-hierarchical-generation][req:Flattening for interaction processing]") {
  GenFixture f;
  f.style.scale.scale_factor = 1;
  const std::size_t layer = f.layer("S", {types_op(OpKind::op_or, {f.m1}), op(OpKind::shrink, 2)});
  db::CellDef& child = f.cell("child");
  f.paint(child, f.m1, Rect{0, 0, 500, 10});
  db::CellDef& top = f.cell("top");
  f.place(top, child);
  // The whole strip is shrunk, then clipped: a clipped copy would shrink
  // the window's sides too.
  const LayerPlanes planes = generate_flat(f.source(), top, Rect{90, -5, 110, 15}, f.style, f.ctx);
  CHECK(GenFixture::rects(planes, layer) == Rects{Rect{90, 2, 110, 8}});
}

TEST_CASE("Flat generation composes paint across cells",
          "[cap:mask-hierarchical-generation][req:Flattening for interaction processing]") {
  GenFixture f;
  f.style.scale.scale_factor = 1;
  const std::size_t fet = f.layer("FET", {types_op(OpKind::op_or, {f.nfet})});
  db::CellDef& child = f.cell("child");
  f.paint(child, f.ndiff, Rect{-4, 0, 6, 4});
  db::CellDef& top = f.cell("top");
  f.paint(top, f.poly, Rect{0, -2, 2, 6});
  f.place(top, child);
  const LayerPlanes planes = generate_flat(f.source(), top, Rect{-5, -5, 7, 7}, f.style, f.ctx);
  CHECK(GenFixture::rects(planes, fet) == Rects{Rect{0, 0, 2, 4}});
}

TEST_CASE("Flat generation copies vendor paint", "[cap:mask-hierarchical-generation][req:Flattening for interaction processing]") {
  for (const bool on : {true, false}) {
    GenFixture f(on);
    f.style.scale.scale_factor = 1;
    f.style.see_no_vendor = true;  // Q-MASK-401: no effect
    f.style.options |= style_option::see_no_vendor;
    const std::size_t layer = f.layer("M", {types_op(OpKind::op_or, {f.m1})});
    db::CellDef& vendor = f.cell("vendor");
    f.paint(vendor, f.m1, Rect{0, 0, 10, 10});
    vendor.flags().vendor_gds = true;
    db::CellDef& top = f.cell("top");
    f.place(top, vendor);
    const LayerPlanes planes = generate_flat(f.source(), top, Rect{0, 0, 10, 10}, f.style, f.ctx);
    CHECK(GenFixture::rects(planes, layer) == Rects{Rect{0, 0, 10, 10}});
  }
}

TEST_CASE("Flat generation copies the subtree's mask hints transformed",
          "[cap:mask-hints][req:Mask-hint output operator]") {
  GenFixture f;
  LayerOp hint = op(OpKind::mask_hints);
  hint.text = "SRAM";
  const std::size_t layer = f.layer("SRAMID", {hint});
  db::CellDef& child = f.cell("child");
  f.paint(child, f.m2, Rect{0, 0, 20, 20});
  hints::HintRegion region;
  region.paint(Rect{0, 0, 10, 10});
  child.properties().put("MASKHINTS_SRAM", region);
  db::CellDef& top = f.cell("top");
  f.place(top, child, translate(30, 0));
  const LayerPlanes planes = generate_flat(f.source(), top, Rect{30, 0, 50, 20}, f.style, f.ctx);
  CHECK(GenFixture::rects(planes, layer) == Rects{Rect{300, 0, 400, 100}});
}

TEST_CASE("Halo radius from the operators",
          "[cap:mask-hierarchical-generation][req:Halo radius]") {
  SECTION("grow distance") {
    GenFixture f;
    f.layer("A", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 30)});
    CHECK(generation_radius(f.style, f.compat) == 7);
    f.set_compat(false);
    CHECK(generation_radius(f.style, f.compat) == 7);
  }
  SECTION("shrink dominates, and ties use the shrink") {
    GenFixture f;
    const std::size_t a = f.layer("A", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 20)}, true);
    f.layer("B", {layers_op(OpKind::op_or, {a}), op(OpKind::shrink, 25)});
    CHECK(generation_radius(f.style, f.compat) == 6);
  }
  SECTION("empty style still has radius 1") {
    GenFixture f;
    f.layer("A", {types_op(OpKind::op_or, {f.m1}), types_op(OpKind::op_and, {f.m2})});
    CHECK(generation_radius(f.style, f.compat) == 1);
  }
  SECTION("bridge width counted with magic-compat OFF (Q-MASK-404)") {
    GenFixture f;
    LayerOp bridge = op(OpKind::bridge, 20);
    bridge.values = {40};
    f.layer("A", {types_op(OpKind::op_or, {f.m1}), bridge});
    CHECK(generation_radius(f.style, f.compat) == 1);
    f.set_compat(false);
    CHECK(generation_radius(f.style, f.compat) == 9);
  }
  SECTION("bloat-max counted with magic-compat OFF (Q-MASK-404)") {
    GenFixture f;
    LayerOp bloat = op(OpKind::bloat_max);
    bloat.bloat = BloatTable{};
    bloat.bloat->distance[f.m1] = 30;
    f.layer("A", {types_op(OpKind::op_or, {f.m1}), bloat});
    CHECK(generation_radius(f.style, f.compat) == 1);
    f.set_compat(false);
    CHECK(generation_radius(f.style, f.compat) == 7);
  }
  SECTION("magic-compat ON keeps the style's stale radius (Q-MASK-18)") {
    GenFixture f;
    f.layer("A", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 30)});
    f.style.radius = 3;
    CHECK(generation_radius(f.style, f.compat) == 3);
    f.set_compat(false);
    CHECK(generation_radius(f.style, f.compat) == 7);
  }
}

TEST_CASE("cif see generates the layer and its dependencies",
          "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  GenFixture f;
  const std::size_t grown =
      f.layer("G", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 10)}, true);
  f.layer("OUT", {layers_op(OpKind::op_or, {grown})});
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, Rect{0, 0, 10, 10});
  auto tiles = see_tiles(f.source(), f.style, top, "OUT", Rect{-5, -5, 15, 15}, f.ctx);
  REQUIRE(tiles.ok());
  REQUIRE(tiles.value().size() == 1);
  CHECK(tiles.value().front().area == Rect{-10, -10, 110, 110});

  auto sel = select_layers(f.style, "OUT");
  REQUIRE(sel.ok());
  CHECK(sel.value().layers == std::vector<bool>{false, true});
  CHECK(sel.value().depend == std::vector<bool>{true, true});

  auto bad = see_tiles(f.source(), f.style, top, "NOPE", Rect{0, 0, 1, 1}, f.ctx);
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().code == "MASK-GEN-001");
  CHECK(bad.error().text ==
        "CIF name \"NOPE\" doesn't exist in style \"test\".\nThe valid CIF layer names are: G, OUT.");
}

TEST_CASE("cif see through the editor uses the current style",
          "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  GenFixture f;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, Rect{0, 0, 10, 10});
  auto none = see(f.ed, top, "CM1", Rect{0, 0, 10, 10}, f.ctx);
  REQUIRE_FALSE(none.ok());
  CHECK(none.error().code == "MASK-GEN-002");

  f.layer("CM1", {types_op(OpKind::op_or, {f.m1})});
  f.ed.mask.output_styles.styles.push_back(std::make_shared<const OutputStyle>(f.style));
  f.ed.mask.output_styles.current = 0;
  auto rects = see(f.ed, top, "CM1", Rect{0, 0, 10, 10}, f.ctx);
  REQUIRE(rects.ok());
  CHECK(rects.value() == Rects{Rect{0, 0, 100, 100}});
}

TEST_CASE("cif paint paints the layer back at internal scale",
          "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  GenFixture f;
  f.layer("G", {types_op(OpKind::op_or, {f.m1}), op(OpKind::grow, 15)});
  f.ed.mask.output_styles.styles.push_back(std::make_shared<const OutputStyle>(f.style));
  f.ed.mask.output_styles.current = 0;
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, Rect{0, 0, 10, 10});
  db::CellDef& target = f.cell("target");
  REQUIRE(paint(f.ed, top, "G", Rect{-5, -5, 15, 15}, f.m2, target, f.ctx).ok());
  // (-15,-15)-(115,115) truncates to (-1,-1)-(11,11).
  CHECK(target.bbox() == Rect{-1, -1, 11, 11});
}

TEST_CASE("cif coverage of a half-filled box", "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  GenFixture f;
  f.style.scale.scale_factor = 1;
  f.layer("CMF", {types_op(OpKind::op_or, {f.m1})});
  db::CellDef& top = f.cell("top");
  f.paint(top, f.m1, Rect{0, 0, 10, 5});
  auto c = coverage(f.source(), f.style, top, "CMF", Rect{0, 0, 10, 10}, f.ctx);
  REQUIRE(c.ok());
  CHECK(c.value().fraction() == 0.5F);
  CHECK(c.value().total == 100);
  CHECK(c.value().covered == 50);
  CHECK(coverage_report(c.value(), true) ==
        "Cursor Box Area = 100 CIF units^2\nLayer Bounding Area = 50 CIF units^2\n"
        "Layer Total Area = 50 CIF units^2\nCoverage in box = 50.0%");
}

TEST_CASE("cif coverage counts a triangle per Q-MASK-325",
          "[cap:cif-format-io][req:cif see, paint, coverage and statistics options]") {
  for (const bool on : {true, false}) {
    GenFixture f(on);
    f.style.scale.scale_factor = 1;
    f.layer("CMF", {types_op(OpKind::op_or, {f.m1})});
    db::CellDef& top = f.cell("top");
    REQUIRE(db::paint_type(top, *f.tech, Rect{0, 0, 10, 10}, f.m1,
                           db::TriangleSpec{db::Diagonal::ne_sw, true})
                .ok());
    db::recompute_bbox(top);
    auto c = coverage(f.source(), f.style, top, "CMF", Rect{0, 0, 10, 10}, f.ctx);
    REQUIRE(c.ok());
    CHECK(c.value().covered == (on ? 100 : 50));
  }
}

TEST_CASE("cif statistics prints and resets the recent counters",
          "[cap:mask-hierarchical-generation][req:Generation statistics]") {
  StatisticsCounters counters;
  counters.add(GenerationStatistics{5, 2, 3, 1, 1});
  const std::string first = statistics(counters);
  CHECK(first ==
        "CIF statistics (recent/total):\n    Geometrical tile operations: 5/5\n"
        "    Tile operations for hierarchy: 2/2\n    CIF rectangles output: 3/3\n"
        "    CIF rectangles due to hierarchical interactions: 1/1");
  const std::string second = statistics(counters);
  CHECK(second ==
        "CIF statistics (recent/total):\n    Geometrical tile operations: 0/5\n"
        "    Tile operations for hierarchy: 0/2\n    CIF rectangles output: 0/3\n"
        "    CIF rectangles due to hierarchical interactions: 0/1");
}

TEST_CASE("Generation counts its tile operations", "[cap:mask-hierarchical-generation][req:Generation statistics]") {
  GenFixture f;
  GapLayout g(f);
  (void)f.hierarchical(g.top);
  CHECK(f.ctx.statistics.flat_tiles > 0);
  CHECK(f.ctx.statistics.hier_tiles > 0);
  CHECK(f.ctx.statistics.hier_rects == 1);
  CHECK(f.ctx.statistics.interaction_areas >= 1);
}

}  // namespace hefesto::mask::test
