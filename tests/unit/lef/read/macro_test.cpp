// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MACRO and SITE sections in creation mode (capability lef-read "MACRO and
// SITE cell creation", "Macro coordinate offset", "Macro statements and
// cell properties", "Macro completion", "PIN attributes", "PORT and OBS
// geometry", "Port labels in creation mode"; openspec/changes/
// m6-lef-def-oasis tasks.md 5.4), each compat decision in both modes.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "hefesto/lef/via/polygon.hpp"
#include "macro_fixture.hpp"

using namespace lefread_test;

namespace {

bool has_text(const std::vector<std::string>& texts, std::string_view text) {
  return std::find(texts.begin(), texts.end(), text) != texts.end();
}

const db::Label& only_label(const db::CellDef& cell) {
  REQUIRE(cell.labels().size() == 1);
  return cell.labels().front();
}

}  // namespace

// --- MACRO and SITE cell creation ------------------------------------------------------

TEST_CASE("Duplicate macro in one file", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  MacroRun run;
  run.read("MACRO INV\n  SIZE 1 BY 1 ;\nEND INV\nMACRO INV\n  SIZE 2 BY 2 ;\nEND INV\n");
  CHECK(run.texts() == Strings{"LEF read, Line 4 (Warning): Cell \"INV\" was already defined in this file.  "
                               "Renaming this cell \"INV_1\""});
  CHECK(fixed_bbox(run.cell("INV")) == db::Rect{0, 0, 1, 1});
  CHECK(fixed_bbox(run.cell("INV_1")) == db::Rect{0, 0, 2, 2});
  CHECK(run.host.created == Strings{"INV", "INV_1"});
}

TEST_CASE("Re-reading the same LEF file", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  MacroRun run;
  run.read("MACRO INV\n  SIZE 4 BY 4 ;\n  OBS\n    LAYER metal1 ;\n    RECT 0 0 1 1 ;\n  END\nEND INV\n");
  db::CellDef& inv = run.cell("INV");
  CHECK(tiles(inv, "metal1") == std::vector<db::Rect>{{0, 0, 1, 1}});
  db::set_property(inv, "LEFview", std::nullopt);
  run.texts();
  // The second read annotates the cell: OBS is skipped and LEFview not set.
  run.read("MACRO INV\n  SIZE 4 BY 4 ;\n  OBS\n    LAYER metal1 ;\n    RECT 2 2 3 3 ;\n  END\nEND INV\n");
  CHECK(run.texts().empty());
  CHECK(run.host.annotated == Strings{"INV"});
  CHECK(run.host.created == Strings{"INV"});
  CHECK(tiles(inv, "metal1") == std::vector<db::Rect>{{0, 0, 1, 1}});
  CHECK_FALSE(string_property(inv, "LEFview").has_value());
  CHECK(fixed_bbox(inv) == db::Rect{0, 0, 4, 4});
}

TEST_CASE("the macro table is cleared after each read", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  // A second read of a file with a duplicate renames only within that read.
  MacroRun run;
  run.read("MACRO A\n  SIZE 1 BY 1 ;\nEND A\n");
  run.read("MACRO B\n  SIZE 1 BY 1 ;\nEND B\nMACRO B\n  SIZE 1 BY 1 ;\nEND B\n");
  CHECK(run.host.created == Strings{"A", "B", "B_1"});
}

TEST_CASE("lef read -annotate with an unknown macro", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  const std::string text = "MACRO NAND2\n  SIZE 1 BY 1 ;\nEND NAND2\n";
  SECTION("magic-compat ON: the cell is created") {
    MacroRun run{true};
    run.macros.annotate = true;
    run.read(text);
    CHECK(run.host.created == Strings{"NAND2"});
    CHECK(string_property(run.cell("NAND2"), "LEFview") == "TRUE");
  }
  SECTION("magic-compat OFF: the macro is skipped") {
    MacroRun run{false};
    run.macros.annotate = true;
    run.read(text + "MACRO B\nEND B\n");
    CHECK(run.texts().empty());
    CHECK(run.host.created.empty());
    CHECK(run.host.cells.find("NAND2") == nullptr);
  }
}

