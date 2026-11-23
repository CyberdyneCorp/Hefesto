// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// DEF write: the file grammar, the header and DIEAREA, the output scale,
// COMPONENTS and PINS (capability def-write; openspec/changes/
// m6-lef-def-oasis tasks.md 8.1-8.3; Q-LEFDEF-93, 94, 103, 114).
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "write_fixture.hpp"

namespace def_write_test {
namespace {

constexpr std::string_view kHeaderTop =
    "VERSION 5.7 ;\n"
    "   NAMESCASESENSITIVE ON ;\n"
    "   DIVIDERCHAR \"/\" ;\n"
    "   BUSBITCHARS \"()\" ;\n"
    "   DESIGN top ;\n"
    "   TECHNOLOGY lefdef ;\n";

TEST_CASE("an empty cell writes the three fixed sections and END DESIGN",
          "[cap:def-write][req:DEF output file grammar and section order]") {
  Session s;
  db::CellDef& top = s.cell("top");
  REQUIRE(s.write(top, "").ok());
  const std::string text = s.read("top.def");
  CHECK(text.starts_with(kHeaderTop));
  CHECK(text.ends_with(
      "VIAS 0 ;\nEND VIAS\n\nCOMPONENTS 0 ;\nEND COMPONENTS\n\nPINS 0 ;\nEND PINS\n\nEND DESIGN\n\n"));
  CHECK(text.find("SPECIALNETS") == std::string::npos);
  CHECK(text.find("BLOCKAGES") == std::string::npos);
}

TEST_CASE("the default write is named after the cell, with 1000 units",
          "[cap:def-write][req:def write command]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 100, 50});
  REQUIRE(s.run("def write").ok());
  REQUIRE(s.exists("top.def"));
  const std::string text = s.read("top.def");
  CHECK(text.find("   UNITS DISTANCE MICRONS 1000 ;\n") != std::string::npos);
  CHECK(text.find("   DIEAREA ( 0 0 ) ( 1000 500 ) ;\n\n") != std::string::npos);
  CHECK(s.said("Generating DEF output top.def for cell top:"));
}

TEST_CASE("def write -units scales every coordinate", "[cap:def-write][req:def write command]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 100, 50});
  REQUIRE(s.run("def write out -units 2000").ok());
  const std::string text = s.read("out.def");
  CHECK(text.find("   UNITS DISTANCE MICRONS 2000 ;\n") != std::string::npos);
  CHECK(text.find("   DIEAREA ( 0 0 ) ( 2000 1000 ) ;\n") != std::string::npos);
}

TEST_CASE("the output scale is binary32 and printed with %.10g",
          "[cap:def-write][req:Output scale and number formatting]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 37, 1});
  def::DefWriteOptions o;
  o.units = 1;  // s = f32(0.01) * 1 = 0.00999999977648258
  REQUIRE(s.write(top, "u", o).ok());
  CHECK(s.read("u.def").find("   DIEAREA ( 0 0 ) ( 0.3700000048 0.009999999776 ) ;\n") != std::string::npos);
}

TEST_CASE("DIEAREA is the FIXED_BBOX property when it holds four integers", "[cap:def-write][req:DIEAREA]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 100, 50});
  top.properties().put("FIXED_BBOX", db::DimensionList{{-10, -10, 110, 60}});
  REQUIRE(s.write(top, "").ok());
  CHECK(s.read("top.def").find("   DIEAREA ( -100 -100 ) ( 1100 600 ) ;\n") != std::string::npos);
  top.properties().put("FIXED_BBOX", db::DimensionList{{1, 2, 3}});
  REQUIRE(s.write(top, "").ok());
  CHECK(s.read("top.def").find("   DIEAREA ( 0 0 ) ( 1000 500 ) ;\n") != std::string::npos);
}

TEST_CASE("component orientation follows defTransPos", "[cap:def-write][req:Component orientation]") {
  Session s;
  db::CellDef& c = s.cell("c", false);
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  db::CellDef& top = s.cell("top");
  const std::vector<std::pair<db::Transform, std::string>> cases{
      {db::Transform{1, 0, 0, 0, 1, 0}, "N"},   {db::Transform{-1, 0, 0, 0, -1, 0}, "S"},
      {db::Transform{0, 1, 0, -1, 0, 0}, "E"},  {db::Transform{0, -1, 0, 1, 0, 0}, "W"},
      {db::Transform{-1, 0, 0, 0, 1, 0}, "FN"}, {db::Transform{1, 0, 0, 0, -1, 0}, "FS"},
      {db::Transform{0, -1, 0, -1, 0, 0}, "FE"}, {db::Transform{0, 1, 0, 1, 0, 0}, "FW"},
  };
  for (std::size_t i = 0; i < cases.size(); ++i) {
    s.place(top, c, "u" + std::to_string(i), cases[i].first);
  }
  REQUIRE(s.write(top, "").ok());
  const std::string comps = section(s.read("top.def"), "COMPONENTS");
  for (std::size_t i = 0; i < cases.size(); ++i) {
    INFO("u" << i);
    const std::size_t at = comps.find("   - u" + std::to_string(i) + " c\n");
    REQUIRE(at != std::string::npos);
    const std::size_t eol = comps.find(" ;\n", comps.find('\n', at) + 1);
    CHECK(comps.substr(eol - cases[i].second.size() - 1, cases[i].second.size() + 1) == " " + cases[i].second);
  }
}

TEST_CASE("COMPONENTS: a simple placement", "[cap:def-write][req:COMPONENTS section]") {
  Session s;
  db::CellDef& inv = s.cell("inv", false);
  s.paint(inv, "metal1", db::Rect{0, 0, 46, 272});
  db::CellDef& top = s.cell("top");
  s.place(top, inv, "u1", db::Transform{1, 0, 100, 0, 1, 0});
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "COMPONENTS") ==
        "COMPONENTS 1 ;\n   - u1 inv\n      + PLACED ( 1000 0 ) N ;\nEND COMPONENTS\n\n");
}

