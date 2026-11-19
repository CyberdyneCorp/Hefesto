// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared via generator: geometry from VIARULE parameters, the LAYERS
// order check, omitted parameters (Q-LEFDEF-61), and ORIGIN, OFFSET and
// PATTERN with magic-compat OFF only (Q-LEFDEF-62)
// (capabilities lef-read "Generated via geometry", def-read "Generated DEF
// vias"; openspec/changes/m6-lef-def-oasis tasks.md 4.3).
//
// Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <vector>

#include "hefesto/lef/via/generate.hpp"
#include "hefesto/lef/via/store.hpp"
#include "via_fixture.hpp"

namespace hefesto::lef {
namespace {

using test::Context;

ViaLayers m1_m2(Context& c) {
  return ViaLayers{c.type("metal1"), c.type("m2contact"), c.type("metal2")};
}

/// The DEF scenario "1x2 generated via": CUTSIZE 100 100, CUTSPACING 100
/// 100, ENCLOSURE 50 20 20 50, ROWCOL 1 2, oscale 1.
GeneratedViaParams def_vg(Context& c) {
  GeneratedViaParams p;
  p.layers = m1_m2(c);
  p.cut_size = {100, 100};
  p.cut_spacing = {100, 100};
  p.bottom_enclosure = {50, 20};
  p.top_enclosure = {20, 50};
  p.rows = 1;
  p.columns = 2;
  return p;
}

TEST_CASE("a 1 x 2 cut array", "[cap:lef-read][req:Generated via geometry]") {
  Context c{test::via_tech()};
  GeneratedViaParams p;
  p.layers = m1_m2(c);
  p.cut_size = {4, 4};
  p.cut_spacing = {2, 2};
  p.bottom_enclosure = {1, 1};
  p.top_enclosure = {2, 2};
  p.rows = 1;
  p.columns = 2;
  const ViaGeometry g = ViaGenerator::generated(p, c.via());
  CHECK(g.type == c.type("metal2"));
  CHECK(g.area == db::Rect{-14, -8, 14, 8});
  const std::vector<ViaShape> shapes{{c.type("m2contact"), db::Rect{2, -4, 10, 4}},
                                     {c.type("m2contact"), db::Rect{-10, -4, -2, 4}},
                                     {c.type("metal1"), db::Rect{-12, -6, 12, 6}}};
  CHECK(g.shapes == shapes);
  CHECK(c.texts().empty());
}

TEST_CASE("a 1x2 generated DEF via", "[cap:def-read][req:Generated DEF vias]") {
  Context c{test::via_tech()};
  const ViaGeometry g = ViaGenerator::generated(def_vg(c), c.via());
  CHECK(g.type == c.type("metal2"));
  CHECK(g.area == db::Rect{-340, -200, 340, 200});
  const std::vector<ViaShape> shapes{{c.type("m2contact"), db::Rect{100, -100, 300, 100}},
                                     {c.type("m2contact"), db::Rect{-300, -100, -100, 100}},
                                     {c.type("metal1"), db::Rect{-400, -140, 400, 140}}};
  CHECK(g.shapes == shapes);
}

TEST_CASE("generated geometry rounds in binary32 at the DEF scale", "[cap:def-read][req:Generated DEF vias]") {
  // oscale = 3: W = 2*1 + 0 + 2*1 = 4, round(4/3) = 1; cuts at half scale 1.5.
  Context c{test::via_tech()};
  GeneratedViaParams p;
  p.layers = m1_m2(c);
  p.cut_size = {2, 2};
  p.top_enclosure = {1, 1};
  p.scale = 3.0F;
  const ViaGeometry g = ViaGenerator::generated(p, c.via());
  CHECK(g.area == db::Rect{-1, -1, 1, 1});
  // x0 = trunc(-2/2) = -1: round(-1/1.5) = -1, size round(2/1.5) = 1.
  REQUIRE(g.shapes.size() == 2);
  CHECK(g.shapes.front() == ViaShape{c.type("m2contact"), db::Rect{-1, -1, 0, 0}});
}

TEST_CASE("generated cuts grow to the output style's contact size", "[cap:def-read][req:Generated DEF vias]") {
  const auto technology = test::via_tech();
  const tech::TileType via = test::type_of(*technology, "m2contact");
  const mask::OutputStyle style = test::squares_style(via, 1, 1, 2, 2);
  for (const bool compat : {true, false}) {
    Context c{technology, compat, &style};
    GeneratedViaParams p;
    p.layers = m1_m2(c);
    p.cut_size = {3, 3};
    const ViaGeometry g = ViaGenerator::generated(p, c.via());
    // The cut (-2,-2,4,4) half units (x0 = trunc(-3/2) = -1) grows to 8 x 8
    // about its centre (1, 1).
    CHECK(g.shapes.front() == ViaShape{via, db::Rect{-3, -3, 5, 5}});
    // Magic names the entry's normal type, the top layer (ON).
    const std::string named = compat ? "metal2" : "m2contact";
    CHECK(c.texts() == std::vector<std::string>{
                           "Cut size for magic type \"" + named + "\" (4 x 4) does not match LEF/DEF",
                           "Via cut size (6 x 6).  Magic layer cut size will be used!"});
  }
}

TEST_CASE("an improper LAYERS order is reported and repaired", "[cap:def-read][req:Generated DEF vias]") {
  Context c{test::via_tech()};
  ViaLayers swapped{c.type("metal1"), c.type("metal2"), c.type("m2contact")};
  order_layers(swapped, c.via());
  CHECK(swapped == m1_m2(c));
  CHECK(c.texts() == std::vector<std::string>{"Improper layer order for VIARULE."});

  ViaLayers cut_first{c.type("m2contact"), c.type("metal1"), c.type("metal2")};
  order_layers(cut_first, c.via());
  CHECK(cut_first == ViaLayers{c.type("metal1"), c.type("m2contact"), c.type("metal2")});
  CHECK(c.texts().size() == 1);

  ViaLayers none{c.type("metal1"), c.type("metal2"), c.type("ndiff")};
  order_layers(none, c.via());
  CHECK(none == ViaLayers{c.type("metal1"), c.type("metal2"), c.type("ndiff")});
  CHECK(c.texts() == std::vector<std::string>{"Improper layer order for VIARULE.",
                                              "No cut layer specified in VIARULE."});

  ViaLayers fine = m1_m2(c);
  order_layers(fine, c.via());
  CHECK(fine == m1_m2(c));
  CHECK(c.texts().empty());
}

TEST_CASE("generated geometry replaces the primary record and keeps earlier rectangles",
          "[cap:def-read][req:Generated DEF vias]") {
  // Magic-compat ON: the generated condition persists, so a later RECT-only
  // via gets the geometry of the last parameters (the reader calls this).
  Context c{test::via_tech()};
  ViaTable vias;
  ViaDefinition* vr = begin_via(vias, LayerTable{}, "VR", ViaSection::def_via, c.via());
  add_via_rect(*vr, c.type("metal1"), db::Rect{-20, -20, 20, 20}, c.via());
  add_via_rect(*vr, c.type("metal3"), db::Rect{-30, -30, 30, 30}, c.via());
  const ViaGeometry g = ViaGenerator::generated(def_vg(c), c.via());
  apply_generated(*vr, g);
  CHECK(vr->generated);
  CHECK(vr->type == c.type("metal2"));
  CHECK(vr->area == db::Rect{-340, -200, 340, 200});
  REQUIRE(vr->shapes.size() == 4);
  CHECK(vr->shapes.front() == g.shapes.front());
  CHECK(vr->shapes.back() == ViaShape{c.type("metal3"), db::Rect{-30, -30, 30, 30}});
}

TEST_CASE("omitted generation parameters take the VIARULE values or 0 with a warning",
          "[cap:def-read][req:Generated DEF vias]") {
  Context c{test::via_tech()};
  ViaParamSet given;
  given.layers = m1_m2(c);
  given.cut_size = db::Point{100, 100};
  given.rows = 1;
  given.columns = 2;
  const GeneratedViaParams no_rule = complete_params(given, nullptr, "VG", c.via());
  CHECK(no_rule.cut_spacing == db::Point{0, 0});
  CHECK(no_rule.bottom_enclosure == db::Point{0, 0});
  CHECK(no_rule.cut_size == db::Point{100, 100});
  CHECK(no_rule.columns == 2);
  const auto diagnostics = c.sink.take();
  REQUIRE(diagnostics.size() == 2);
  CHECK(diagnostics[0].text == "Via \"VG\": CUTSPACING not given; using 0.");
  CHECK(diagnostics[1].text == "Via \"VG\": ENCLOSURE not given; using 0.");
  CHECK(diagnostics[0].native_only);

  const GeneratedViaParams rule = def_vg(c);
  const GeneratedViaParams from_rule = complete_params(given, &rule, "VG", c.via());
  CHECK(from_rule.cut_spacing == db::Point{100, 100});
  CHECK(from_rule.bottom_enclosure == db::Point{50, 20});
  CHECK(from_rule.top_enclosure == db::Point{20, 50});
  CHECK(c.texts() == std::vector<std::string>{"Via \"VG\": CUTSPACING not given; using the VIARULE value.",
                                              "Via \"VG\": ENCLOSURE not given; using the VIARULE value."});
  CHECK(ViaGenerator::generated(from_rule, c.via()) == ViaGenerator::generated(rule, c.via()));
}

TEST_CASE("ORIGIN and OFFSET apply with magic-compat OFF only", "[cap:def-read][req:Generated DEF vias]") {
  for (const bool compat : {true, false}) {
    Context c{test::via_tech(), compat};
    GeneratedViaParams p = def_vg(c);
    const ViaGeometry plain = ViaGenerator::generated(p, c.via());
    p.origin = {10, 10};
    p.bottom_offset = {5, 0};
    p.top_offset = {0, -5};
    const ViaGeometry g = ViaGenerator::generated(p, c.via());
    if (compat) {
      CHECK(g == plain);  // Magic ignores the properties (Q-LEFDEF-62 ON)
      continue;
    }
    // Half units: ORIGIN (20, 20) everywhere, OFFSET bottom (10, 0), top (0, -10).
    CHECK(g.area == db::Rect{-320, -190, 360, 210});
    CHECK(g.shapes.back() == ViaShape{c.type("metal1"), db::Rect{-370, -120, 430, 160}});
    CHECK(g.shapes.front() == ViaShape{c.type("m2contact"), db::Rect{120, -80, 320, 120}});
  }
}

TEST_CASE("PATTERN removes cuts with magic-compat OFF only", "[cap:def-read][req:Generated DEF vias]") {
  for (const bool compat : {true, false}) {
    Context c{test::via_tech(), compat};
    GeneratedViaParams p = def_vg(c);
    p.pattern = "1_8";  // one row: the left cut only
    const ViaGeometry g = ViaGenerator::generated(p, c.via());
    const std::size_t cuts = compat ? 2 : 1;
    REQUIRE(g.shapes.size() == cuts + 1);
    // The list head is the last cut generated: the right one when both are.
    CHECK(g.shapes.front().area == (compat ? db::Rect{100, -100, 300, 100} : db::Rect{-300, -100, -100, 100}));
  }
  Context c{test::via_tech(), false};
  GeneratedViaParams p = def_vg(c);
  p.pattern = "2_F";  // two rows for a one-row array
  CHECK(ViaGenerator::generated(p, c.via()).shapes.size() == 3);
  CHECK(c.texts() == std::vector<std::string>{"Cannot parse via PATTERN \"2_F\"; every cut is kept."});
}

TEST_CASE("cut patterns parse DEF 5.8 row groups", "[cap:def-read][req:Generated DEF vias]") {
  using Rows = std::vector<std::vector<bool>>;
  CHECK(cut_pattern("1_F", 1, 4) == Rows{{true, true, true, true}});
  CHECK(cut_pattern("1_A_1_5", 2, 4) == Rows{{true, false, true, false}, {false, true, false, true}});
  CHECK(cut_pattern("2_R2F", 2, 6) == Rows{std::vector<bool>(6, true), std::vector<bool>(6, true)});
  CHECK(cut_pattern("1_c", 1, 2) == Rows{{true, true}});
  CHECK_FALSE(cut_pattern("1_F", 2, 4).has_value());    // too few rows
  CHECK_FALSE(cut_pattern("1_F", 1, 5).has_value());    // too few columns
  CHECK_FALSE(cut_pattern("1_FF", 1, 4).has_value());   // too many digits
  CHECK_FALSE(cut_pattern("1_G", 1, 4).has_value());
  CHECK_FALSE(cut_pattern("1", 1, 4).has_value());
  CHECK_FALSE(cut_pattern("0_F", 1, 4).has_value());
  CHECK_FALSE(cut_pattern("1_R999999999F", 1, 4).has_value());
}

TEST_CASE("a layer that did not resolve gives no shapes", "[cap:def-read][req:Generated DEF vias]") {
  Context c{test::via_tech()};
  GeneratedViaParams p = def_vg(c);
  p.layers = ViaLayers{std::nullopt, std::nullopt, c.type("metal2")};
  const ViaGeometry g = ViaGenerator::generated(p, c.via());
  CHECK(g.type == c.type("metal2"));
  CHECK(g.shapes.empty());
  p.scale = 0.0F;
  CHECK(ViaGenerator::generated(p, c.via()).area == db::Rect{});
}

TEST_CASE("regression: a generated via too large for a coordinate has no geometry",
          "[cap:def-read][req:Generated DEF vias]") {
  // CUTSIZE x ROWCOL of 2^32 input units: Magic's (int)roundf is undefined
  // there (fixed in both modes); the via gets no geometry.
  for (const bool compat : {true, false}) {
    Context c{test::via_tech(), compat};
    GeneratedViaParams p;
    p.layers = m1_m2(c);
    p.cut_size = {1 << 30, 2};
    p.rows = 1;
    p.columns = 4;
    const ViaGeometry g = ViaGenerator::generated(p, c.via());
    CHECK(g.type == c.type("metal2"));
    CHECK(g.area == db::Rect{});
    CHECK(g.shapes.empty());
  }
}

TEST_CASE("regression: a generated cut whose edge overflows is dropped", "[cap:def-read][req:Generated DEF vias]") {
  // Four 2^29 cuts with enclosure -2^29: the enclosure fits, but the last
  // cut's x0 + size is 2^31 half units (an int overflow in Magic).
  Context c{test::via_tech()};
  GeneratedViaParams p;
  p.layers = m1_m2(c);
  p.cut_size = {1 << 29, 2};
  p.bottom_enclosure = {-(1 << 29), 1};
  p.top_enclosure = {-(1 << 29), 1};
  p.rows = 1;
  p.columns = 4;
  const ViaGeometry g = ViaGenerator::generated(p, c.via());
  CHECK(g.area == db::Rect{-(1 << 30), -4, 1 << 30, 4});
  std::size_t cuts = 0;
  for (const ViaShape& s : g.shapes) {
    cuts += s.type == c.type("m2contact") ? 1U : 0U;
  }
  CHECK(cuts == 3);
}

TEST_CASE("regression: an ORIGIN past the coordinate range saturates the shift",
          "[cap:def-read][req:Generated DEF vias]") {
  Context c{test::via_tech(), false};
  GeneratedViaParams p = def_vg(c);
  // 2^29 input units each: 2^30 half units, which fit; their sum does not.
  p.origin = {std::numeric_limits<db::Coord>::max() / 4, 0};
  p.top_offset = {std::numeric_limits<db::Coord>::max() / 4, 0};
  const ViaGeometry g = ViaGenerator::generated(p, c.via());
  CHECK(g.area.x1 == std::numeric_limits<db::Coord>::max());
}

}  // namespace
}  // namespace hefesto::lef
