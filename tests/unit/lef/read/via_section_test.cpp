// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// VIA and VIARULE sections through the LEF reader (capability lef-read
// "LEF VIA and VIARULE definitions", "Via redefinition"; openspec/changes/
// m6-lef-def-oasis tasks.md 4.1, the parser-driven tests LEFVIA left open):
// the definition reaches LEFVIA's store through the reader, with
// Q-LEFDEF-7 and Q-LEFDEF-20 per mode.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/lef/via/generate.hpp"
#include "hefesto/lef/via/polygon.hpp"
#include "read_fixture.hpp"

using namespace lefread_test;

namespace {

/// The scenario's LEF names: met1, met2 (route) and the cut `via` (m2c).
void define_via_layers(FakeLayers& layers) {
  layers.define({"met1"}, lef::LayerEntry{lef::LayerClass::route, type_of("metal1"), 0, 0, std::nullopt});
  layers.define({"met2"}, lef::LayerEntry{lef::LayerClass::route, type_of("metal2"), 0, 0, std::nullopt});
  layers.define({"via"}, lef::LayerEntry{lef::LayerClass::cut, type_of("m2c"), 0, 0, std::nullopt});
  layers.define({"pwell"}, lef::LayerEntry{lef::LayerClass::ignore, 0, 0, 0, std::nullopt});
}

const lef::ViaDefinition& via(const LefRun& run, const std::string& name) {
  const auto* entry = run.vias.find(name);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.has_value());
  return *entry->value;
}

constexpr std::string_view kM1M2 =
    "VIA M1M2_PR DEFAULT\n"
    "  LAYER met1 ;\n"
    "  RECT -0.16 -0.13 0.16 0.13 ;\n"
    "  LAYER via ;\n"
    "  RECT -0.075 -0.075 0.075 0.075 ;\n"
    "  LAYER met2 ;\n"
    "  RECT -0.15 -0.15 0.15 0.15 ;\n"
    "END M1M2_PR\n";

}  // namespace

TEST_CASE("Contact type becomes primary", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  LefRun run{true, 0.005F};
  define_via_layers(run.layers);
  run.read(kM1M2);
  CHECK(run.texts().empty());
  const lef::ViaDefinition& v = via(run, "M1M2_PR");
  CHECK(v.type == type_of("m2c"));
  CHECK(v.area == db::Rect{-30, -30, 30, 30});
  const std::vector<lef::ViaShape> shapes{{type_of("metal2"), db::Rect{-60, -60, 60, 60}},
                                          {type_of("metal1"), db::Rect{-64, -52, 64, 52}}};
  CHECK(v.shapes == shapes);
  CHECK(v.canonical_name == "M1M2_PR");
}

TEST_CASE("VIARULE after VIA of the same name", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  LefRun run{true, 0.005F};
  define_via_layers(run.layers);
  run.read(std::string{kM1M2} +
           "VIARULE M1M2_PR\n  LAYER met1 ;\n  RECT -1 -1 1 1 ;\nEND M1M2_PR\n"
           "VIARULE GEN GENERATE\n  LAYER via ;\n  RECT -0.075 -0.075 0.075 0.075 ;\nEND GEN\n");
  CHECK(run.texts().empty());
  CHECK(via(run, "M1M2_PR").area == db::Rect{-30, -30, 30, 30});
  CHECK(via(run, "M1M2_PR").shapes.size() == 2);
  // A new VIARULE name is read as a via (lefRead.c:3058-3071).
  CHECK(via(run, "GEN").area == db::Rect{-30, -30, 30, 30});
}

TEST_CASE("RECT MASK in a via definition", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  const std::string text = "VIA V1\n  LAYER met1 ;\n  RECT MASK 1 -0.16 -0.13 0.16 0.13 ;\nEND V1\n";
  SECTION("magic-compat ON: a bad RECT") {
    LefRun run{true, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts() == Strings{"LEF read, Line 3 (Error): Bad port geometry: RECT requires 4 values."});
    CHECK(via(run, "V1").area == db::Rect{});
    CHECK_FALSE(via(run, "V1").type.has_value());
  }
  SECTION("magic-compat OFF: the mask number is ignored") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts().empty());
    CHECK(via(run, "V1").type == type_of("metal1"));
    CHECK(via(run, "V1").area == db::Rect{-64, -52, 64, 52});
  }
}