TEST_CASE("COMPONENTS: a child bbox not at the origin, per mode (Q-LEFDEF-93)",
          "[cap:def-write][req:COMPONENTS section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& c = s.cell("c", false);
    s.paint(c, "metal1", db::Rect{-5, 0, 20, 10});
    db::CellDef& top = s.cell("top");
    s.place(top, c, "u2", db::Transform{1, 0, 100, 0, 1, 0});
    REQUIRE(s.write(top, "").ok());
    CHECK(section(s.read("top.def"), "COMPONENTS").find(on ? "+ PLACED ( 1000 0 ) N ;" : "+ PLACED ( 950 0 ) N ;") !=
          std::string::npos);
  }
}

TEST_CASE("COMPONENTS: a two-dimensional array, y outer and x inner", "[cap:def-write][req:COMPONENTS section]") {
  Session s;
  db::CellDef& c = s.cell("c", false);
  s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
  db::CellDef& top = s.cell("top");
  db::PlaceOptions o;
  o.id = "a";
  o.array = db::ArrayInfo{0, 1, 0, 1, 20, 30};
  REQUIRE(db::place(top, c, o).ok());
  db::recompute_bbox(top);
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "COMPONENTS") ==
        "COMPONENTS 4 ;\n"
        "   - a[0,0] c\n      + PLACED ( 0 0 ) N ;\n"
        "   - a[0,1] c\n      + PLACED ( 200 0 ) N ;\n"
        "   - a[1,0] c\n      + PLACED ( 0 300 ) N ;\n"
        "   - a[1,1] c\n      + PLACED ( 200 300 ) N ;\n"
        "END COMPONENTS\n\n");
}

