// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: tokenization, the top-level grammar, unit scaling, the design
// cell and the die area (tasks.md 7.1, 7.2; Q-LEFDEF-77, 78, 79, 84, 68).
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "def_fixture.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/lef/read/units.hpp"

namespace def_read_test {
namespace {

std::vector<std::int32_t> die(Session& s, std::string_view cell) {
  const auto* value = s.cell(cell).properties().find("FIXED_BBOX");
  REQUIRE(value != nullptr);
  const auto* dims = std::get_if<db::DimensionList>(value);
  REQUIRE(dims != nullptr);
  return dims->values;
}

// --- Tokenization and keyword matching ----------------------------------------------

TEST_CASE("section names match case-insensitively", "[cap:def-read][req:Tokenization and keyword matching]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\ncomponents 0 ;\nend components\nEND DESIGN\n").ok());
  CHECK(s.said("  Processed 0 subcell instances total."));
  CHECK_FALSE(s.said("in DEF file; ignoring."));
  CHECK(s.said("DEF read: Processed 4 lines."));
}

TEST_CASE("NEW is case-sensitive in routes", "[cap:def-read][req:Tokenization and keyword matching]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nNETS 1 ;\n- n + ROUTED met1 ( 0 0 ) ( 100 0 ) New met9 ( 0 0 ) ( 0 100 ) ;\n"
                 "END NETS\nEND DESIGN\n")
              .ok());
  CHECK(s.said("Via name \"New\" unknown in route."));
  CHECK(s.said("Via name \"met9\" unknown in route."));
  // The leg goes on: the last points are a metal1 wire, not a new leg.
  CHECK(s.covers("top", "metal1", db::Rect{-70, -70, 70, 100}));
}

// --- Top-level DEF grammar and section dispatch -------------------------------------

TEST_CASE("skipped statements and sections give no messages",
          "[cap:def-read][req:Top-level DEF grammar and section dispatch]") {
  Session s;
  REQUIRE(s.read("VERSION 5.8 ;\nDESIGN top ;\nROW ROW_0 core 0 0 N DO 10 BY 1 STEP 460 0 ;\n"
                 "GROUPS 1 ; - g1 u1 ; END GROUPS\nTRACKS X 0 DO 10 STEP 10 LAYER met1 ;\nEND DESIGN\n")
              .ok());
  for (const auto& d : s.messages) {
    CHECK(d.code == "DEF-READ-004");
  }
  CHECK(s.cell("top").labels().empty());
}

TEST_CASE("FILLS per magic-compat mode (Q-LEFDEF-84)",
          "[cap:def-read][req:Top-level DEF grammar and section dispatch]") {
  const std::string text = "DESIGN top ;\nFILLS 1 ; - LAYER met1 RECT ( 0 0 ) ( 10 10 ) ; END FILLS\nEND DESIGN\n";
  SECTION("ON: Magic's noisy recovery") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Unknown keyword \"FILLS\" in DEF file; ignoring."));
    CHECK(s.said("Unknown keyword \"-\" in DEF file; ignoring."));
    CHECK(s.said("END statement out of context."));
    CHECK(s.said("DEF Read: encountered 1 error total."));
  }
  SECTION("OFF: one message, no error") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("FILLS section not supported; skipped."));
    CHECK_FALSE(s.said("END statement out of context."));
    CHECK_FALSE(s.said("error total."));
  }
}

TEST_CASE("END out of context and content after END DESIGN",
          "[cap:def-read][req:Top-level DEF grammar and section dispatch]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nEND NETS\nDIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\nDIEAREA ( 0 0 ) ( 20 20 ) ;\n")
              .ok());
  CHECK(s.said("END statement out of context."));
  CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 10, 10});
}

TEST_CASE("a section without END reports it", "[cap:def-read][req:Top-level DEF grammar and section dispatch]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nGROUPS 1 ; - g1 u1 ;\n").ok());
  CHECK(s.said("Section GROUPS has no END record!"));
}

// --- Unit scaling (Q-LEFDEF-79) -----------------------------------------------------

constexpr std::string_view kNanometres = "style nm\n    scalefactor 1 nanometers\n    layer M1 metal1\n";
constexpr std::string_view kTenNanometres = "style cm\n    scalefactor 1\n    layer M1 metal1\n";

TEST_CASE("nanometre grid", "[cap:def-read][req:Unit scaling]") {
  Session s{true, kNanometres};
  REQUIRE(hefesto::lef::lef_scale(s.editor.mask.output_styles.current_style()) == 0.001F);
  REQUIRE(s.read("UNITS DISTANCE MICRONS 1000 ;\nDESIGN top ;\nDIEAREA ( 0 0 ) ( 1235 1235 ) ;\nEND DESIGN\n").ok());
  CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 1235, 1235});
}

