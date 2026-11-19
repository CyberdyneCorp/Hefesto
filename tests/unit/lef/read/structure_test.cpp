// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unit tests of the LEF file structure and the LAYER section (capability
// lef-read; openspec/changes/m6-lef-def-oasis tasks.md 5.2, 5.3).

#include <catch2/catch_test_macros.hpp>

#include "read_fixture.hpp"

using namespace lefread_test;

TEST_CASE("content after END LIBRARY", "[cap:lef-read][req:Supported LEF file structure]") {
  LefRun run;
  define_scenario_layers(run.layers);
  const auto result = run.read(
      "VERSION 5.7 ;\nNONDEFAULTRULE A\nEND A\nEND LIBRARY\nNONDEFAULTRULE B\nEND B\n");
  CHECK(result.end_library);
  CHECK(result.lines == 4);
  CHECK(run.rules.find("A") != nullptr);
  CHECK(run.rules.find("B") == nullptr);
  CHECK(run.texts().empty());
}

TEST_CASE("UNITS ignored", "[cap:lef-read][req:Supported LEF file structure]") {
  LefRun run{true, 0.01F};
  define_scenario_layers(run.layers);
  run.read("UNITS DATABASE MICRONS 2000 ; END UNITS\nLAYER metal1 WIDTH 1 ; END metal1\n");
  CHECK(run.layers.at("metal1").route.width == 100);
  CHECK(run.texts().empty());
}

TEST_CASE("ignored statements and skipped sections", "[cap:lef-read][req:Supported LEF file structure]") {
  LefRun run;
  define_scenario_layers(run.layers);
  const auto result = run.read(
      "VERSION 5.7 ;\nBUSBITCHARS \"[]\" ;\nDIVIDERCHAR \"/\" ;\nnamescasesensitive ON ;\n"
      "MANUFACTURINGGRID 0.005 ;\nUSEMINSPACING OBS OFF ;\nCLEARANCEMEASURE EUCLIDEAN ;\n"
      "NOWIREEXTENSIONATPIN ON ;\n"
      "PROPERTYDEFINITIONS\n  LAYER LEF57_SPACING STRING ;\nEND PROPERTYDEFINITIONS\n"
      "SPACING SAMENET metal1 metal1 0.1 ; END SPACING\n"
      "NOISETABLE 1 ; END NOISETABLE\nCORRECTIONTABLE 1 ; END CORRECTIONTABLE\n"
      "IRDROP TABLE x ; END IRDROP\nARRAY a SITE s ; END a END ARRAY\nTIMING x ; END TIMING\n"
      "BEGINEXT \"tag\"\n  CREATOR \"x\" ;\nENDEXT\n"
      "LAYER metal1 WIDTH 0.5 ; END metal1\n");
  CHECK_FALSE(result.end_library);
  CHECK(result.lines == 21);
  CHECK(run.layers.at("metal1").route.width == 50);
  CHECK(run.texts().empty());
}

TEST_CASE("unknown top-level keywords are skipped with a message", "[cap:lef-read][req:Supported LEF file structure]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("FIXEDMASK ;\nLAYER metal1 WIDTH 0.5 ; END metal1\n");
  CHECK(run.texts() == Strings{"LEF read, Line 1 (Message): Unknown keyword \"FIXEDMASK\" in LEF file; ignoring."});
  CHECK(run.layers.at("metal1").route.width == 50);
}

TEST_CASE("END out of context", "[cap:lef-read][req:Supported LEF file structure]") {
  LefRun run;
  define_scenario_layers(run.layers);
  const auto result = run.read("END FOO\nLAYER metal1 WIDTH 0.5 ; END metal1\nEND END\nLAYER metal2 WIDTH 1 ; END metal2\n");
  CHECK(run.texts() == Strings{"LEF read, Line 1 (Error): END statement out of context."});
  CHECK(run.layers.at("metal1").route.width == 50);
  CHECK(result.end_library);  // END END stops the read, as in Magic
  CHECK_FALSE(run.layers.at("metal2").route.width);
}