TEST_CASE("lef read -annotate skips a duplicate with no cell as Magic does", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  // Magic skips the duplicate to `END MACRO` (LefSkipSection(f, "MACRO")):
  // the rest of the file is not read.
  MacroRun run{true};
  run.macros.annotate = true;
  run.read("MACRO A\n  SIZE 1 BY 1 ;\nEND A\nMACRO A\nEND A\nMACRO B\n  SIZE 1 BY 1 ;\nEND B\n");
  const auto texts = run.texts();
  CHECK(has_text(texts, "LEF read, Line 4 (Warning): Cell \"A\" was already defined in this file.  "
                        "Renaming this cell \"A_1\""));
  CHECK(has_text(texts, "LEF read, Line 8 (Error): Section MACRO has no END record!"));
  CHECK(run.host.created == Strings{"A"});
}

TEST_CASE("a fixed LEF datestamp is given to every macro", "[cap:lef-read][req:MACRO and SITE cell creation]") {
  MacroRun run;
  run.existing("OLD");
  run.macros.datestamp = 1600000000;
  run.read("MACRO INV\n  SIZE 1 BY 1 ;\nEND INV\nMACRO OLD\nEND OLD\n");
  for (const char* name : {"INV", "OLD"}) {
    const db::CellDef& cell = run.cell(name);
    CHECK(cell.timestamp() == 1600000000);
    CHECK(cell.flags().fixed_timestamp);
  }
}

// --- Macro coordinate offset ---------------------------------------------------------

TEST_CASE("ORIGIN shifts geometry but not the box (verify against Magic)",
          "[cap:lef-read][req:Macro coordinate offset]") {
  const std::string text =
      "MACRO A\n  ORIGIN 1 1 ;\n  SIZE 10 BY 10 ;\n  OBS\n    LAYER metal1 ;\n    RECT 0 0 2 2 ;\n  END\nEND A\n";
  SECTION("magic-compat ON: geometry at g - ORIGIN, box (0, 0, SIZE)") {
    MacroRun run{true, 0.01F};
    run.read(text);
    CHECK(tiles(run.cell("A"), "metal1") == std::vector<db::Rect>{{-100, -100, 100, 100}});
    CHECK(fixed_bbox(run.cell("A")) == db::Rect{0, 0, 1000, 1000});
  }
  SECTION("magic-compat OFF: LEF's frame") {
    MacroRun run{false, 0.01F};
    run.read(text);
    CHECK(tiles(run.cell("A"), "metal1") == std::vector<db::Rect>{{0, 0, 200, 200}});
    CHECK(fixed_bbox(run.cell("A")) == db::Rect{-100, -100, 900, 900});
  }
}

TEST_CASE("FOREIGN offset", "[cap:lef-read][req:Macro coordinate offset]") {
  const std::string text =
      "MACRO B\n  FOREIGN B 0.5 0.5 ;\n  SIZE 2 BY 2 ;\n  OBS\n    LAYER metal1 ;\n    RECT 0 0 1 1 ;\n  END\nEND B\n";
  SECTION("magic-compat ON") {
    MacroRun run{true, 0.01F};
    run.read(text);
    CHECK(tiles(run.cell("B"), "metal1") == std::vector<db::Rect>{{-50, -50, 50, 50}});
    CHECK(fixed_bbox(run.cell("B")) == db::Rect{50, 50, 250, 250});
  }
  SECTION("magic-compat OFF: the box moves with the geometry") {
    MacroRun run{false, 0.01F};
    run.read(text);
    CHECK(tiles(run.cell("B"), "metal1") == std::vector<db::Rect>{{-50, -50, 50, 50}});
    CHECK(fixed_bbox(run.cell("B")) == db::Rect{-50, -50, 150, 150});
  }
}