TEST_CASE("rounding half away from zero", "[cap:def-read][req:Unit scaling]") {
  Session s{true, kTenNanometres};
  REQUIRE(hefesto::lef::lef_scale(s.editor.mask.output_styles.current_style()) == 0.01F);
  REQUIRE(s.read("UNITS DISTANCE MICRONS 1000 ;\nDESIGN top ;\nDIEAREA ( -1235 -1235 ) ( 1235 1235 ) ;\nEND DESIGN\n")
              .ok());
  CHECK(die(s, "top") == std::vector<std::int32_t>{-124, -124, 124, 124});
  // LEFDEF-201's input.
  REQUIRE(s.read("UNITS DISTANCE MICRONS 1000 ;\nDESIGN t201 ;\nDIEAREA ( 1235 -1235 ) ( 2000 2000 ) ;\nEND DESIGN\n")
              .ok());
  CHECK(die(s, "t201") == std::vector<std::int32_t>{124, -124, 200, 200});
}

TEST_CASE("LEFDEF-200's die area", "[cap:def-read][req:Unit scaling]") {
  Session s{true, kNanometres};
  REQUIRE(s.read("UNITS DISTANCE MICRONS 1000 ;\nDESIGN t ;\nDIEAREA ( 0 0 ) ( 200000 150000 ) ;\nEND DESIGN\n").ok());
  CHECK(die(s, "t") == std::vector<std::int32_t>{0, 0, 200000, 150000});
}

TEST_CASE("repeated UNITS per mode", "[cap:def-read][req:Unit scaling]") {
  const std::string text =
      "UNITS DISTANCE MICRONS 1000 ;\nUNITS DISTANCE MICRONS 2000 ;\nDESIGN top ;\n"
      "DIEAREA ( 0 0 ) ( 2000 2000 ) ;\nEND DESIGN\n";
  SECTION("ON: multiplies") {
    Session s{true, kNanometres};
    REQUIRE(s.read(text).ok());
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 1, 1});
  }
  SECTION("OFF: replaces with a warning") {
    Session s{false, kNanometres};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Repeated UNITS statement; the new value replaces the previous one."));
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 1000, 1000});
  }
}

TEST_CASE("UNITS value parse per mode", "[cap:def-read][req:Unit scaling]") {
  const std::string text =
      "UNITS DISTANCE MICRONS 1000.5 ;\nDESIGN top ;\nDIEAREA ( 0 0 ) ( 1000 1000 ) ;\nEND DESIGN\n";
  SECTION("ON: the integer prefix, silently") {
    Session s{true, kNanometres};
    REQUIRE(s.read(text).ok());
    CHECK_FALSE(s.said("Invalid syntax for UNITS statement."));
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 1000, 1000});
  }
  SECTION("OFF: not an integer, default 100") {
    Session s{false, kNanometres};
    REQUIRE(s.read(text).ok());
    CHECK(s.said("Invalid syntax for UNITS statement."));
    CHECK(s.said("Assuming default value of 100"));
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 10000, 10000});
  }
  SECTION("both: no leading integer, default 100") {
    Session s{true, kNanometres};
    REQUIRE(s.read("UNITS DISTANCE MICRONS abc ;\nDESIGN top ;\nDIEAREA ( 0 0 ) ( 1 1 ) ;\nEND DESIGN\n").ok());
    CHECK(s.said("Invalid syntax for UNITS statement."));
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 10, 10});
  }
}

TEST_CASE("no UNITS statement reads microns", "[cap:def-read][req:Unit scaling]") {
  Session s{true, kNanometres};
  REQUIRE(s.read("DESIGN top ;\nDIEAREA ( 0 0 ) ( 10 20 ) ;\nEND DESIGN\n").ok());
  CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 10000, 20000});
}

// --- Design cell selection (Q-LEFDEF-78) --------------------------------------------

TEST_CASE("DESIGN names a new cell", "[cap:def-read][req:Design cell selection]") {
  Session s;
  const auto result = s.read("DESIGN picorv ;\nDIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\n");
  REQUIRE(result.ok());
  CHECK(result.value == cmd::Value{"picorv"});
  CHECK(s.cell("picorv").flags().available);
  CHECK(s.cell("picorv").flags().modified);
}

