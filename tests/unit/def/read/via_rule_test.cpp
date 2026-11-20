// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read "Generated DEF vias": a DEF via naming a LEF VIARULE GENERATE
// takes the values it omits from the rule (Q-LEFDEF-61 fix, tasks.md 4.3),
// the generated-via cut cap (Q-LEFDEF-117, user decision D1 of
// 2026-11-19, security.md M6-S2), and via redefinition of one alias of a
// shared tech `lef` record (Q-LEFDEF-8 fix, tasks.md 4.1) through
// `lef read` and `def read`.
//
// Owner: DEFR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "def_fixture.hpp"

namespace def_read_test {
namespace {

const lef::ViaDefinition& via(Session& s, std::string_view name) {  // by value: GCC 14 -Wdangling-reference
  const auto* entry = s.editor.lefdef.vias.find(std::string{name});
  INFO("via " << name);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.has_value());
  return *entry->value;
}

std::string vias(std::string_view body, int count = 1) {
  return "DESIGN top ;\nVIAS " + std::to_string(count) + " ;\n" + std::string{body} + "\nEND VIAS\nEND DESIGN\n";
}

// No output style: LEF and DEF numbers are internal units (s = 1, oscale
// = 1), so the rule's values are the DEF values of vias_test.cpp's VG:
// cut 100 x 100, cut pitch 200 (spacing 100), enclosures (50, 20) below and
// (20, 50) above.
constexpr std::string_view kRule =
    "VERSION 5.7 ;\n"
    "VIARULE M1M2 GENERATE\n"
    "  LAYER met1 ;\n"
    "    ENCLOSURE 50 20 ;\n"
    "  LAYER met2 ;\n"
    "    ENCLOSURE 20 50 ;\n"
    "  LAYER via1 ;\n"
    "    RECT -50 -50 50 50 ;\n"
    "    SPACING 200 BY 200 ;\n"
    "END M1M2\n"
    "END LIBRARY\n";

constexpr std::string_view kFull =
    "- VG + VIARULE M1M2 + CUTSIZE 100 100 + LAYERS met1 via1 met2 + CUTSPACING 100 100 "
    "+ ENCLOSURE 50 20 20 50 + ROWCOL 1 2 ;";

void read_rule(Session& s) {
  s.editor.command_point = cmd::PointerState{};  // lef read needs a window or a point
  s.file("/t/rule.lef", std::string{kRule});
  REQUIRE(s.run("lef read /t/rule.lef").ok());
}

TEST_CASE("lef read keeps the parameters of a VIARULE GENERATE", "[cap:def-read][req:Generated DEF vias]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    read_rule(s);
    const lef::ViaDefinition& rule = via(s, "M1M2");
    REQUIRE(rule.rule.has_value());
    const lef::ViaRuleGenerate& g = *rule.rule;
    REQUIRE(g.layers.has_value());
    CHECK(g.layers->bottom == s.type("metal1"));
    CHECK(g.layers->cut == s.type("m2c"));
    CHECK(g.layers->top == s.type("metal2"));
    // 1/10000 micron, with one LEF micron one internal unit here.
    CHECK(g.cut_size == db::Point{1000000, 1000000});
    CHECK(g.cut_spacing == db::Point{1000000, 1000000});
    CHECK(g.bottom_enclosure == db::Point{500000, 200000});
    CHECK(g.top_enclosure == db::Point{200000, 500000});
    // The RECT is still the record's via geometry, as Magic reads it.
    CHECK(rule.type == s.type("m2c"));
  }
}

TEST_CASE("a DEF via naming a LEF VIARULE takes the values it omits (Q-LEFDEF-61)",
          "[cap:def-read][req:Generated DEF vias]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session full{on};
    REQUIRE(full.read(vias(kFull)).ok());
    const lef::ViaDefinition expected = via(full, "VG");

    Session s{on};
    read_rule(s);
    REQUIRE(s.read(vias("- VG + VIARULE M1M2 + CUTSIZE 100 100 + LAYERS met1 via1 met2 + ROWCOL 1 2 ;")).ok());
    CHECK(s.said("Via \"VG\": CUTSPACING not given; using the VIARULE value."));
    CHECK(s.said("Via \"VG\": ENCLOSURE not given; using the VIARULE value."));
    const lef::ViaDefinition& v = via(s, "VG");
    CHECK(v.type == expected.type);
    CHECK(v.area == expected.area);
    CHECK(v.shapes == expected.shapes);
  }
}

TEST_CASE("a DEF via naming only the rule and ROWCOL is generated from the rule",
          "[cap:def-read][req:Generated DEF vias]") {
  Session full{false};
  REQUIRE(full.read(vias(kFull)).ok());
  const lef::ViaDefinition expected = via(full, "VG");
  Session s{false};
  read_rule(s);
  REQUIRE(s.read(vias("- VG + VIARULE M1M2 + ROWCOL 1 2 ;")).ok());
  CHECK(s.said("Via \"VG\": LAYERS not given; using the VIARULE value."));
  CHECK(s.said("Via \"VG\": CUTSIZE not given; using the VIARULE value."));
  CHECK(via(s, "VG").area == expected.area);
  CHECK(via(s, "VG").shapes == expected.shapes);
}