TEST_CASE("ORIGIN and FOREIGN apply to the geometry that follows them", "[cap:lef-read][req:Macro coordinate offset]") {
  MacroRun run{true};
  run.read(
      "MACRO A\n  OBS\n    LAYER metal1 ;\n    RECT 0 0 1 1 ;\n  END\n  FOREIGN A 10 0 ;\n"
      "  OBS\n    LAYER metal2 ;\n    RECT 0 0 1 1 ;\n  END\n  SIZE 1 BY 1 ;\nEND A\n");
  CHECK(tiles(run.cell("A"), "metal1") == std::vector<db::Rect>{{0, 0, 1, 1}});
  CHECK(tiles(run.cell("A"), "metal2") == std::vector<db::Rect>{{-10, 0, -9, 1}});
}

TEST_CASE("malformed SIZE, ORIGIN and FOREIGN", "[cap:lef-read][req:Macro coordinate offset]") {
  MacroRun run;
  run.read("MACRO A\n  SIZE x BY 1 ;\n  ORIGIN 1 ;\n  FOREIGN A 1 ;\nEND A\n");
  const auto texts = run.texts();
  CHECK(has_text(texts, "LEF read, Line 2 (Error): Bad macro SIZE; requires values X BY Y."));
  CHECK(has_text(texts, "LEF read, Line 3 (Error): Bad macro ORIGIN; requires 2 values."));
}

// --- Macro statements and cell properties ------------------------------------------

TEST_CASE("Class and properties", "[cap:lef-read][req:Macro statements and cell properties]") {
  MacroRun run;
  run.read(
      "MACRO A\n  CLASS CORE WELLTAP ;\n  SYMMETRY X Y R90 ;\n  SITE unithd ;\n"
      "  PROPERTY CatenaDesignType deviceLevel ;\n  PROPERTY k2 \"v 2\" ;\n  SOURCE USER ;\n  SIZE 1 BY 1 ;\nEND A\n");
  CHECK(run.texts().empty());
  const db::CellDef& a = run.cell("A");
  CHECK(string_property(a, "LEFclass") == "CORE WELLTAP");
  CHECK(string_property(a, "LEFsymmetry") == "X Y R90");
  CHECK(string_property(a, "LEFsite") == "unithd");
  CHECK(string_property(a, "LEFproperties") == "CatenaDesignType deviceLevel k2 \"v 2\"");
}

TEST_CASE("each CLASS, SYMMETRY and SITE replaces the last", "[cap:lef-read][req:Macro statements and cell properties]") {
  MacroRun run;
  run.read("MACRO A\n  CLASS CORE ;\n  CLASS BLOCK ;\n  SITE s1 ;\n  SITE s2 x ;\n  SIZE 1 BY 1 ;\nEND A\n");
  CHECK(string_property(run.cell("A"), "LEFclass") == "BLOCK");
  CHECK(string_property(run.cell("A"), "LEFsite") == "s2");
}

TEST_CASE("an empty CLASS or SYMMETRY sets no property", "[cap:lef-read][req:Macro statements and cell properties]") {
  for (const bool on : {true, false}) {
    MacroRun run{on};
    run.read("MACRO A\n  CLASS ;\n  SYMMETRY ;\n  SIZE 1 BY 1 ;\nEND A\n");
    CHECK(run.texts().empty());
    CHECK_FALSE(string_property(run.cell("A"), "LEFclass").has_value());
    CHECK_FALSE(string_property(run.cell("A"), "LEFsymmetry").has_value());
  }
}

TEST_CASE("a SITE or PROPERTY with no value", "[cap:lef-read][req:Macro statements and cell properties]") {
  const std::string text = "MACRO A\n  SITE ;\n  SIZE 2 BY 2 ;\n  PROPERTY k ;\n  CLASS CORE ;\nEND A\n";
  SECTION("magic-compat ON: the `;` is the value and the next statement is swallowed") {
    MacroRun run{true};
    run.read(text);
    CHECK(string_property(run.cell("A"), "LEFsite") == ";");
    CHECK(string_property(run.cell("A"), "LEFproperties") == "k ;");
    CHECK_FALSE(fixed_bbox(run.cell("A")) == db::Rect{0, 0, 2, 2});
    CHECK_FALSE(string_property(run.cell("A"), "LEFclass").has_value());
  }
  SECTION("magic-compat OFF: nothing is set and the next statement is read") {
    MacroRun run{false};
    run.read(text);
    CHECK_FALSE(string_property(run.cell("A"), "LEFsite").has_value());
    CHECK_FALSE(string_property(run.cell("A"), "LEFproperties").has_value());
    CHECK(fixed_bbox(run.cell("A")) == db::Rect{0, 0, 2, 2});
    CHECK(string_property(run.cell("A"), "LEFclass") == "CORE");
  }
}

