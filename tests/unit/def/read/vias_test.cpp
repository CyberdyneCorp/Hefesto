// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: the VIAS section through the shared via code (tasks.md 7.3;
// Q-LEFDEF-60, 61, 62), including the def-read tests LEFVIA left open:
// generated DEF vias per mode, swapped LAYERS through the parser, the VIAS
// ORIGIN message per mode, and a DEF via overriding a LEF via end to end
// (the LEF via built through lef::begin_via while LEFR's VIA section is a
// stub).
//
// Owner: DEFR (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "def_fixture.hpp"
#include "hefesto/lef/via/store.hpp"

namespace def_read_test {
namespace {

const lef::ViaDefinition& via(Session& s, const std::string& name) {
  const auto* entry = s.editor.lefdef.vias.find(name);
  INFO("via " << name);
  REQUIRE(entry != nullptr);
  REQUIRE(entry->value.has_value());
  return *entry->value;
}

std::string vias(std::string_view body, int count = 1) {
  return "DESIGN top ;\nVIAS " + std::to_string(count) + " ;\n" + std::string{body} + "\nEND VIAS\nEND DESIGN\n";
}

constexpr std::string_view kGenerated =
    "- VG + VIARULE M1M2 + CUTSIZE 100 100 + LAYERS met1 via1 met2 + CUTSPACING 100 100 "
    "+ ENCLOSURE 50 20 20 50 + ROWCOL 1 2 ;";

// --- VIAS section grammar -----------------------------------------------------------

TEST_CASE("VIAS ORIGIN per mode (Q-LEFDEF-62)", "[cap:def-read][req:VIAS section grammar]") {
  const std::string text = vias("- VO + RECT met1 ( -10 -10 ) ( 10 10 ) + ORIGIN 10 10 ;");
  SECTION("ON: an unknown property") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Unknown via property \"ORIGIN\" in VIAS definition; ignoring."));
    CHECK(via(s, "VO").area == db::Rect{-20, -20, 20, 20});
  }
  SECTION("OFF: the via moves") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.said("Unknown via property \"ORIGIN\" in VIAS definition; ignoring."));
    CHECK(via(s, "VO").area == db::Rect{0, 0, 40, 40});
  }
}

TEST_CASE("VIAS statements, bad values and the count", "[cap:def-read][req:VIAS section grammar]") {
  Session s;
  REQUIRE(s.read(vias("FOO bar ;\n- VB + ROWCOL x 2 + CUTSIZE 10 10 + LAYERS met1 via1 met2 + CUTSPACING 0 0 "
                      "+ ENCLOSURE 0 0 0 0 ;",
                      2))
              .ok());
  CHECK(s.said("Unknown keyword \"FOO\" in VIAS definition; ignoring."));
  CHECK(s.said("Invalid syntax for ROWCOL."));
  CHECK(s.said("Number of vias read (1) does not match the number declared (2)."));
  // ROWCOL x 2: one row (the failed value is 1), two columns.
  CHECK(via(s, "VB").shapes.size() == 3);
}

TEST_CASE("VIAS POLYGON and MASK with magic-compat OFF", "[cap:def-read][req:VIAS section grammar]") {
  Session s{false};
  REQUIRE(s.read(vias("- VP + POLYGON met2 ( 0 0 ) ( 20 0 ) ( 20 10 ) ( 10 10 ) ( 10 20 ) ( 0 20 ) + MASK 1 ;")).ok());
  const lef::ViaDefinition& v = via(s, "VP");
  CHECK(v.type == s.type("metal2"));
  CHECK(v.shapes.size() == 1);
  CHECK_FALSE(s.said("ignoring."));
}

// --- Rectangle-based DEF vias -------------------------------------------------------

TEST_CASE("three-rectangle via", "[cap:def-read][req:Rectangle-based DEF vias]") {
  Session s;
  REQUIRE(s.read(vias("- V12 + RECT met1 ( -100 -100 ) ( 100 100 ) + RECT via1 ( -50 -50 ) ( 50 50 ) "
                      "+ RECT met2 ( -120 -80 ) ( 120 80 ) ;"))
              .ok());
  const lef::ViaDefinition& v = via(s, "V12");
  CHECK(v.type == s.type("m2c"));
  CHECK(v.area == db::Rect{-100, -100, 100, 100});
  REQUIRE(v.shapes.size() == 2);
  CHECK(v.shapes[0] == lef::ViaShape{s.type("metal2"), db::Rect{-240, -160, 240, 160}});
  CHECK(v.shapes[1] == lef::ViaShape{s.type("metal1"), db::Rect{-200, -200, 200, 200}});
  CHECK(s.said("  Processed 1 vias total."));
}

