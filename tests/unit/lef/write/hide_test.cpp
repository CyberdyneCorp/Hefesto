// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lef-write: the -pinonly, -hide and -toplayer passes and the hidden
// obstructions (tasks.md 6.5; Q-LEFDEF-37, 38, 47, 51).
//
// Owner: LEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "write_fixture.hpp"

namespace hefesto::lef::write_test {
namespace {

// B = (0,0)-(100,200) through FIXED_BBOX, whatever the paint.
void fix_boundary(Session& s, db::CellDef& c) {
  c.flags().fixed_bbox = true;
  s.property(c, "FIXED_BBOX", db::DimensionList{{0, 0, 100, 200}});
}

LefWriteOptions hide(std::optional<db::Coord> setback, std::optional<db::Coord> pin_only = std::nullopt) {
  LefWriteOptions o;
  o.hide = setback;
  o.pin_only = pin_only;
  return o;
}

std::string pin(const std::string& text, std::string_view name = "A") {
  return block(text, "  PIN " + std::string{name}, "  END " + std::string{name});
}

std::string port_of(std::initializer_list<std::string_view> rects) {
  std::string out = "    PORT\n      LAYER met1 ;\n";
  for (const std::string_view r : rects) {
    out += "        RECT " + std::string{r} + " ;\n";
  }
  return out + "    END\n";
}

TEST_CASE("lef write -pinonly keeps the label area as the pin", "[cap:lef-write][req:Pin-only pin geometry]") {
  Session s;
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{10, 20, 30, 180});
  s.label(c, Port{"A", "metal1", db::Rect{10, 50, 30, 150}, 0});
  const std::string text = s.macro(c, hide(std::nullopt, 0));
  CHECK(pin(text) == "  PIN A\n" + port_of({"0.100 0.500 0.300 1.500"}) + "  END A\n");
  CHECK(block(text, "  OBS", "  END") ==
        "  OBS\n      LAYER met1 ;\n        RECT 0.100 1.500 0.300 1.800 ;\n        RECT 0.100 0.200 0.300 0.500 ;\n"
        "  END\n");
}

TEST_CASE("lef write -pinonly d keeps the net within d of the boundary", "[cap:lef-write][req:Pin-only pin geometry]") {
  Session s;
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{10, 0, 30, 180});
  s.label(c, Port{"A", "metal1", db::Rect{10, 50, 30, 150}, 0});
  const std::string text = s.macro(c, hide(std::nullopt, 10));
  CHECK(pin(text) == "  PIN A\n" + port_of({"0.100 0.500 0.300 1.500", "0.100 0.000 0.300 0.100"}) + "  END A\n");
  CHECK(block(text, "  OBS", "  END") ==
        "  OBS\n      LAYER met1 ;\n        RECT 0.100 1.500 0.300 1.800 ;\n        RECT 0.100 0.100 0.300 0.500 ;\n"
        "  END\n");
}

TEST_CASE("lef write -hide makes the pin the chunk holding the label", "[cap:lef-write][req:Hidden-detail pin geometry with zero setback]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{10, 20, 30, 180});
  s.paint(c, "metal1", db::Rect{30, 20, 80, 40});
  s.label(c, Port{"A", "metal1", db::Rect{10, 50, 30, 60}, 0});
  const std::string text = s.macro(c, hide(0));
  CHECK(pin(text) == "  PIN A\n" + port_of({"0.100 0.200 0.300 1.800"}) + "  END A\n");
  // The cover of the rest, minus the pin grown by the metal1 spacing (3).
  CHECK(block(text, "  OBS", "  END") == "  OBS\n      LAYER met1 ;\n        RECT 0.330 0.200 0.800 0.400 ;\n  END\n");
}

// Regression: the chunk search grows its window by its own width on each
// side it fills; once the window was wider than about 2^30 (here 40 * 3^16
// = 1721868840, still inside the usable plane), the next growth overflowed
// int (undefined behaviour, as in Magic). It now saturates, leaves the
// plane and stops with the last window, in both modes.
TEST_CASE("lef write -hide over material out to the plane's edge stops at the plane",
          "[cap:lef-write][req:Hidden-detail pin geometry with zero setback]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& c = s.cell("c");
    s.paint(c, "metal1", db::Rect{-1'000'000'000, -1'000'000'000, 1'000'000'000, 1'000'000'000});
    s.label(c, Port{"A", "metal1", db::Rect{0, 0, 20, 20}, 0});
    const std::string text = s.macro(c, hide(0));
    CHECK(pin(text) == "  PIN A\n" + port_of({"-8609343.000 -8609343.000 8609343.000 8609343.000"}) + "  END A\n");
  }
}

