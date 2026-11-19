// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: the file grammar and layout, the library header, SITE blocks,
// technology layers and the macro header lines (tasks.md 6.3).
//
// Owner: LEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

// The cell of lef-write "LEF file grammar"'s annotated example.
db::CellDef& annotated_inv(Session& s) {
  db::CellDef& inv = s.cell("inv");
  s.paint(inv, "bnd", db::Rect{0, 0, 100, 200});
  s.paint(inv, "metal1", db::Rect{10, 50, 30, 150});
  s.paint(inv, "metal1", db::Rect{70, 50, 90, 150});
  s.paint(inv, "metal1", db::Rect{40, 0, 60, 20});
  s.label(inv, Port{"A", "metal1", db::Rect{10, 50, 30, 150}, 0, db::PortClass::input, db::PortUse::signal});
  s.label(inv, Port{"Y", "metal1", db::Rect{70, 50, 90, 150}, 1, db::PortClass::output});
  s.property(inv, "LEFclass", std::string{"CORE"});
  s.property(inv, "LEFsymmetry", std::string{"X Y"});
  s.property(inv, "LEFsite", std::string{"unit"});
  return inv;
}

constexpr std::string_view kAnnotated =
    "VERSION 5.7 ;\n"
    "  NOWIREEXTENSIONATPIN ON ;\n"
    "  DIVIDERCHAR \"/\" ;\n"
    "  BUSBITCHARS \"[]\" ;\n"
    "MACRO inv\n"
    "  CLASS CORE ;\n"
    "  FOREIGN inv ;\n"
    "  ORIGIN 0.000 0.000 ;\n"
    "  SIZE 1.000 BY 2.000 ;\n"
    "  SYMMETRY X Y ;\n"
    "  SITE unit ;\n"
    "  PIN A\n"
    "    DIRECTION INPUT ;\n"
    "    USE SIGNAL ;\n"
    "    PORT\n"
    "      LAYER met1 ;\n"
    "        RECT 0.100 0.500 0.300 1.500 ;\n"
    "    END\n"
    "  END A\n"
    "  PIN Y\n"
    "    DIRECTION OUTPUT ;\n"
    "    PORT\n"
    "      LAYER met1 ;\n"
    "        RECT 0.700 0.500 0.900 1.500 ;\n"
    "    END\n"
    "  END Y\n"
    "  OBS\n"
    "      LAYER met1 ;\n"
    "        RECT 0.400 0.000 0.600 0.200 ;\n"
    "  END\n"
    "END inv\n"
    "END LIBRARY\n"
    "\n";

TEST_CASE("the annotated example is written byte for byte", "[cap:lef-write][req:LEF file grammar]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& inv = annotated_inv(s);
    CHECK(s.macro(inv) == kAnnotated);
  }
}

TEST_CASE("a cell that is not the edit root is written without the header",
          "[cap:lef-write][req:Single-cell file layout]") {
  Session s;
  db::CellDef& inv = annotated_inv(s);
  db::CellDef& top = s.cell("top");
  s.editor.edit_root = &top;
  REQUIRE(s.write(inv, "inv").ok());
  const std::string text = s.read("inv.lef");
  CHECK(text.starts_with("MACRO inv\n"));
  CHECK(text.ends_with("END inv\nEND LIBRARY\n\n"));
  CHECK(text.find("VERSION") == std::string::npos);
}

TEST_CASE("-tech writes the UNITS block after the fixed lines", "[cap:lef-write][req:Library header]") {
  Session s;
  db::CellDef& inv = annotated_inv(s);
  LefWriteOptions o;
  o.tech = true;
  const std::string text = s.macro(inv, o);
  CHECK(text.starts_with("VERSION 5.7 ;\n  NOWIREEXTENSIONATPIN ON ;\n  DIVIDERCHAR \"/\" ;\n  BUSBITCHARS \"[]\" ;\n"
                         "UNITS\n  DATABASE MICRONS 1000 ;\nEND UNITS\n\n"));
}