TEST_CASE("a via rectangle on an unknown layer adds nothing", "[cap:def-read][req:Rectangle-based DEF vias]") {
  Session s;
  REQUIRE(s.read(vias("- VU + RECT met9 ( 0 0 ) ( 10 10 ) + RECT met1 ( 0 0 ) ( 10 10 ) ;")).ok());
  CHECK(s.said("Don't know how to parse layer \"met9\""));
  CHECK(s.said("No layer defined for RECT."));
  const lef::ViaDefinition& v = via(s, "VU");
  CHECK(v.type == s.type("metal1"));
  CHECK(v.shapes.empty());
  // Shared LEF helpers report under "LEF read".
  CHECK(std::any_of(s.messages.begin(), s.messages.end(), [](const diag::Diagnostic& d) {
    return d.text.starts_with("LEF read, Line 3 (Error): Don't know how to parse layer");
  }));
}

// --- Generated DEF vias -------------------------------------------------------------

TEST_CASE("1x2 generated via", "[cap:def-read][req:Generated DEF vias]") {
  Session s;
  REQUIRE(s.read(vias(kGenerated)).ok());
  const lef::ViaDefinition& v = via(s, "VG");
  CHECK(v.generated);
  CHECK(v.type == s.type("metal2"));
  CHECK(v.area == db::Rect{-340, -200, 340, 200});
  REQUIRE(v.shapes.size() == 3);
  CHECK(v.shapes[0] == lef::ViaShape{s.type("m2c"), db::Rect{100, -100, 300, 100}});
  CHECK(v.shapes[1] == lef::ViaShape{s.type("m2c"), db::Rect{-300, -100, -100, 100}});
  CHECK(v.shapes[2] == lef::ViaShape{s.type("metal1"), db::Rect{-400, -140, 400, 140}});
}

TEST_CASE("swapped LAYERS through the parser", "[cap:def-read][req:Generated DEF vias]") {
  Session s;
  REQUIRE(s.read(vias("- VS + CUTSIZE 100 100 + LAYERS met1 met2 via1 + CUTSPACING 0 0 + ENCLOSURE 0 0 0 0 ;")).ok());
  CHECK(std::any_of(s.messages.begin(), s.messages.end(), [](const diag::Diagnostic& d) {
    return d.text.starts_with("DEF read, Line 3 (Warning): Improper layer order for VIARULE.");
  }));
  const lef::ViaDefinition& v = via(s, "VS");
  CHECK(v.type == s.type("metal2"));
  REQUIRE(v.shapes.size() == 2);
  CHECK(v.shapes[0].type == s.type("m2c"));
  CHECK(v.shapes[1].type == s.type("metal1"));
}

TEST_CASE("no cut layer in LAYERS", "[cap:def-read][req:Generated DEF vias]") {
  Session s;
  REQUIRE(s.read(vias("- VN + CUTSIZE 10 10 + LAYERS met1 met2 met1 + CUTSPACING 0 0 + ENCLOSURE 0 0 0 0 ;")).ok());
  CHECK(s.said("Improper layer order for VIARULE."));
  CHECK(s.said("No cut layer specified in VIARULE."));
}

TEST_CASE("generated condition after a generated via per mode (Q-LEFDEF-60)",
          "[cap:def-read][req:Generated DEF vias]") {
  const std::string text = vias(std::string{kGenerated} + "\n- VR + RECT met1 ( -10 -10 ) ( 10 10 ) ;", 2);
  SECTION("ON: VR is generated from VG's parameters") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    const lef::ViaDefinition& v = via(s, "VR");
    CHECK(v.generated);
    CHECK(v.type == s.type("metal2"));
    // ROWCOL resets to 1 x 1 at each via: one cut 100 wide. The generated
    // top layer replaces the area the RECT had set (LefGenViaGeometry).
    CHECK(v.area == db::Rect{-140, -200, 140, 200});
    REQUIRE(v.shapes.size() == 2);
    CHECK(v.shapes[1].type == s.type("metal1"));
    CHECK(v.shapes[1].area == db::Rect{-200, -140, 200, 140});
  }
  SECTION("OFF: VR holds its rectangle only") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    const lef::ViaDefinition& v = via(s, "VR");
    CHECK_FALSE(v.generated);
    CHECK(v.type == s.type("metal1"));
    CHECK(v.area == db::Rect{-20, -20, 20, 20});
    CHECK(v.shapes.empty());
  }
}