TEST_CASE("RECT on an ignored layer in a via definition", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  const std::string text = "VIA V1\n  LAYER pwell ;\n  RECT 0 0 1 1 ;\nEND V1\n";
  SECTION("magic-compat ON: No layer defined for RECT.") {
    LefRun run{true, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts() == Strings{"LEF read, Line 3 (Error): No layer defined for RECT."});
    CHECK(via(run, "V1").area == db::Rect{});
    CHECK(via(run, "V1").shapes.empty());
  }
  SECTION("magic-compat OFF: silent") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts().empty());
    CHECK(via(run, "V1").area == db::Rect{});
    CHECK(via(run, "V1").shapes.empty());
  }
  SECTION("an unknown layer reports the error in both modes") {
    for (const bool on : {true, false}) {
      LefRun run{on, 0.005F};
      define_via_layers(run.layers);
      run.read("VIA V1\n  LAYER bogus ;\n  RECT 0 0 1 1 ;\nEND V1\n");
      const auto texts = run.texts();
      REQUIRE(texts.size() == 3);
      CHECK(texts[2] == "LEF read, Line 3 (Error): No layer defined for RECT.");
    }
  }
}

TEST_CASE("RECT ITERATE in a via definition", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  const std::string text =
      "VIA V1\n  LAYER via ;\n  RECT ITERATE -0.05 -0.05 0.05 0.05 DO 2 BY 1 STEP 0.2 0 ;\nEND V1\n";
  SECTION("magic-compat ON: a bad RECT") {
    LefRun run{true, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts() == Strings{"LEF read, Line 3 (Error): Bad port geometry: RECT requires 4 values."});
    CHECK(via(run, "V1").area == db::Rect{});
  }
  SECTION("magic-compat OFF: the rectangle at each step") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts().empty());
    const lef::ViaDefinition& v = via(run, "V1");
    CHECK(v.area == db::Rect{-20, -20, 20, 20});
    const std::vector<lef::ViaShape> shapes{{type_of("m2c"), db::Rect{60, -20, 100, 20}}};
    CHECK(v.shapes == shapes);
  }
  SECTION("magic-compat OFF: a malformed step pattern adds nothing") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read("VIA V1\n  LAYER via ;\n  RECT ITERATE 0 0 1 1 DO 0 BY 1 STEP 1 1 ;\nEND V1\n");
    CHECK(run.texts() ==
          Strings{"LEF read, Line 3 (Error): Bad RECT ITERATE: requires DO <nx> BY <ny> STEP <dx> <dy>."});
    CHECK(via(run, "V1").area == db::Rect{});
  }
}

TEST_CASE("VIA POLYGON per mode", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  // An L: two rectangles in the decomposition.
  const std::string text =
      "VIA V1\n  LAYER met1 ;\n  POLYGON 0 0 0.02 0 0.02 0.01 0.01 0.01 0.01 0.02 0 0.02 ;\nEND V1\n";
  SECTION("magic-compat ON: an unknown keyword") {
    LefRun run{true, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts() == Strings{"LEF read, Line 3 (Message): Unknown keyword \"POLYGON\" in LEF file; ignoring."});
    CHECK(via(run, "V1").area == db::Rect{});
  }
  SECTION("magic-compat OFF: the decomposition's rectangles, as RECTs") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts().empty());
    const lef::ViaDefinition& v = via(run, "V1");
    const auto rects = lef::polygon_rects(std::vector<db::Point>{{0, 0}, {8, 0}, {8, 4}, {4, 4}, {4, 8}, {0, 8}});
    REQUIRE(rects.size() == 2);
    CHECK(v.type == type_of("metal1"));
    CHECK(v.area == rects[0]);
    REQUIRE(v.shapes.size() == 1);
    CHECK(v.shapes[0] == lef::ViaShape{type_of("metal1"), rects[1]});
  }
}