TEST_CASE("each section is read to its END and reading continues after it",
          "[cap:lef-read][req:Supported LEF file structure]") {
  // Without a macro host (lef::StreamTarget::macros), MACRO and SITE are
  // skipped to their END; VIA and VIARULE define vias.
  LefRun run;
  define_scenario_layers(run.layers);
  run.read(
      "SITE core CLASS CORE ; SIZE 0.46 BY 2.72 ; END core\n"
      "VIA v12 DEFAULT LAYER metal1 ; RECT -0.1 -0.1 0.1 0.1 ; END v12\n"
      "VIARULE gen GENERATE LAYER metal1 ; END gen\n"
      "MACRO INV CLASS CORE ; PIN A PORT LAYER metal1 ; RECT 0 0 1 1 ; END END A END INV\n"
      "LAYER metal1 WIDTH 0.5 ; END metal1\n");
  CHECK(run.texts().empty());
  CHECK(run.vias.size() == 2);
  CHECK(run.layers.at("metal1").route.width == 50);
}

TEST_CASE("a section keyword at the end of the input ends the read", "[cap:lef-read][req:Supported LEF file structure]") {
  for (const char* text : {"LAYER", "NONDEFAULTRULE", "MACRO", "VIA"}) {
    LefRun run;
    const auto result = run.read(text);
    CHECK_FALSE(result.end_library);
    CHECK(run.texts().empty());
  }
}

TEST_CASE("LEFDEF-006: more than 100 errors", "[cap:lef-read][req:LEF/DEF message reporting]") {
  // 75 unknown layers give 150 errors: 100 printed, one notice, then the
  // totals at the end of the read.
  std::string text = "LAYER metal1\n";
  for (int i = 0; i < 75; ++i) {
    text += "  LAYER poly2 ;\n";
  }
  text += "END metal1\n";
  LefRun run;
  define_scenario_layers(run.layers);
  run.read(text);
  run.counter.summarize(run.sink, lef::Dialect::lef);
  const auto texts = run.texts();
  REQUIRE(texts.size() == 102);
  CHECK(texts[0] == "LEF read, Line 2 (Error): Don't know how to parse layer \"poly2\"");
  CHECK(texts[99] == "LEF read, Line 51 (Error): Try adding this name to the LEF techfile section");
  CHECK(texts[100] == "LEF Read:  Further errors will not be reported.");
  CHECK(texts[101] == "LEF Read: encountered 150 errors total.");
}

TEST_CASE("LEF width overrides tech default", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run{true, 0.01F};
  define_scenario_layers(run.layers);
  run.read("LAYER metal1 TYPE ROUTING ; WIDTH 0.14 ; PITCH 0.34 ; DIRECTION HORIZONTAL ; END metal1\n");
  const auto& route = run.layers.at("metal1").route;
  CHECK(route.width == 14);
  CHECK(route.pitch == 34);
  CHECK(route.direction == lef::RouteDirection::horizontal);
  CHECK(&run.layers.at("m1") == &run.layers.at("metal1"));  // the alias shares the record
  CHECK(run.texts().empty());
}

TEST_CASE("direction other than h is vertical", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("LAYER metal2 DIRECTION diagonal ; END metal2\nLAYER metal1 DIRECTION horizontal ; END metal1\n");
  CHECK(run.layers.at("metal2").route.direction == lef::RouteDirection::vertical);
  CHECK(run.layers.at("metal1").route.direction == lef::RouteDirection::horizontal);
}

