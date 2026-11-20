// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-workstream test of the round-4 integration (openspec/changes/
// m6-lef-def-oasis tasks.md 8.1, 8.3): a design read with `def read` (DEFR)
// and written back with `def write` (DEFW) through the shared `lef`/`def`
// dispatcher, then read again.
// Owner: INTEGRATION.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "write_fixture.hpp"

namespace def_write_test {
namespace {

constexpr std::string_view kDesign =
    "VERSION 5.7 ;\n"
    "DIVIDERCHAR \"/\" ;\n"
    "BUSBITCHARS \"[]\" ;\n"
    "DESIGN rt ;\n"
    "UNITS DISTANCE MICRONS 1000 ;\n"
    "DIEAREA ( 0 0 ) ( 1000 500 ) ;\n"
    "PINS 1 ;\n"
    "- A + NET A + DIRECTION INPUT + USE SIGNAL\n"
    "  + LAYER metal1 ( -50 -50 ) ( 50 50 ) + PLACED ( 250 250 ) N ;\n"
    "END PINS\n"
    "END DESIGN\n";

}  // namespace

TEST_CASE("def read then def write keeps the die area and the pins", "[cap:def-write][req:def write command]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    s.file("/t/rt.def", std::string{kDesign});
    const auto read = s.run("def read rt");
    for (const auto& d : s.messages) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    REQUIRE(read.ok());
    // The fixture has no window, so the design read is made the edit root.
    s.editor.edit_root = s.editor.cells.find("rt");
    REQUIRE(s.editor.edit_root != nullptr);
    const auto written = s.run("def write");
    for (const auto& d : written.diagnostics) {
      UNSCOPED_INFO(d.code << ": " << d.text);
    }
    REQUIRE(written.ok());
    REQUIRE(s.exists("rt.def"));
    const std::string text = s.read("rt.def");
    INFO(text);
    CHECK(text.find("   DESIGN rt ;\n") != std::string::npos);
    CHECK(text.find("   DIEAREA ( 0 0 ) ( 1000 500 ) ;\n") != std::string::npos);
    CHECK(text.find("PINS 1 ;\n") != std::string::npos);
    CHECK(text.find("- A + NET A") != std::string::npos);
    CHECK(text.find("+ LAYER metal1 ( -50 -50 ) ( 50 50 )") != std::string::npos);
    CHECK(text.find("( 250 250 ) N ;") != std::string::npos);

    // The written file reads back to the same design.
    Session again{on};
    again.file("/t/rt.def", text);
    REQUIRE(again.run("def read rt").ok());
    again.editor.edit_root = again.editor.cells.find("rt");
    REQUIRE(again.editor.edit_root != nullptr);
    REQUIRE(again.run("def write").ok());
    CHECK(again.read("rt.def") == text);
  }
}

}  // namespace def_write_test