TEST_CASE("via-rule vias per mode", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  // s = 0.005: the generator's units are 1/10000 micron, 50 per internal
  // unit. CUTSIZE 0.02 (4 internal units), spacing 0.01, enclosures 0.005
  // (bottom) and 0.01 (top), 1 x 2 cuts.
  const std::string text =
      "VIA V12\n  VIARULE M1M2 ;\n  CUTSIZE 0.02 0.02 ;\n  LAYERS met1 via met2 ;\n  CUTSPACING 0.01 0.01 ;\n"
      "  ENCLOSURE 0.005 0.005 0.01 0.01 ;\n  ROWCOL 1 2 ;\nEND V12\n";
  SECTION("magic-compat ON: no geometry") {
    LefRun run{true, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    const auto texts = run.texts();
    REQUIRE(texts.size() == 5);  // VIARULE, CUTSIZE, LAYERS, CUTSPACING, ROWCOL
    CHECK(texts[0] == "LEF read, Line 2 (Message): Unknown keyword \"VIARULE\" in LEF file; ignoring.");
    const lef::ViaDefinition& v = via(run, "V12");
    CHECK_FALSE(v.type.has_value());
    CHECK(v.area == db::Rect{});
    CHECK(v.shapes.empty());
  }
  SECTION("magic-compat OFF: the generated geometry") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read(text);
    CHECK(run.texts().empty());
    lef::GeneratedViaParams p;
    p.layers = lef::ViaLayers{type_of("metal1"), type_of("m2c"), type_of("metal2")};
    p.cut_size = {200, 200};
    p.cut_spacing = {100, 100};
    p.bottom_enclosure = {50, 50};
    p.top_enclosure = {100, 100};
    p.rows = 1;
    p.columns = 2;
    p.scale = 50.0F;
    diag::NullSink quiet;
    const hefesto::compat::CompatSettings off{false, true};
    const lef::ViaGeometry expected =
        lef::ViaGenerator::generated(p, lef::ViaContext{*technology(), nullptr, off, quiet});
    const lef::ViaDefinition& v = via(run, "V12");
    CHECK(v.type == type_of("metal2"));
    CHECK(v.area == expected.area);
    CHECK(v.shapes == expected.shapes);
    CHECK(v.area == db::Rect{-14, -8, 14, 8});
    CHECK(v.generated);
  }
  SECTION("magic-compat OFF: an omitted parameter is 0, with the warning") {
    LefRun run{false, 0.005F};
    define_via_layers(run.layers);
    run.read("VIA V12\n  VIARULE M1M2 ;\n  CUTSIZE 0.02 0.02 ;\n  LAYERS met1 via met2 ;\n  ROWCOL 1 1 ;\nEND V12\n");
    const auto texts = run.texts();
    REQUIRE(texts.size() == 2);
    CHECK(texts[0].find("CUTSPACING") != std::string::npos);
    CHECK(texts[1].find("ENCLOSURE") != std::string::npos);
    CHECK(via(run, "V12").type == type_of("metal2"));
  }
}

TEST_CASE("Reading the same technology LEF twice", "[cap:lef-read][req:Via redefinition]") {
  LefRun run{true, 0.005F};
  define_via_layers(run.layers);
  run.read(kM1M2);
  const lef::ViaDefinition first = via(run, "M1M2_PR");
  run.texts();
  run.read(kM1M2);
  CHECK(run.texts() == Strings{"LEF read, Line 1 (Warning): Cut type \"M1M2_PR\" redefined."});
  CHECK(via(run, "M1M2_PR") == first);
}

TEST_CASE("a NONDEFAULTRULE USEVIA names a LEF via", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  LefRun run{true, 0.005F};
  define_via_layers(run.layers);
  run.read(kM1M2);
  run.read("NONDEFAULTRULE R\n  USEVIA M1M2_PR ;\nEND R\n");
  REQUIRE(run.rule("R").layers.size() == 1);
  CHECK(run.rule("R").layers[0].target == lef::RuleTarget::via);
}

TEST_CASE("a new VIA inside a NONDEFAULTRULE is defined", "[cap:lef-read][req:LEF VIA and VIARULE definitions]") {
  LefRun run{true, 0.005F};
  define_via_layers(run.layers);
  run.read("NONDEFAULTRULE R\n  VIA NV DEFAULT\n    LAYER via ;\n    RECT -0.075 -0.075 0.075 0.075 ;\n  END NV\nEND R\n");
  CHECK(run.texts().empty());
  CHECK(via(run, "NV").type == type_of("m2c"));
  CHECK(via(run, "NV").area == db::Rect{-30, -30, 30, 30});
  REQUIRE(run.rule("R").layers.size() == 1);
  CHECK(run.rule("R").layers[0].target == lef::RuleTarget::via);
}