TEST_CASE("last SPACING wins", "[cap:lef-read][req:LEF LAYER section]") {
  constexpr std::string_view text = "LAYER metal1 SPACING 0.14 ; SPACING 0.28 RANGE 3 100 ; END metal1\n";
  SECTION("magic-compat ON") {
    LefRun run{true, 0.01F};
    define_scenario_layers(run.layers);
    run.read(text);
    CHECK(run.layers.at("metal1").route.spacing == 28);
  }
  SECTION("magic-compat OFF") {
    LefRun run{false, 0.01F};
    define_scenario_layers(run.layers);
    run.read(text);
    CHECK(run.layers.at("metal1").route.spacing == 14);
  }
}

TEST_CASE("without an unqualified SPACING the smallest one is kept", "[cap:lef-read][req:LEF LAYER section]") {
  constexpr std::string_view text =
      "LAYER metal1 SPACING 0.14 RANGE 1 2 ; SPACING 0.28 ENDOFLINE 0.1 WITHIN 0.05 ; END metal1\n";
  SECTION("magic-compat ON") {
    LefRun run{true, 0.01F};
    define_scenario_layers(run.layers);
    run.read(text);
    CHECK(run.layers.at("metal1").route.spacing == 28);
  }
  SECTION("magic-compat OFF") {
    LefRun run{false, 0.01F};
    define_scenario_layers(run.layers);
    run.read(text);
    CHECK(run.layers.at("metal1").route.spacing == 14);
  }
}

TEST_CASE("reclassification error", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("LAYER metal1 TYPE CUT ; END metal1\nLAYER via1 TYPE ROUTING ; END via1\n"
           "LAYER via1 TYPE CUT ; TYPE MASTERSLICE ; END via1\nLAYER metal1 TYPE IMPLANT ; END metal1\n");
  CHECK(run.texts() ==
        Strings{"LEF read, Line 1 (Error): Attempt to reclassify layer metal1 from ROUTING to CUT",
                "LEF read, Line 2 (Error): Attempt to reclassify layer via1 from CUT to ROUTING",
                "LEF read, Line 4 (Warning): Unknown layer type \"IMPLANT\" in LEF file; ignoring."});
  CHECK(run.layers.at("metal1").entry.layer_class == lef::LayerClass::route);
}

TEST_CASE("route values apply to ROUTE entries only", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("LAYER via1 TYPE CUT ; WIDTH 0.5 ; SPACING 0.5 ; PITCH 1 ; DIRECTION HORIZONTAL ; END via1\n");
  CHECK(run.layers.updates == 0);
  CHECK(run.texts().empty());
}

TEST_CASE("a LAYER that is not in the table", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read(
      "LAYER poly9 TYPE MASTERSLICE ; END poly9\n"           // no magic type: skipped silently
      "LAYER m2contact TYPE CUT ; END m2contact\n"           // a magic contact
      "LAYER POLY TYPE ROUTING ; WIDTH 0.2 ; END POLY\n");  // a new ROUTE entry
  CHECK(run.texts() == Strings{"LEF read, Line 2 (Error): Layer m2contact maps to a magic contact layer; must be "
                               "defined in lef section of techfile"});
  CHECK(run.layers.records.count("poly9") == 0);
  CHECK(run.layers.records.count("m2contact") == 0);
  const auto& added = run.layers.at("POLY");
  CHECK(added.added);
  CHECK(added.entry.layer_class == lef::LayerClass::route);
  CHECK(added.entry.type == type_of("poly"));
  CHECK(added.route.width == 20);
}

TEST_CASE("a layer defined only for obstructions", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  run.layers.define({"obsm1"}, lef::LayerEntry{lef::LayerClass::route, 0, type_of("obsv1"), 0, std::nullopt});
  run.read("LAYER obsm1 WIDTH 0.5 ; END obsm1\nLAYER pwell WIDTH 0.5 ; END pwell\n");
  CHECK(run.texts() == Strings{"LEF read, Line 1 (Error): Layer obsm1 is only defined for obstructions!"});
  CHECK(run.layers.updates == 0);
}