TEST_CASE("COMPONENTS: reversed array bounds count per mode (Q-LEFDEF-94)",
          "[cap:def-write][req:COMPONENTS section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& c = s.cell("c", false);
    s.paint(c, "metal1", db::Rect{0, 0, 10, 10});
    db::CellDef& top = s.cell("top");
    db::PlaceOptions o;
    o.id = "r";
    o.array = db::ArrayInfo{3, 0, 0, 0, 20, 0};
    REQUIRE(db::place(top, c, o).ok());
    db::recompute_bbox(top);
    REQUIRE(s.write(top, "").ok());
    const std::string comps = section(s.read("top.def"), "COMPONENTS");
    CHECK(comps.starts_with(on ? "COMPONENTS -2 ;\n" : "COMPONENTS 4 ;\n"));
    CHECK(comps.find("   - r[3] c\n") != std::string::npos);
  }
}

TEST_CASE("PINS: an input signal pin", "[cap:def-write][req:PINS section]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.paint(top, "metal1", db::Rect{0, 0, 30, 30});
  s.label(top, Pin{"A", "metal1", db::Rect{10, 20, 14, 24}, true, db::PortClass::input, db::PortUse::signal});
  s.label(top, Pin{"B", "metal1", db::Rect{0, 0, 2, 2}, true});
  s.label(top, Pin{"note", "metal1", db::Rect{0, 0, 2, 2}, false});
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "PINS") ==
        "PINS 2 ;\n"
        "   - A + NET A\n"
        "     + DIRECTION INPUT\n"
        "     + USE SIGNAL\n"
        "     + PORT\n"
        "        + LAYER metal1 ( -20 -20 ) ( 20 20 )        + PLACED ( 120 220 ) N ;\n"
        "   - B + NET B\n"
        "     + PORT\n"
        "        + LAYER metal1 ( -10 -10 ) ( 10 10 )        + PLACED ( 10 10 ) N ;\n"
        "END PINS\n\n");
}

TEST_CASE("PINS: a scan pin per mode (Q-LEFDEF-103)", "[cap:def-write][req:PINS section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& top = s.cell("top");
    s.label(top, Pin{"SE", "metal1", db::Rect{10, 20, 14, 24}, true, db::PortClass::input, db::PortUse::scan});
    REQUIRE(s.write(top, "").ok());
    const std::string pins = section(s.read("top.def"), "PINS");
    if (on) {
      CHECK(pins.find("     + USE \n     + PORT\n        + LAYER metal1 ( -20 -20 ) ( 20 20 )        + PLACED") !=
            std::string::npos);
    } else {
      CHECK(pins ==
            "PINS 1 ;\n   - SE + NET SE\n     + DIRECTION INPUT\n     + USE SCAN\n     + PORT\n"
            "        + LAYER metal1 ( -20 -20 ) ( 20 20 )\n        + PLACED ( 120 220 ) N ;\nEND PINS\n\n");
    }
  }
}

TEST_CASE("PINS: a point label", "[cap:def-write][req:PINS section]") {
  Session s;
  db::CellDef& top = s.cell("top");
  s.label(top, Pin{"C", "metal2", db::Rect{7, 7, 7, 7}, true});
  REQUIRE(s.write(top, "").ok());
  CHECK(section(s.read("top.def"), "PINS").find("+ LAYER metal2 ( 0 0 ) ( 0 0 )        + PLACED ( 70 70 ) N ;") !=
        std::string::npos);
}

TEST_CASE("PINS: an unmapped layer's message per mode (Q-LEFDEF-114)", "[cap:def-write][req:PINS section]") {
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s(on);
    db::CellDef& top = s.cell("top");
    s.label(top, Pin{"P", "ndiff", db::Rect{0, 0, 2, 2}, true});
    REQUIRE(s.write(top, "").ok());
    CHECK(s.said(on ? "No LEF layer corresponding to layer P of pin \"ndiffusion\"."
                    : "No LEF layer corresponding to layer ndiffusion of pin \"P\"."));
    CHECK(section(s.read("top.def"), "PINS") ==
          "PINS 1 ;\n   - P + NET P\n     + PORT\n        + PLACED ( 10 10 ) N ;\nEND PINS\n\n");
  }
}

}  // namespace
}  // namespace def_write_test