TEST_CASE("content before DESIGN per mode", "[cap:def-read][req:Design cell selection]") {
  const std::string text = "DIEAREA ( 0 0 ) ( 10 10 ) ;\nEND DESIGN\n";
  SECTION("ON: the name as typed") {
    Session s{true};
    s.options.typed_name = "build/chip.def";
    REQUIRE(s.read(text).ok());
    CHECK(s.editor.cells.find("build/chip.def") != nullptr);
  }
  SECTION("OFF: the basename without .def") {
    Session s{false};
    s.options.typed_name = "build/chip.def";
    REQUIRE(s.read(text).ok());
    CHECK(s.editor.cells.find("chip") != nullptr);
  }
  SECTION("a later DESIGN renames it") {
    Session s{false};
    s.options.typed_name = "build/chip.def";
    REQUIRE(s.read("DIEAREA ( 0 0 ) ( 10 10 ) ;\nDESIGN core ;\nEND DESIGN\n").ok());
    CHECK(s.editor.cells.find("chip") == nullptr);
    CHECK(die(s, "core") == std::vector<std::int32_t>{0, 0, 10, 10});
  }
}

TEST_CASE("re-reading adds to an existing cell", "[cap:def-read][req:Design cell selection]") {
  Session s;
  REQUIRE(s.read("DESIGN picorv ;\nBLOCKAGES 1 ;\n- LAYER met2 RECT ( 0 0 ) ( 10 10 ) ;\nEND BLOCKAGES\nEND DESIGN\n")
              .ok());
  REQUIRE(s.read("DESIGN picorv ;\nBLOCKAGES 1 ;\n- LAYER met2 RECT ( 20 0 ) ( 30 10 ) ;\nEND BLOCKAGES\nEND DESIGN\n")
              .ok());
  CHECK(s.covers("picorv", "metal2", db::Rect{0, 0, 10, 10}));
  CHECK(s.covers("picorv", "metal2", db::Rect{20, 0, 30, 10}));
}

TEST_CASE("technology name mismatch and an empty design", "[cap:def-read][req:Design cell selection]") {
  Session s;
  const auto result = s.read("TECHNOLOGY sky130 ;\nEND DESIGN\n");
  REQUIRE(result.ok());
  CHECK(s.said("DEF technology name \"sky130\" does not match current magic technology name \"deftest\""));
  CHECK(s.said("DEF read: Design has no contents."));
  CHECK(result.value == cmd::Value{std::string{}});
}

// --- Die area (Q-LEFDEF-77, 68) -----------------------------------------------------

TEST_CASE("die area property", "[cap:def-read][req:Die area]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nDIEAREA ( 0 0 ) ( 200000 150000 ) ;\nEND DESIGN\n").ok());
  CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 200000, 150000});
  CHECK(s.cell("top").flags().fixed_bbox);
}

TEST_CASE("reversed corners and points without parentheses", "[cap:def-read][req:Die area]") {
  Session s;
  REQUIRE(s.read("DESIGN top ;\nDIEAREA ( 100 100 ) ( 0 0 ) ;\nEND DESIGN\n").ok());
  CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 100, 100});
  REQUIRE(s.read("DESIGN top2 ;\nDIEAREA 5 6 1 2 ;\nEND DESIGN\n").ok());
  CHECK(die(s, "top2") == std::vector<std::int32_t>{1, 2, 5, 6});
}

TEST_CASE("polygonal die area per mode (Q-LEFDEF-77)", "[cap:def-read][req:Die area]") {
  const std::string text =
      "DESIGN top ;\nDIEAREA ( 0 0 ) ( 100 0 ) ( 100 50 ) ( 50 50 ) ( 50 100 ) ( 0 100 ) ;\nEND DESIGN\n";
  SECTION("ON: the first two points") {
    Session s{true};
    REQUIRE(s.read(text).ok());
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 100, 0});
  }
  SECTION("OFF: the bounding box") {
    Session s{false};
    REQUIRE(s.read(text).ok());
    CHECK(die(s, "top") == std::vector<std::int32_t>{0, 0, 100, 100});
  }
}

TEST_CASE("malformed die area is reported and skipped (Q-LEFDEF-68)", "[cap:def-read][req:Die area]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    REQUIRE(s.read("DESIGN top ;\nDIEAREA ( 0 0 ) ( 10 x ) ;\nDESIGN top ;\nEND DESIGN\n").ok());
    CHECK(s.said("Bad port geometry: RECT requires 4 values."));
    CHECK(s.cell("top").properties().find("FIXED_BBOX") == nullptr);
  }
}

}  // namespace
}  // namespace def_read_test