TEST_CASE("a mismatched layer END reports and continues", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("LAYER metal1 WIDTH 0.1 ; END metal2 WIDTH 0.2 ; END metal1\n");
  CHECK(run.texts() == Strings{"LEF read, Line 1 (Error): Layer END statement missing."});
  CHECK(run.layers.at("metal1").route.width == 20);
}

TEST_CASE("skipped and unknown LAYER statements", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read(
      "LAYER metal1\n"
      "  TYPE ROUTING ;\n"
      "  MAXWIDTH 5 ; AREA 0.08 ; SPACINGTABLE PARALLELRUNLENGTH 0 WIDTH 0 0.1 ;\n"
      "  OFFSET 0.1 ; WIREEXTENSION 0.1 ; RESISTANCE RPERSQ 0.1 ; CAPACITANCE CPERSQDIST 1 ;\n"
      "  EDGECAPACITANCE 1 ; THICKNESS 1 ; HEIGHT 1 ; PROPERTY LEF57_X \"a ; b\" ;\n"
      "  MINIMUMDENSITY 1 ; ANTENNAAREARATIO 1 ; ANTENNADIFFAREARATIO 1 ; ANTENNASIDEAREARATIO 1 ;\n"
      "  FOREIGN x ; ENCLOSURE BELOW 0 0 ; PREFERENCLOSURE BELOW 0 0 ; OVERHANG 1 ; METALOVERHANG 1 ;\n"
      "  VIA x ; DEFAULT GENERATE\n"
      "  MINSTEP 0.05 ;\n"
      "  WIDTH 0.14 ;\n"
      "END metal1\n");
  CHECK(run.texts() == Strings{"LEF read, Line 9 (Message): Unknown keyword \"MINSTEP\" in LEF file; ignoring."});
  CHECK(run.layers.at("metal1").route.width == 14);
}

TEST_CASE("current density tables", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read(
      "LAYER metal1\n"
      "  ACCURRENTDENSITY AVERAGE FREQUENCY 1 2 ; WIDTH 1 2 ; TABLEENTRIES 1 2 ;\n"
      "  WIDTH 0.5 ;\n"
      "  DCCURRENTDENSITY AVERAGE WIDTH 1 2 ; TABLEENTRIES 3 4 ;\n"
      "  PITCH 0.3 ;\n"
      "  ACCURRENTDENSITY PEAK 1.5 ;\n"
      "  DIRECTION VERTICAL ;\n"
      "END metal1\n");
  CHECK(run.texts().empty());
  const auto& route = run.layers.at("metal1").route;
  CHECK(route.width == 50);
  CHECK(route.pitch == 30);
  CHECK(route.direction == lef::RouteDirection::vertical);
}

TEST_CASE("a non-numeric value leaves the field unchanged", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("LAYER metal1 WIDTH 0.5 ; WIDTH abc ; PITCH x ; END metal1\n");
  CHECK(run.layers.at("metal1").route.width == 50);
  CHECK_FALSE(run.layers.at("metal1").route.pitch);
}

TEST_CASE("LAYER and RECT statements inside a LAYER section", "[cap:lef-read][req:LEF LAYER section]") {
  LefRun run;
  define_scenario_layers(run.layers);
  run.read("LAYER metal1 LAYER poly2 ; RECT 0 0 1 1 ; LAYER metal2 ; RECT 0 0 1 ; WIDTH 0.5 ; END metal1\n");
  CHECK(run.texts() == Strings{"LEF read, Line 1 (Error): Don't know how to parse layer \"poly2\"",
                               "LEF read, Line 1 (Error): Try adding this name to the LEF techfile section",
                               "LEF read, Line 1 (Error): No layer defined for RECT.",
                               "LEF read, Line 1 (Error): Bad port geometry: RECT requires 4 values."});
  // The bad RECT's `;` was read as its fourth value, so the statement skip
  // took WIDTH's statement.
  CHECK_FALSE(run.layers.at("metal1").route.width);
}
