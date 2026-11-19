// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MACRO sections that annotate an existing cell (capability lef-read
// "Annotation of existing cells"; openspec/changes/m6-lef-def-oasis
// tasks.md 5.5), with Q-LEFDEF-21 in both modes.

#include <catch2/catch_test_macros.hpp>

#include "hefesto/db/hierarchy.hpp"
#include "macro_fixture.hpp"

using namespace lefread_test;

namespace {

constexpr std::uint8_t kAll = db::kPortNorth | db::kPortEast | db::kPortSouth | db::kPortWest;

void put(db::CellDef& cell, db::Label l) {
  REQUIRE(db::put_label(cell, std::move(l)).ok());
}

void paint(db::CellDef& cell, std::string_view type, const db::Rect& r) {
  REQUIRE(db::paint_type(cell, *technology(), r, type_of(type)).ok());
  db::recompute_bbox(cell);
}

}  // namespace

TEST_CASE("Area label becomes a port without geometry change", "[cap:lef-read][req:Annotation of existing cells]") {
  for (const bool on : {true, false}) {
    MacroRun run{on};
    db::CellDef& inv = run.existing("inv");
    put(inv, label("A", "metal1", db::Rect{0, 0, 10, 10}));
    run.read("MACRO inv\n  PIN A\n    DIRECTION INPUT ;\n    USE SIGNAL ;\n    PORT\n      LAYER metal1 ;\n"
             "      RECT 0 0 0.5 0.5 ;\n    END\n  END A\nEND inv\n");
    CHECK(run.texts().empty());
    CHECK(run.host.annotated == Strings{"inv"});
    const db::Label& a = inv.labels().at(0);
    REQUIRE(inv.labels().size() == 1);
    CHECK(a.is_port());
    CHECK(a.port_class == db::PortClass::input);
    CHECK(a.port_use == db::PortUse::signal);
    CHECK(a.port_directions == kAll);
    CHECK(a.port_index == 0);
    CHECK(a.rect == db::Rect{0, 0, 10, 10});
    CHECK_FALSE(a.sticky);
    CHECK(tiles(inv, "metal1").empty());
    // Annotation sets no LEFview and, without SIZE, no FIXED_BBOX.
    CHECK_FALSE(string_property(inv, "LEFview").has_value());
    CHECK_FALSE(fixed_bbox(inv).has_value());
  }
}

TEST_CASE("an annotated port label keeps its index; a new one takes the next",
          "[cap:lef-read][req:Annotation of existing cells]") {
  MacroRun run;
  db::CellDef& cell = run.existing("c");
  db::Label p = label("P", "metal1", db::Rect{0, 0, 2, 2});
  p.port_directions = db::kPortNorth;
  p.port_index = 7;
  put(cell, p);
  put(cell, label("Q", "metal1", db::Rect{4, 4, 6, 6}));
  run.read("MACRO c\n  PIN P\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END P\n"
           "  PIN Q\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END Q\n  SIZE 9 BY 9 ;\nEND c\n");
  CHECK(cell.labels().at(0).port_index == 7);
  CHECK(cell.labels().at(0).port_directions == kAll);
  CHECK(cell.labels().at(1).port_index == 8);
  // With SIZE, annotation sets FIXED_BBOX.
  CHECK(fixed_bbox(cell) == db::Rect{0, 0, 9, 9});
  CHECK(cell.flags().fixed_bbox);
}

TEST_CASE("Point label replaced by LEF geometry", "[cap:lef-read][req:Annotation of existing cells]") {
  for (const bool on : {true, false}) {
    MacroRun run{on};
    db::CellDef& cell = run.existing("c");
    paint(cell, "metal1", db::Rect{0, 0, 20, 20});
    put(cell, label("Y", "metal1", db::Rect{5, 5, 5, 5}));
    run.read("MACRO c\n  PIN Y\n    PORT\n      LAYER metal1 ;\n      RECT 2 2 8 8 ;\n    END\n  END Y\nEND c\n");
    REQUIRE(cell.labels().size() == 1);
    const db::Label& y = cell.labels().front();
    CHECK(y.rect == db::Rect{2, 2, 8, 8});
    CHECK(y.type == type_of("metal1"));
    CHECK(y.is_port());
    CHECK(y.sticky);
    CHECK(y.port_index == 0);
    CHECK(tiles(cell, "metal1") == std::vector<db::Rect>{{0, 0, 20, 20}});
  }
}