TEST_CASE("TIMING is skipped and unknown keywords are reported", "[cap:lef-read][req:Macro statements and cell properties]") {
  MacroRun run;
  run.read("MACRO A\n  TIMING\n    x ;\n  END TIMING\n  EEQ B ;\n  SIZE 1 BY 1 ;\nEND A\n");
  CHECK(run.texts() == Strings{"LEF read, Line 5 (Message): Unknown keyword \"EEQ\" in LEF file; ignoring."});
  CHECK(fixed_bbox(run.cell("A")) == db::Rect{0, 0, 1, 1});
}

// --- Macro completion ----------------------------------------------------------------

TEST_CASE("Macro without SIZE", "[cap:lef-read][req:Macro completion]") {
  MacroRun run{true, 0.1F};
  run.read("MACRO A\n  OBS\n    LAYER metal1 ;\n    RECT 0 0 1 2 ;\n  END\nEND A\n");
  CHECK(run.texts() ==
        Strings{"LEF read, Line 6 (Warning):    Macro does not define size:  computing from geometry"});
  const db::CellDef& a = run.cell("A");
  CHECK(fixed_bbox(a) == db::Rect{0, 0, 10, 20});
  CHECK(a.flags().fixed_bbox);
  CHECK(string_property(a, "LEFview") == "TRUE");
  CHECK(run.host.drc == std::vector<std::optional<db::Rect>>{db::Rect{0, 0, 10, 20}});
}

TEST_CASE("SITE becomes a cell", "[cap:lef-read][req:Macro completion]") {
  MacroRun run{true, 0.005F};
  run.read("SITE unithd CLASS CORE ; SYMMETRY Y ; SIZE 0.46 BY 2.72 ; END unithd\n");
  CHECK(run.texts().empty());
  const db::CellDef& site = run.cell("unithd");
  for (std::size_t p = 0; p < site.plane_count(); ++p) {
    CHECK(site.plane(static_cast<tech::PlaneId>(p)).empty());
  }
  CHECK(string_property(site, "LEFclass") == "CORE");
  CHECK(string_property(site, "LEFsymmetry") == "Y");
  CHECK(fixed_bbox(site) == db::Rect{0, 0, 92, 544});
  CHECK(string_property(site, "LEFview") == "TRUE");
  CHECK(run.host.done == Strings{"unithd"});
  CHECK(run.host.drc == std::vector<std::optional<db::Rect>>{std::nullopt});
}

TEST_CASE("a mismatched macro END is reported and reading goes on", "[cap:lef-read][req:Macro completion]") {
  MacroRun run;
  run.read("MACRO A\n  SIZE 1 BY 1 ;\nEND B\nEND A\nMACRO C\n  SIZE 1 BY 1 ;\nEND C\n");
  CHECK(run.texts() == Strings{"LEF read, Line 3 (Error): Macro END statement missing."});
  CHECK(run.host.created == Strings{"A", "C"});
}

// --- PIN attributes ------------------------------------------------------------------

TEST_CASE("DIRECTION after PORT does not apply", "[cap:lef-read][req:PIN attributes]") {
  MacroRun run;
  run.read("MACRO M\n  PIN A\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n"
           "    DIRECTION INPUT ;\n  END A\n  SIZE 1 BY 1 ;\nEND M\n");
  CHECK(run.texts().empty());
  CHECK(only_label(run.cell("M")).port_class == db::PortClass::default_class);
}

