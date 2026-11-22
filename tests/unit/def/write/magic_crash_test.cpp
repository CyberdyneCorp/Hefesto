// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Hefesto-only regressions: the inputs of golden cases LEFDEF-317, 318,
// 320, 330, 333 and 334 as first authored, on which Magic 8.3.694 crashes
// with SIGSEGV in `def write` (decision D4 of 2026-11-19; kept under
// tests/golden/LEFDEF/magic-crash/, since GOLDEN re-authors the cases).
// Each has a node that is neither a port nor a special net and has
// top-level paint (m1_0_50#, a_n5# and x1/sig*b, m3_0_50#, m1_0_0#,
// m2_34_40#), and Magic's blockage pass calls defBlockageGeometryFunc
// with DBSrConnect's (tile, dinfo, plane, cdata) arguments against its
// (tile, plane, cdata) parameters, so the collector reads its data from
// the plane number (Q-LEFDEF-101). Hefesto writes the geometry under
// BLOCKAGES in both modes and says Magic would have crashed.
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "write_fixture.hpp"

namespace def_write_test {
namespace {

struct CaseRun {
  std::string def;
  bool crash_note = false;
  std::vector<diag::Diagnostic> messages;
};

/// One file of a case's first input, tests/golden/LEFDEF/magic-crash/<id>/.
std::string crash_input(const std::string& id, std::string_view name) {
  const std::filesystem::path path =
      std::filesystem::path{HEFESTO_SOURCE_DIR} / "tests/golden/LEFDEF/magic-crash" / id / name;
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

CaseRun run_case(const std::string& id, std::string_view command, bool magic_compat) {
  Session s(magic_compat, crash_input(id, "lefdef.tech"));
  s.file("/t/top.mag", crash_input(id, "top.mag"));
  s.file("/t/top.ext", crash_input(id, "top.ext"));
  REQUIRE(s.run("load top").ok());
  s.editor.edit_root = s.editor.cells.find("top");
  REQUIRE(s.editor.edit_root != nullptr);
  const auto result = s.run(command);
  for (const auto& d : s.messages) {
    UNSCOPED_INFO(d.code << ": " << d.text);
  }
  REQUIRE(result.ok());
  UNSCOPED_INFO(s.read("top.def"));
  CaseRun out;
  out.def = s.read("top.def");
  out.messages = s.messages;
  out.crash_note = std::any_of(s.messages.begin(), s.messages.end(), [](const diag::Diagnostic& d) {
    return d.code == "DEF-WRITE-012" && d.native_only && d.text.find("Q-LEFDEF-101") != std::string::npos;
  });
  return out;
}

TEST_CASE("LEFDEF-333's floating metal1 net is written as a blockage",
          "[cap:def-write][req:BLOCKAGES section][magic-crash]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    const CaseRun r = run_case("LEFDEF-333", "def write", on);
    CHECK(r.crash_note);
    CHECK(r.def ==
          "VERSION 5.7 ;\n"
          "   NAMESCASESENSITIVE ON ;\n"
          "   DIVIDERCHAR \"/\" ;\n"
          "   BUSBITCHARS \"()\" ;\n"
          "   DESIGN top ;\n"
          "   TECHNOLOGY lefdef ;\n"
          "   UNITS DISTANCE MICRONS 1000 ;\n"
          "   DIEAREA ( 0 0 ) ( 500 30 ) ;\n"
          "\n"
          "VIAS 0 ;\nEND VIAS\n\n"
          "COMPONENTS 0 ;\nEND COMPONENTS\n\n"
          "PINS 0 ;\nEND PINS\n\n"
          "BLOCKAGES 1 ;\n"
          "   - LAYER metal1\n"
          "      RECT ( 0 0 ) ( 500 30 ) ;\n"
          "END BLOCKAGES\n\n"
          "END DESIGN\n\n");
  }
}

TEST_CASE("LEFDEF-317 and 318: the unlabeled net is a blockage, the others are nets",
          "[cap:def-write][req:Net classification][magic-crash]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    const CaseRun r = run_case("LEFDEF-317", "def write", on);
    CHECK(r.crash_note);
    CHECK(section(r.def, "SPECIALNETS").starts_with("SPECIALNETS 2 ;\n"));
    CHECK(r.def.find("   - vdd!\n      + ROUTED metal1 100  ( 0 1950 ) ( 2000 * ) ;\n") != std::string::npos);
    CHECK(r.def.find("   - clkint\n      + ROUTED metal1 100  ( 1000 1050 ) ( 1600 * ) ;\n") != std::string::npos);
    CHECK(section(r.def, "NETS") ==
          "NETS 1 ;\n"
          "   - A ( PIN A )\n"
          "      + ROUTED metal1 TAPERRULE metal1_width_100  ( 50 1050 ) ( 550 * ) ;\n"
          "END NETS\n\n");
    CHECK(section(r.def, "NONDEFAULTRULES") ==
          "NONDEFAULTRULES 1 ;\n  - metal1_width_100\n     + LAYER metal1 WIDTH 100 ;\nEND NONDEFAULTRULES\n\n");
    CHECK(section(r.def, "BLOCKAGES") ==
          "BLOCKAGES 1 ;\n   - LAYER metal1\n      RECT ( 0 500 ) ( 600 600 ) ;\nEND BLOCKAGES\n\n");

    const CaseRun all = run_case("LEFDEF-318", "def write -allspecial", on);
    CHECK(all.crash_note);
    CHECK(section(all.def, "SPECIALNETS").starts_with("SPECIALNETS 3 ;\n"));
    CHECK(all.def.find("   - A ( PIN A )\n      + ROUTED metal1 100  ( 0 1050 ) ( 600 * ) ;\n") != std::string::npos);
    CHECK(all.def.find("\nNETS ") == std::string::npos);
    CHECK(section(all.def, "BLOCKAGES") == section(r.def, "BLOCKAGES"));
  }
}

TEST_CASE("LEFDEF-320: the hierarchical and internal names are blockages",
          "[cap:def-write][req:DEF net naming][magic-crash]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    const CaseRun r = run_case("LEFDEF-320", "def write", on);
    CHECK(r.crash_note);
    CHECK(r.def.find("   - net_1\n") != std::string::npos);
    CHECK(r.def.find("a_n5") == std::string::npos);
    CHECK(r.def.find("x1/sig_b") == std::string::npos);
    CHECK(section(r.def, "BLOCKAGES") ==
          "BLOCKAGES 1 ;\n   - LAYER metal1\n      RECT ( 0 400 ) ( 600 430 )\n      RECT ( 0 200 ) ( 600 230 ) ;\n"
          "END BLOCKAGES\n\n");
  }
}