TEST_CASE("PROPERTYDEFINITIONS lists each LEFproperties key in hash order", "[cap:lef-write][req:Library header]") {
  Session s;
  db::CellDef& inv = annotated_inv(s);
  s.property(inv, "LEFproperties", std::string{"CDL \"inv x\" MASK 1"});
  const std::string text = s.macro(inv);
  const std::string defs = block(text, "PROPERTYDEFINITIONS", "END PROPERTYDEFINITIONS");
  REQUIRE_FALSE(defs.empty());
  CHECK(defs.find("  MACRO CDL STRING ;\n") != std::string::npos);
  CHECK(defs.find("  MACRO MASK STRING ;\n") != std::string::npos);
  CHECK(text.find("END PROPERTYDEFINITIONS\n\nMACRO inv\n") != std::string::npos);
}

TEST_CASE("PROPERTY lines are the LEFproperties pairs, verbatim", "[cap:lef-write][req:Macro PROPERTY lines]") {
  Session s;
  db::CellDef& inv = annotated_inv(s);
  s.property(inv, "LEFproperties", std::string{"CDL \"inv x\" MASK 1"});
  CHECK(s.macro(inv).find("  END\n  PROPERTY CDL \"inv x\" ;\n  PROPERTY MASK 1 ;\nEND inv\n") != std::string::npos);
  s.property(inv, "LEFproperties", std::string{"A 1 B"});
  const std::string dangling = s.macro(inv);
  CHECK(dangling.find("  PROPERTY A 1 ;\nEND inv\n") != std::string::npos);
  CHECK(dangling.find("PROPERTY B") == std::string::npos);
  s.property(inv, "LEFproperties", std::string{"A \"open"});
  CHECK(s.macro(inv).find("  PROPERTY ") == std::string::npos);
}

TEST_CASE("a run of spaces gives Magic's empty PROPERTYDEFINITIONS key only with magic-compat ON",
          "[cap:lef-write][req:Library header]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& inv = annotated_inv(s);
    s.property(inv, "LEFproperties", std::string{"A 1  B 2"});
    const std::string text = s.macro(inv);
    CHECK((text.find("  MACRO  STRING ;\n") != std::string::npos) == on);
    CHECK(text.find("  PROPERTY A 1 ;\n  PROPERTY B 2 ;\n") != std::string::npos);
  }
}

TEST_CASE("a SITE block is written for a LEFsite naming a cell", "[cap:lef-write][req:SITE blocks]") {
  Session s;
  REQUIRE(s.run("cif ostyle nm5").ok());
  db::CellDef& unit = s.cell("unit");
  s.paint(unit, "bnd", db::Rect{0, 0, 46, 272});
  db::CellDef& inv = annotated_inv(s);
  const std::string text = s.macro(inv);
  // nm5: oscale 0.005, U = 200 (grid limit 5).
  CHECK(block(text, "SITE unit", "END unit") ==
        "SITE unit\n  SYMMETRY Y ;\n  CLASS CORE ;\n  SIZE 0.230 BY 1.360 ;\nEND unit\n");
  CHECK(text.find("END unit\n\nMACRO inv\n") != std::string::npos);
}

TEST_CASE("a fixed-bbox SITE takes the written cell's FIXED_BBOX only with magic-compat ON",
          "[cap:lef-write][req:SITE blocks]") {
  for (const bool on : {true, false}) {
    Session s(on);
    REQUIRE(s.run("cif ostyle nm5").ok());
    db::CellDef& unit = s.cell("unit");
    s.paint(unit, "bnd", db::Rect{0, 0, 46, 272});
    unit.flags().fixed_bbox = true;
    s.property(unit, "FIXED_BBOX", db::DimensionList{{0, 0, 46, 272}});
    db::CellDef& inv = annotated_inv(s);
    s.property(inv, "FIXED_BBOX", db::DimensionList{{0, 0, 460, 272}});
    const std::string text = s.macro(inv);
    CHECK(text.find(on ? "  SIZE 2.300 BY 1.360 ;\nEND unit\n" : "  SIZE 0.230 BY 1.360 ;\nEND unit\n") !=
          std::string::npos);
  }
}

TEST_CASE("a LEFsite with no cell of that name writes no SITE block", "[cap:lef-write][req:SITE blocks]") {
  Session s;
  db::CellDef& inv = annotated_inv(s);
  CHECK(s.macro(inv).find("SITE unit\n") == std::string::npos);
}

}  // namespace
}  // namespace hefesto::lef::write_test