TEST_CASE("OUTPUT TRISTATE", "[cap:lef-read][req:PIN attributes]") {
  const std::string text = "MACRO M\n  PIN Z\n    DIRECTION OUTPUT TRISTATE ;\n    PORT\n      LAYER metal1 ;\n"
                           "      RECT 0 0 1 1 ;\n    END\n  END Z\n  SIZE 1 BY 1 ;\nEND M\n";
  SECTION("magic-compat ON: output") {
    MacroRun run{true};
    run.read(text);
    CHECK(only_label(run.cell("M")).port_class == db::PortClass::output);
  }
  SECTION("magic-compat OFF: tristate") {
    MacroRun run{false};
    run.read(text);
    CHECK(only_label(run.cell("M")).port_class == db::PortClass::tristate);
  }
}

TEST_CASE("DEF-style USE value", "[cap:lef-read][req:PIN attributes]") {
  const std::string text = "MACRO M\n  PIN T\n    USE TIEOFF ;\n    PORT\n      LAYER metal1 ;\n"
                           "      RECT 0 0 1 1 ;\n    END\n  END T\n  SIZE 1 BY 1 ;\nEND M\n";
  SECTION("magic-compat ON: improper") {
    MacroRun run{true};
    run.read(text);
    CHECK(run.texts() == Strings{"LEF read, Line 3 (Error): Improper USE statement"});
    CHECK(only_label(run.cell("M")).port_use == db::PortUse::default_use);
  }
  SECTION("magic-compat OFF: tie-off, scan and reset") {
    MacroRun run{false};
    run.read(text);
    CHECK(run.texts().empty());
    CHECK(only_label(run.cell("M")).port_use == db::PortUse::tieoff);
  }
}

TEST_CASE("DIRECTION, USE and SHAPE values", "[cap:lef-read][req:PIN attributes]") {
  MacroRun run;
  run.read("MACRO M\n  PIN P\n    DIRECTION inout ;\n    USE ground ;\n    SHAPE ABUTMENT ;\n"
           "    DIRECTION sideways ;\n    SHAPE square ;\n    ANTENNAGATEAREA 0.1 ;\n    NETEXPR \"x\" ;\n"
           "    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END Q\n  END P\n  SIZE 1 BY 1 ;\nEND M\n");
  CHECK(run.texts() == Strings{"LEF read, Line 6 (Error): Improper DIRECTION statement",
                               "LEF read, Line 7 (Error): Improper SHAPE statement",
                               "LEF read, Line 14 (Error): Pin END statement missing."});
  const db::Label& p = only_label(run.cell("M"));
  CHECK(p.port_class == db::PortClass::bidirectional);
  CHECK(p.port_use == db::PortUse::ground);
  CHECK(p.port_shape == db::PortShape::abutment);
}

// --- PORT and OBS geometry ------------------------------------------------------------

TEST_CASE("Contact sizing of a cut RECT", "[cap:lef-read][req:PORT and OBS geometry]") {
  MacroRun run;
  run.lef.layers.define({"pc"}, lef::LayerEntry{lef::LayerClass::cut, type_of("m2c"), 0, 0, db::Rect{-4, -4, 4, 4}});
  run.read("MACRO M\n  PIN A\n    PORT\n      LAYER pc ;\n      RECT 10 10 13 13 ;\n    END\n  END A\n"
           "  SIZE 1 BY 1 ;\nEND M\n");
  CHECK(tiles(run.cell("M"), "m2c") == std::vector<db::Rect>{{9, 9, 13, 13}});
  CHECK(only_label(run.cell("M")).rect == db::Rect{9, 9, 13, 13});
}

TEST_CASE("Ignored PATH", "[cap:lef-read][req:PORT and OBS geometry]") {
  const std::string text =
      "MACRO M\n  OBS\n    LAYER metal1 ;\n    WIDTH 2 ;\n    PATH 0 0 5 0 ;\n  END\n  SIZE 1 BY 1 ;\nEND M\n";
  SECTION("magic-compat ON: nothing painted") {
    MacroRun run{true};
    run.read(text);
    CHECK(run.texts().empty());
    CHECK(tiles(run.cell("M"), "metal1").empty());
  }
  SECTION("magic-compat OFF: the path is painted") {
    MacroRun run{false};
    run.read(text);
    CHECK(run.texts().empty());
    CHECK(tiles(run.cell("M"), "metal1") == std::vector<db::Rect>{{-1, -1, 6, 1}});
  }
}