TEST_CASE("lef write -hide -pinonly p limits the chunk pin only with magic-compat OFF",
          "[cap:lef-write][req:Hidden-detail pin geometry with zero setback]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& c = s.cell("c");
    fix_boundary(s, c);
    s.paint(c, "metal1", db::Rect{40, 0, 60, 100});
    s.label(c, Port{"A", "metal1", db::Rect{40, 40, 60, 50}, 0});
    const std::string text = s.macro(c, hide(0, 10));
    CHECK(pin(text) == "  PIN A\n" +
                           (on ? port_of({"0.400 0.000 0.600 1.000"})
                               : port_of({"0.400 0.400 0.600 0.500", "0.400 0.000 0.600 0.100"})) +
                           "  END A\n");
  }
}

TEST_CASE("lef write -hide -pinonly 0 makes the label the pin", "[cap:lef-write][req:Hidden-detail pin geometry with zero setback]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{10, 20, 30, 180});
  s.label(c, Port{"A", "metal1", db::Rect{10, 50, 30, 60}, 0});
  CHECK(pin(s.macro(c, hide(0, 0))) == "  PIN A\n" + port_of({"0.100 0.500 0.300 0.600"}) + "  END A\n");
}

TEST_CASE("lef write -hide d makes the pin the net within d of the boundary",
          "[cap:lef-write][req:Hidden-detail pin geometry with positive setback]") {
  Session s;
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{40, 0, 60, 100});
  s.label(c, Port{"A", "metal1", db::Rect{40, 0, 60, 10}, 0});
  CHECK(pin(s.macro(c, hide(10))) == "  PIN A\n" + port_of({"0.400 0.000 0.600 0.100"}) + "  END A\n");
}

TEST_CASE("lef write -pinonly smaller than the setback limits the pin only with magic-compat OFF",
          "[cap:lef-write][req:Hidden-detail pin geometry with positive setback]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& c = s.cell("c");
    fix_boundary(s, c);
    s.paint(c, "metal1", db::Rect{40, 0, 60, 100});
    s.label(c, Port{"A", "metal1", db::Rect{40, 40, 60, 50}, 0});
    const std::string expected = on ? port_of({"0.400 0.400 0.600 0.500", "0.400 0.000 0.600 0.200"})
                                    : port_of({"0.400 0.400 0.600 0.500", "0.400 0.000 0.600 0.100"});
    CHECK(pin(s.macro(c, hide(20, 10))) == "  PIN A\n" + expected + "  END A\n");
  }
}

TEST_CASE("a setback over half the boundary hides nothing and is reported", "[cap:lef-write][req:Degenerate setback]") {
  Session s;
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{40, 0, 60, 100});
  s.paint(c, "metal1", db::Rect{0, 150, 30, 160});
  s.label(c, Port{"A", "metal1", db::Rect{40, 40, 60, 50}, 0});
  const std::string plain = s.macro(c);
  CHECK(s.macro(c, hide(60)) == plain);
  CHECK(s.texts("LEF-WRITE-006") ==
        std::vector<std::string>{"Macro \"c\": the -hide setback 60 is more than half the 100 x 200 boundary; nothing "
                                 "is hidden (Q-LEFDEF-47)"});
  CHECK(s.macro(c, hide(-5)) == plain);  // a negative distance disables hiding
}

TEST_CASE("lef write -toplayer writes only the net's top plane", "[cap:lef-write][req:Top-layer pins]") {
  Session s;
  db::CellDef& c = s.cell("c");
  s.paint(c, "metal1", db::Rect{0, 0, 20, 100});
  s.paint(c, "metal2", db::Rect{0, 0, 100, 20});
  s.paint(c, "m2c", db::Rect{0, 0, 20, 20});
  s.label(c, Port{"A", "metal1", db::Rect{0, 50, 20, 60}, 0});
  LefWriteOptions o;
  o.top_layer = true;
  const std::string text = s.macro(c, o);
  CHECK(pin(text) ==
        "  PIN A\n    PORT\n      LAYER met2 ;\n        RECT 0.000 0.000 1.000 0.200 ;\n    END\n  END A\n");
  CHECK(block(text, "  OBS", "  END") ==
        "  OBS\n      LAYER met1 ;\n        RECT 0.000 0.000 0.200 1.000 ;\n"
        "      LAYER via1 ;\n        RECT 0.000 0.000 0.200 0.200 ;\n  END\n");
}

// lef-write "Obstructions with zero setback", the abstract cover.
db::CellDef& cover_cell(Session& s) {
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{10, 50, 30, 150});
  s.paint(c, "metal1", db::Rect{5, 5, 95, 20});
  s.paint(c, "metal1", db::Rect{40, 180, 95, 195});
  s.label(c, Port{"A", "metal1", db::Rect{10, 50, 30, 150}, 0});
  return c;
}