TEST_CASE("omitted parameters are 0 with a warning (Q-LEFDEF-61)", "[cap:def-read][req:Generated DEF vias]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read(vias("- VX + CUTSIZE 100 100 + LAYERS met1 via1 met2 ;")).ok());
    CHECK(s.said("Via \"VX\": CUTSPACING not given; using 0."));
    CHECK(s.said("Via \"VX\": ENCLOSURE not given; using 0."));
    CHECK(via(s, "VX").area == db::Rect{-100, -100, 100, 100});
  }
}

TEST_CASE("ON keeps the last parameters; OFF warns for each via (Q-LEFDEF-60, 61)",
          "[cap:def-read][req:Generated DEF vias]") {
  const std::string text = vias(std::string{kGenerated} + "\n- VY + CUTSIZE 10 10 ;", 2);
  SECTION("ON") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.said("not given; using 0."));
    CHECK(via(s, "VY").type == s.type("metal2"));
  }
  SECTION("OFF") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Via \"VY\": LAYERS not given; using 0."));
    CHECK_FALSE(via(s, "VY").type.has_value());
  }
}

TEST_CASE("generated vias honour OFFSET and PATTERN only with magic-compat OFF (Q-LEFDEF-62)",
          "[cap:def-read][req:Generated DEF vias]") {
  const std::string text = vias(std::string{kGenerated.substr(0, kGenerated.size() - 1)} +
                                "+ OFFSET 0 0 10 0 + PATTERN 1_8 ;");
  SECTION("ON") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Unknown via property \"OFFSET\" in VIAS definition; ignoring."));
    CHECK(s.said("Unknown via property \"PATTERN\" in VIAS definition; ignoring."));
    CHECK(via(s, "VG").shapes.size() == 3);
  }
  SECTION("OFF") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    const lef::ViaDefinition& v = via(s, "VG");
    CHECK(v.area == db::Rect{-320, -200, 360, 200});  // top moved by 10 DEF units
    CHECK(v.shapes.size() == 2);                       // pattern 8 = 1000: only the left cut
  }
}

// --- Via redefinition ---------------------------------------------------------------

TEST_CASE("DEF overrides LEF via end to end", "[cap:def-read][req:Via redefinition]") {
  Session s;
  diag::CollectingSink sink;
  const hefesto::compat::CompatSettings compat{};
  const lef::ViaContext context{*s.editor.technology, nullptr, compat, sink};
  lef::ViaDefinition* lef_via =
      lef::begin_via(s.editor.lefdef.vias, s.editor.lefdef.layers, "V12", lef::ViaSection::lef_via, context);
  REQUIRE(lef_via != nullptr);
  lef::add_via_rect(*lef_via, s.type("metal2"), db::Rect{-1000, -1000, 1000, 1000}, context);
  REQUIRE(s.read("DESIGN top ;\nVIAS 1 ;\n- V12 + RECT met1 ( -100 -100 ) ( 100 100 ) ;\nEND VIAS\n"
                 "NETS 1 ;\n- n + ROUTED met1 ( 0 0 ) V12 ;\nEND NETS\nEND DESIGN\n")
              .ok());
  CHECK(s.said("Composite via \"V12\" redefined."));
  CHECK(s.painted("top", "metal2").empty());
  CHECK(s.covers("top", "metal1", db::Rect{-100, -100, 100, 100}));
}

TEST_CASE("redefining a via of the same read", "[cap:def-read][req:Via redefinition]") {
  Session s;
  REQUIRE(s.read(vias("- VA + RECT met1 ( 0 0 ) ( 10 10 ) ;\n- VA + RECT met2 ( 0 0 ) ( 20 20 ) ;", 2)).ok());
  CHECK(s.said("Composite via \"VA\" redefined."));
  const lef::ViaDefinition& v = via(s, "VA");
  CHECK(v.type == s.type("metal2"));
  CHECK(v.area == db::Rect{0, 0, 40, 40});
}

}  // namespace
}  // namespace def_read_test