TEST_CASE("PORT PATH and VIA per mode", "[cap:lef-read][req:PORT and OBS geometry]") {
  const std::string text =
      "VIA V1\n  LAYER metal1 ;\n  RECT -1 -1 1 1 ;\nEND V1\n"
      "MACRO M\n  PIN A\n    PORT\n      LAYER metal2 ;\n      WIDTH 2 ;\n      PATH 0 10 0 15 ;\n"
      "      VIA 5 5 V1 ;\n    END\n  END A\n  SIZE 1 BY 1 ;\nEND M\n";
  SECTION("magic-compat ON: no paint and no label") {
    MacroRun run{true};
    run.read(text);
    CHECK(tiles(run.cell("M"), "metal1").empty());
    CHECK(tiles(run.cell("M"), "metal2").empty());
    CHECK(run.cell("M").labels().empty());
  }
  SECTION("magic-compat OFF: painted, with port labels") {
    MacroRun run{false};
    run.read(text);
    CHECK(run.texts().empty());
    CHECK(tiles(run.cell("M"), "metal2") == std::vector<db::Rect>{{-1, 9, 1, 16}});
    CHECK(tiles(run.cell("M"), "metal1") == std::vector<db::Rect>{{4, 4, 6, 6}});
    const auto& labels = run.cell("M").labels();
    REQUIRE(labels.size() == 2);
    CHECK(labels[0].rect == db::Rect{4, 4, 6, 6});
    CHECK(labels[1].rect == db::Rect{-1, 9, 1, 16});
  }
  SECTION("magic-compat OFF: an unknown via is reported") {
    MacroRun run{false};
    run.read("MACRO M\n  OBS\n    LAYER metal1 ;\n    VIA 0 0 NOPE ;\n  END\n  SIZE 1 BY 1 ;\nEND M\n");
    CHECK(run.texts() == Strings{"LEF read, Line 4 (Message): Unknown via \"NOPE\" in LEF geometry; ignoring."});
  }
}

TEST_CASE("OBS paints the secondary obstruction type", "[cap:lef-read][req:PORT and OBS geometry]") {
  MacroRun run;
  run.read("MACRO M\n  OBS\n    LAYER via1 ;\n    RECT 0 0 4 4 ;\n  END\n  SIZE 1 BY 1 ;\nEND M\n");
  CHECK(tiles(run.cell("M"), "metal1") == std::vector<db::Rect>{{0, 0, 4, 4}});
  CHECK(tiles(run.cell("M"), "obsv1") == std::vector<db::Rect>{{0, 0, 4, 4}});
}

TEST_CASE("RECT with no layer, malformed RECT and POLYGON errors", "[cap:lef-read][req:PORT and OBS geometry]") {
  MacroRun run;
  run.read("MACRO M\n  OBS\n    RECT 0 0 1 1 ;\n    LAYER metal1 ;\n    RECT 0 0 1 x ;\n    RECT MASK 1 0 0 1 1 ;\n"
           "    POLYGON 0 0 x 1 ;\n    POLYGON 0 0 1 ;\n  END\n  SIZE 1 BY 1 ;\nEND M\n");
  CHECK(run.texts() == Strings{"LEF read, Line 5 (Error): Bad port geometry: RECT requires 4 values.",
                               "LEF read, Line 6 (Error): Bad port geometry: RECT requires 4 values.",
                               "LEF read, Line 7 (Error): Bad X value in polygon.",
                               "LEF read, Line 8 (Error): Missing Y value in polygon point!"});
  CHECK(tiles(run.cell("M"), "metal1").empty());
}