TEST_CASE("lef write -hide covers each hidden layer and cuts the pins out", "[cap:lef-write][req:Obstructions with zero setback]") {
  Session s;
  db::CellDef& c = cover_cell(s);
  const std::string text = s.macro(c, hide(0));
  CHECK(pin(text) == "  PIN A\n" + port_of({"0.100 0.500 0.300 1.500"}) + "  END A\n");
  CHECK(block(text, "  OBS", "  END") ==
        "  OBS\n      LAYER met1 ;\n        RECT 0.050 1.530 0.950 1.950 ;\n        RECT 0.050 0.470 0.070 1.530 ;\n"
        "        RECT 0.330 0.470 0.950 1.530 ;\n        RECT 0.050 0.050 0.950 0.470 ;\n  END\n");
}

TEST_CASE("OBS_BBOX enlarges the cover, and a malformed one is reported",
          "[cap:lef-write][req:Obstructions with zero setback]") {
  Session s;
  db::CellDef& c = cover_cell(s);
  s.property(c, "OBS_BBOX", db::DimensionList{{0, 0, 100, 200}});
  CHECK(block(s.macro(c, hide(0)), "  OBS", "  END").starts_with("  OBS\n      LAYER met1 ;\n        RECT 0.000 1.530 1.000 2.000 ;\n"));
  s.property(c, "OBS_BBOX", std::string{"1 2 3"});
  const std::string text = s.macro(c, hide(0));
  CHECK(block(text, "  OBS", "  END").starts_with("  OBS\n      LAYER met1 ;\n        RECT 0.050 1.530 0.950 1.950 ;\n"));
  CHECK(s.texts("LEF-WRITE-007") ==
        std::vector<std::string>{"Improper values for obstruction bounding box OBS_BBOX property"});
}

TEST_CASE("a malformed OBS_BBOX message has no line end with magic-compat ON only",
          "[cap:lef-write][req:Obstructions with zero setback]") {
  // Q-LEFDEF-134 (compat): Magic's TxError has no newline, so its message
  // and the next one print on one line (LEFDEF-132).
  for (const bool on : {true, false}) {
    INFO("magic-compat " << (on ? "ON" : "OFF"));
    Session s{on};
    db::CellDef& c = cover_cell(s);
    s.property(c, "OBS_BBOX", std::string{"1 2 3"});
    (void)s.macro(c, hide(0));
    bool found = false;
    for (const auto& d : s.messages) {
      if (d.code == "LEF-WRITE-007") {
        found = true;
        CHECK(d.no_newline == on);
      }
    }
    CHECK(found);
  }
}

TEST_CASE("lef write -hide d keeps the periphery's detail", "[cap:lef-write][req:Obstructions with positive setback]") {
  Session s;
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{2, 2, 8, 8});
  CHECK(block(s.macro(c, hide(10)), "  OBS", "  END") ==
        "  OBS\n      LAYER met1 ;\n        RECT 0.020 0.020 0.080 0.080 ;\n  END\n");
}

TEST_CASE("a tile crossing a setback strip is copied whole only with magic-compat ON",
          "[cap:lef-write][req:Obstructions with positive setback]") {
  for (const bool on : {true, false}) {
    Session s(on);
    db::CellDef& c = s.cell("c");
    fix_boundary(s, c);
    // pwell is a substrate type (no cover of its own); the nwell cover spans
    // the well plane's material within the setback, (10,40)-(50,60). The
    // whole strip tile paints over it; the clipped one leaves it.
    s.paint(c, "pwell", db::Rect{2, 40, 50, 60});
    CHECK(block(s.macro(c, hide(10)), "  OBS", "  END") ==
          (on ? "  OBS\n      LAYER pwell ;\n        RECT 0.020 0.400 0.500 0.600 ;\n  END\n"
              : "  OBS\n      LAYER pwell ;\n        RECT 0.020 0.400 0.100 0.600 ;\n"
                "      LAYER nwell ;\n        RECT 0.100 0.400 0.500 0.600 ;\n  END\n"));
  }
}

TEST_CASE("lef write -hide d writes the strips' contacts as cuts", "[cap:lef-write][req:Obstructions with positive setback]") {
  Session s;
  db::CellDef& c = s.cell("c");
  fix_boundary(s, c);
  s.paint(c, "metal1", db::Rect{0, 0, 8, 8});
  s.paint(c, "m2c", db::Rect{0, 0, 8, 8});
  s.paint(c, "m2c", db::Rect{50, 100, 58, 108});  // inside the setback: hidden
  const std::string obs = block(s.macro(c, hide(10)), "  OBS", "  END");
  CHECK(obs.find("      LAYER via1 ;\n        RECT 0.000 0.000 0.080 0.080 ;\n") != std::string::npos);
  CHECK(obs.find("RECT 0.500 1.000 0.580 1.080") == std::string::npos);
}

}  // namespace
}  // namespace hefesto::lef::write_test