TEST_CASE("LEFDEF-330: -anal with a floating metal3 net", "[cap:def-write][req:NONDEFAULTRULES section][magic-crash]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    const CaseRun r = run_case("LEFDEF-330", "def write -anal", on);
    CHECK(r.crash_note);
    // `wide` and `n1` are labeled non-port nets, so SPECIALNETS (no rule).
    CHECK(section(r.def, "SPECIALNETS").starts_with("SPECIALNETS 2 ;\n"));
    CHECK(r.def.find("NONDEFAULTRULES") == std::string::npos);
    CHECK(section(r.def, "BLOCKAGES") ==
          "BLOCKAGES 1 ;\n   - LAYER metal3\n      RECT ( 0 500 ) ( 1000 550 ) ;\nEND BLOCKAGES\n\n");
  }
}

TEST_CASE("LEFDEF-334: vias, obstructions and a floating metal2 net",
          "[cap:def-write][req:Table enumeration order][magic-crash]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    const CaseRun r = run_case("LEFDEF-334", "def write", on);
    CHECK(r.crash_note);
    CHECK(section(r.def, "VIAS").starts_with("VIAS 2 ;\n"));
    CHECK(r.def.find("   - via1_40_40\n") != std::string::npos);
    CHECK(r.def.find("   - via2_40_40\n") != std::string::npos);
    const std::string blockages = section(r.def, "BLOCKAGES");
    CHECK(blockages.find("   - LAYER metal1\n      RECT ( 1000 0 ) ( 1200 200 ) ;\n") != std::string::npos);
    CHECK(blockages.find("   - LAYER metal2\n") != std::string::npos);
    CHECK(blockages.find("RECT ( 340 400 ) ( 600 440 )") != std::string::npos);
    CHECK(blockages.find("RECT ( 1000 400 ) ( 1200 600 )") != std::string::npos);
  }
}

}  // namespace
}  // namespace def_write_test