TEST_CASE("a VIARULE that names no known rule leaves omitted values 0", "[cap:def-read][req:Generated DEF vias]") {
  Session s{false};
  read_rule(s);
  REQUIRE(s.read(vias("- VX + VIARULE NOSUCH + CUTSIZE 100 100 + LAYERS met1 via1 met2 ;")).ok());
  CHECK(s.said("Via \"VX\": CUTSPACING not given; using 0."));
  CHECK(s.said("Via \"VX\": ENCLOSURE not given; using 0."));
}

TEST_CASE("regression: a generated via above 1000000 cuts is refused (Q-LEFDEF-117)",
          "[cap:def-read][req:Generated DEF vias]") {
  // `+ ROWCOL 100000 100000` made the generator build 10^10 cuts (Magic runs
  // out of memory). Refused in both modes, with no generated geometry.
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    REQUIRE(s.read(vias("- VBIG + CUTSIZE 1 1 + LAYERS met1 via1 met2 + CUTSPACING 1 1 + ENCLOSURE 0 0 0 0 "
                        "+ ROWCOL 100000 100000 ;\n"
                        "- VEDGE + CUTSIZE 1 1 + LAYERS met1 via1 met2 + CUTSPACING 1 1 + ENCLOSURE 0 0 0 0 "
                        "+ ROWCOL 1001 1000 ;",
                        2))
                .ok());
    CHECK(s.said("Via \"VBIG\": ROWCOL 100000 100000 gives more than 1000000 cuts; the via is not generated."));
    CHECK(s.said("Via \"VEDGE\": ROWCOL 1001 1000 gives more than 1000000 cuts; the via is not generated."));
    CHECK_FALSE(via(s, "VBIG").generated);
    CHECK(via(s, "VBIG").shapes.empty());
    CHECK_FALSE(via(s, "VEDGE").generated);
  }
}

TEST_CASE("a generated via within the cut cap is generated", "[cap:def-read][req:Generated DEF vias]") {
  Session s{false};
  REQUIRE(s.read(vias("- VOK + CUTSIZE 1 1 + LAYERS met1 via1 met2 + CUTSPACING 1 1 + ENCLOSURE 0 0 0 0 "
                      "+ ROWCOL 10 100 ;"))
              .ok());
  CHECK_FALSE(s.said("the via is not generated."));
  CHECK(via(s, "VOK").generated);
  CHECK(via(s, "VOK").shapes.size() == 1001U);  // 1000 cuts and the bottom rectangle
}

// --- Via redefinition of one alias of a shared tech lef record (Q-LEFDEF-8) ------------

constexpr std::string_view kAliased = R"(
lef
    routing metal1 met1
    routing metal2 met2
    cut m2c via1 V1 VIA12
    obstruction obsm1 met1
end
)";

TEST_CASE("a LEF VIA named after the canonical name of a shared cut keeps the other names on the layer",
          "[cap:lef-read][req:Via redefinition]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on, {}, kAliased};
    const lef::LayerRecord* cut = s.editor.lefdef.layers.find("V1");
    REQUIRE(cut != nullptr);
    REQUIRE(cut->canonical_name == "via1");
    s.editor.command_point = cmd::PointerState{};
    s.file("/t/v.lef", "VERSION 5.7 ;\nVIA via1\n  LAYER met1 ;\n  RECT -10 -10 10 10 ;\nEND via1\nEND LIBRARY\n");
    const auto result = s.run("lef read /t/v.lef");
    REQUIRE(result.ok());
    CHECK(via(s, "via1").layer_class == lef::LayerClass::cut);
    CHECK(via(s, "via1").type == s.type("metal1"));
    const auto& layers = s.editor.lefdef.layers;
    CHECK(layers.find("via1") == nullptr);
    REQUIRE(layers.find("V1") != nullptr);
    CHECK(layers.find("V1") == layers.find("VIA12"));
    const std::string canonical = layers.find("V1")->canonical_name;
    CHECK((canonical == "V1" || canonical == "VIA12"));
    CHECK(layers.find("V1")->type == s.type("m2c"));
  }
}

TEST_CASE("a DEF via named after one alias of a shared cut leaves the layer to the other names",
          "[cap:def-read][req:Via redefinition]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on, {}, kAliased};
    REQUIRE(s.read(vias("- VIA12 + RECT met2 ( -10 -10 ) ( 10 10 ) ;")).ok());
    CHECK(s.said("Composite via \"VIA12\" redefined."));
    CHECK(via(s, "VIA12").type == s.type("metal2"));
    const auto& layers = s.editor.lefdef.layers;
    CHECK(layers.find("VIA12") == nullptr);
    REQUIRE(layers.find("via1") != nullptr);
    CHECK(layers.find("via1")->canonical_name == "via1");  // not the redefined name: kept
    CHECK(layers.find("V1") == layers.find("via1"));
  }
}

}  // namespace
}  // namespace def_read_test