TEST_CASE("annotation paints per magic-compat", "[cap:lef-read][req:Annotation of existing cells]") {
  // A POLYGON is painted with no coverage check with magic-compat ON;
  // with it OFF its rectangles are checked as RECTs and nothing is painted.
  const std::string text =
      "MACRO c\n  PIN P\n    PORT\n      LAYER metal1 ;\n      POLYGON 0 0 4 0 4 4 0 4 ;\n    END\n  END P\nEND c\n";
  SECTION("magic-compat ON") {
    MacroRun run{true};
    db::CellDef& cell = run.existing("c");
    put(cell, label("P", "metal1", db::Rect{1, 1, 1, 1}));
    run.read(text);
    CHECK(tiles(cell, "metal1") == std::vector<db::Rect>{{0, 0, 4, 4}});
    REQUIRE(cell.labels().size() == 1);
    CHECK(cell.labels().front().rect == db::Rect{0, 0, 4, 4});
    CHECK(cell.labels().front().is_port());
  }
  SECTION("magic-compat OFF") {
    MacroRun run{false};
    db::CellDef& cell = run.existing("c");
    put(cell, label("P", "metal1", db::Rect{1, 1, 1, 1}));
    run.read(text);
    CHECK(tiles(cell, "metal1").empty());
    REQUIRE(cell.labels().size() == 1);
    CHECK(cell.labels().front().rect == db::Rect{1, 1, 1, 1});
    CHECK_FALSE(cell.labels().front().is_port());
  }
  SECTION("magic-compat OFF: a covered polygon is listed, not painted") {
    MacroRun run{false};
    db::CellDef& cell = run.existing("c");
    paint(cell, "metal1", db::Rect{-1, -1, 5, 5});
    put(cell, label("P", "metal1", db::Rect{1, 1, 1, 1}));
    run.read(text);
    CHECK(tiles(cell, "metal1") == std::vector<db::Rect>{{-1, -1, 5, 5}});
    REQUIRE(cell.labels().size() == 1);
    CHECK(cell.labels().front().rect == db::Rect{0, 0, 4, 4});
    CHECK(cell.labels().front().is_port());
  }
}

TEST_CASE("Incompatible port rectangle rejected", "[cap:lef-read][req:Annotation of existing cells]") {
  for (const bool on : {true, false}) {
    MacroRun run{on};
    db::CellDef& cell = run.existing("c");
    put(cell, label("X", "metal2", db::Rect{5, 5, 6, 6}));
    run.read("MACRO c\n  PIN Z\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END Z\nEND c\n");
    CHECK(tiles(cell, "metal1").empty());
    REQUIRE(cell.labels().size() == 1);
    CHECK(cell.labels().front().text == "X");
  }
}

TEST_CASE("a port rectangle covered by a subcell is accepted", "[cap:lef-read][req:Annotation of existing cells]") {
  MacroRun run;
  db::CellDef& child = run.existing("child");
  paint(child, "metal1", db::Rect{0, 0, 10, 10});
  db::CellDef& cell = run.existing("c");
  db::PlaceOptions where;
  where.transform = db::Transform{1, 0, 100, 0, 1, 0};
  REQUIRE(db::place(cell, child, where).ok());
  db::recompute_bbox(cell);
  run.read("MACRO c\n  PIN Z\n    PORT\n      LAYER metal1 ;\n      RECT 101 1 102 2 ;\n      RECT 0 0 1 1 ;\n"
           "    END\n  END Z\nEND c\n");
  REQUIRE(cell.labels().size() == 1);
  CHECK(cell.labels().front().rect == db::Rect{101, 1, 102, 2});
}

TEST_CASE("Bus delimiter translation", "[cap:lef-read][req:Annotation of existing cells]") {
  MacroRun run;
  db::CellDef& cell = run.existing("c");
  put(cell, label("D[3]", "metal1", db::Rect{0, 0, 4, 4}));
  put(cell, label("clk", "metal1", db::Rect{6, 6, 8, 8}));
  run.read("MACRO c\n  PIN D<3>\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END D<3>\n"
           "  PIN CLK\n    USE CLOCK ;\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END CLK\nEND c\n");
  REQUIRE(cell.labels().size() == 2);
  CHECK(cell.labels()[0].text == "D[3]");
  CHECK(cell.labels()[0].is_port());
  CHECK(cell.labels()[1].text == "clk");
  CHECK(cell.labels()[1].port_use == db::PortUse::clock);
}

TEST_CASE("one PORT annotates every area label of the pin", "[cap:lef-read][req:Annotation of existing cells]") {
  MacroRun run;
  db::CellDef& cell = run.existing("c");
  put(cell, label("A", "metal1", db::Rect{0, 0, 2, 2}));
  put(cell, label("A", "metal1", db::Rect{4, 4, 6, 6}));
  run.read("MACRO c\n  PIN A\n    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n"
           "    PORT\n      LAYER metal1 ;\n      RECT 0 0 1 1 ;\n    END\n  END A\nEND c\n");
  REQUIRE(cell.labels().size() == 2);
  CHECK(cell.labels()[0].is_port());
  CHECK(cell.labels()[1].is_port());
  CHECK(cell.labels()[0].port_index == 0);
  CHECK(cell.labels()[1].port_index == 1);
}