TEST_CASE("POLYGON on an unknown layer is skipped", "[cap:lef-read][req:PORT and OBS geometry]") {
  MacroRun run;
  run.read("MACRO M\n  OBS\n    LAYER bogus ;\n    POLYGON 0 0 4 0 4 4 0 4 ;\n    LAYER metal1 ;\n"
           "    RECT 0 0 1 1 ;\n  END\n  SIZE 1 BY 1 ;\nEND M\n");
  const auto texts = run.texts();
  REQUIRE(texts.size() == 2);
  CHECK(texts[0] == "LEF read, Line 3 (Error): Don't know how to parse layer \"bogus\"");
  CHECK(tiles(run.cell("M"), "metal1") == std::vector<db::Rect>{{0, 0, 1, 1}});
}

TEST_CASE("a POLYGON is painted and its rectangles follow the RECTs", "[cap:lef-read][req:PORT and OBS geometry]") {
  MacroRun run;
  run.read("MACRO M\n  PIN A\n    PORT\n      LAYER metal1 ;\n      POLYGON 0 0 4 0 4 2 2 2 2 4 0 4 ;\n"
           "      RECT 10 10 11 11 ;\n    END\n  END A\n  SIZE 1 BY 1 ;\nEND M\n");
  const auto rects = lef::polygon_rects(std::vector<db::Point>{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}});
  const auto& labels = run.cell("M").labels();
  REQUIRE(labels.size() == 1 + rects.size());
  CHECK(labels[0].rect == db::Rect{10, 10, 11, 11});
  for (std::size_t i = 0; i < rects.size(); ++i) {
    CHECK(labels[i + 1].rect == rects[i]);
  }
}

TEST_CASE("Ignored layer in a macro", "[cap:lef-tech-section][req:Use of entry classes by readers and writers]") {
  for (const bool on : {true, false}) {
    MacroRun run{on};
    run.read("MACRO M\n  OBS\n    LAYER pwell ;\n    RECT 0 0 1 1 ;\n  END\n  SIZE 1 BY 1 ;\nEND M\n");
    CHECK(run.texts().empty());
    const db::CellDef& m = run.cell("M");
    for (std::size_t p = 0; p < m.plane_count(); ++p) {
      CHECK(m.plane(static_cast<tech::PlaneId>(p)).empty());
    }
  }
}

// --- Port labels in creation mode ----------------------------------------------------

TEST_CASE("Label order and numbering", "[cap:lef-read][req:Port labels in creation mode]") {
  MacroRun run;
  run.read("MACRO M\n  PIN VDD\n    USE POWER ;\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n"
           "      RECT 0 5 1 6 ;\n    END\n  END VDD\n  PIN A\n    PORT\n      LAYER metal1 ;\n"
           "      RECT 3 3 4 4 ;\n    END\n  END A\n  SIZE 1 BY 1 ;\nEND M\n");
  const auto& labels = run.cell("M").labels();
  REQUIRE(labels.size() == 3);
  CHECK(labels[0].rect == db::Rect{0, 5, 1, 6});
  CHECK(labels[1].rect == db::Rect{0, 0, 1, 1});
  for (std::size_t i = 0; i < 2; ++i) {
    CHECK(labels[i].text == "VDD");
    CHECK(labels[i].port_index == 1);
    CHECK(labels[i].port_use == db::PortUse::power);
    CHECK(labels[i].port_class == db::PortClass::default_class);
    CHECK(labels[i].port_directions == (db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest));
    CHECK(labels[i].sticky);
    CHECK(labels[i].type == type_of("metal1"));
  }
  // Second pin numbering.
  CHECK(labels[2].text == "A");
  CHECK(labels[2].port_index == 2);
}

TEST_CASE("a pin with no labels still takes an ordinal", "[cap:lef-read][req:Port labels in creation mode]") {
  MacroRun run;
  run.read("MACRO M\n  PIN NC\n  END NC\n  PIN A\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n"
           "    END\n  END A\n  PIN A\n    PORT\n      LAYER metal1 ;\n      RECT 2 2 3 3 ;\n    END\n  END A\n"
           "  SIZE 1 BY 1 ;\nEND M\n");
  const auto& labels = run.cell("M").labels();
  REQUIRE(labels.size() == 2);
  CHECK(labels[0].port_index == 2);
  CHECK(labels[1].port_index == 2);  // an existing port label of the same name gives its index
}
