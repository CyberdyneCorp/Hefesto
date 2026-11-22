// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// def-read: COMPONENTS, the placement transform and unplaced components
// (tasks.md 7.3; Q-LEFDEF-83, 85).
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include <catch2/catch_test_macros.hpp>

#include "def_fixture.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"

namespace def_read_test {
namespace {

// A master cell with metal1 over `box`.
db::CellDef& master(Session& s, const std::string& name, const db::Rect& box) {
  auto made = s.editor.cells.create(name, s.editor.technology->plane_count());
  REQUIRE(made.ok());
  db::CellDef& def = *made.value();
  def.flags().available = true;
  REQUIRE(db::paint_type(def, *s.editor.technology, box, s.type("metal1")).ok());
  db::recompute_bbox(def);
  return def;
}

std::string components(std::string_view body, int count = 1) {
  return "DESIGN top ;\nCOMPONENTS " + std::to_string(count) + " ;\n" + std::string{body} +
         "\nEND COMPONENTS\nEND DESIGN\n";
}

const db::CellUse& use(Session& s, std::string_view id) {
  const db::CellUse* u = db::find_use(s.cell("top"), id);
  INFO("use " << id);
  REQUIRE(u != nullptr);
  return *u;
}

// --- COMPONENTS section -------------------------------------------------------------

TEST_CASE("slash in instance name", "[cap:def-read][req:COMPONENTS section]") {
  Session s;
  master(s, "sky130_fd_sc_hd__inv_1", db::Rect{0, 0, 100, 200});
  REQUIRE(s.read(components("- core/u1 sky130_fd_sc_hd__inv_1 + PLACED ( 0 0 ) N ;")).ok());
  CHECK(use(s, "core_u1").def == &s.cell("sky130_fd_sc_hd__inv_1"));
  CHECK(s.count("Character in instance name converted to underscore.") == 1);
  CHECK(s.said("  Processed 1 subcell instances total."));
}

TEST_CASE("duplicate instance name is renamed (Q-LEFDEF-83)", "[cap:def-read][req:COMPONENTS section]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    db::CellDef& inv = master(s, "inv", db::Rect{0, 0, 100, 200});
    REQUIRE(s.read("DESIGN top ;\nEND DESIGN\n").ok());
    db::PlaceOptions first;
    first.id = "u1";
    REQUIRE(db::place(s.cell("top"), inv, first).ok());
    REQUIRE(s.read(components("- u1 inv + PLACED ( 0 0 ) N ;")).ok());
    CHECK(s.said("Duplicate instance-id for cell inv (u1) will be renamed"));
    CHECK(use(s, "inv_0").def == &inv);
    CHECK(s.cell("top").uses().size() == 2);
  }
}

TEST_CASE("missing master", "[cap:def-read][req:COMPONENTS section]") {
  Session s;
  REQUIRE(s.read(components("- u1 FOO + PLACED ( 0 0 ) N ;")).ok());
  CHECK(s.said("Cell FOO is not defined.  Maybe you have not read the corresponding LEF file?"));
  CHECK(s.cell("top").uses().empty());
  CHECK(s.editor.cells.find("FOO") == nullptr);
}

TEST_CASE("a master read from its .mag file", "[cap:def-read][req:COMPONENTS section]") {
  Session s;
  std::vector<std::string> asked;
  s.options.read_master = [&](db::CellDef& def) {
    asked.push_back(def.name());
    def.flags().available = true;
    return db::paint_type(def, *s.editor.technology, db::Rect{0, 0, 50, 50}, s.type("metal2")).ok();
  };
  REQUIRE(s.read(components("- u1 nand + FIXED ( 100 0 ) N + SOURCE DIST + WEIGHT 2 + FOO ;")).ok());
  CHECK(asked == std::vector<std::string>{"nand"});
  CHECK(use(s, "u1").transform == db::Transform{1, 0, 100, 0, 1, 0});
  CHECK(s.cell("nand").bbox() == db::Rect{0, 0, 50, 50});
  CHECK(s.said("Unknown component property \"FOO\" in COMPONENT definition; ignoring."));
}

TEST_CASE("annotation skips COMPONENTS", "[cap:def-read][req:COMPONENTS section]") {
  Session s;
  master(s, "inv", db::Rect{0, 0, 100, 200});
  s.options.annotate = true;
  REQUIRE(s.read(components("- u1 inv + PLACED ( 0 0 ) N ;")).ok());
  CHECK(s.cell("top").uses().empty());
}

// --- Component placement transform --------------------------------------------------

TEST_CASE("south and east orientations", "[cap:def-read][req:Component placement transform]") {
  Session s;
  master(s, "inv", db::Rect{0, 0, 100, 200});
  REQUIRE(s.read(components("- us inv + PLACED ( 1000 2000 ) S ;\n- ue inv + FIXED ( 1000 2000 ) E ;", 2)).ok());
  CHECK(use(s, "us").transform == db::Transform{-1, 0, 1100, 0, -1, 2200});
  CHECK(use(s, "us").bbox == db::Rect{1000, 2000, 1100, 2200});
  CHECK(use(s, "ue").transform == db::Transform{0, 1, 1000, -1, 0, 2100});
  CHECK(use(s, "ue").bbox == db::Rect{1000, 2000, 1200, 2100});
}

TEST_CASE("every orientation keeps the lower-left corner", "[cap:def-read][req:Component placement transform]") {
  Session s;
  master(s, "inv", db::Rect{0, 0, 100, 200});
  std::string body;
  for (const std::string o : {"N", "S", "E", "W", "FN", "FS", "FE", "FW"}) {
    body += "- u" + o + " inv + COVER ( 300 400 ) " + o + " ;\n";
  }
  REQUIRE(s.read(components(body, 8)).ok());
  for (const std::string o : {"N", "S", "E", "W", "FN", "FS", "FE", "FW"}) {
    INFO(o);
    CHECK(use(s, "u" + o).bbox.x0 == 300);
    CHECK(use(s, "u" + o).bbox.y0 == 400);
  }
}

TEST_CASE("FIXED_BBOX takes precedence", "[cap:def-read][req:Component placement transform]") {
  Session s;
  db::CellDef& inv = master(s, "inv", db::Rect{-10, -5, 110, 205});
  db::set_property(inv, "FIXED_BBOX", db::PropertyValue{db::DimensionList{{0, 0, 100, 200}}});
  REQUIRE(inv.flags().fixed_bbox);
  REQUIRE(s.read(components("- u1 inv + PLACED ( 0 0 ) N ;")).ok());
  CHECK(use(s, "u1").transform == db::Transform{});
}

TEST_CASE("bad orientation and location keep the transform", "[cap:def-read][req:Component placement transform]") {
  Session s;
  master(s, "inv", db::Rect{0, 0, 100, 200});
  REQUIRE(s.read(components("- u1 inv + PLACED ( 10 10 ) NE ;\n- u2 inv + PLACED 10 10 N ;", 2)).ok());
  CHECK(s.said("Unknown macro orientation \"NE\"."));
  CHECK(s.said("Cannot parse location: must be ( X Y ) orient"));
  CHECK(use(s, "u1").transform == db::Transform{});
  CHECK(use(s, "u2").transform == db::Transform{});
}

TEST_CASE("the last placement wins", "[cap:def-read][req:Component placement transform]") {
  Session s;
  master(s, "inv", db::Rect{0, 0, 100, 200});
  REQUIRE(s.read(components("- u1 inv + PLACED ( 10 10 ) N + FIXED ( 500 0 ) N ;")).ok());
  CHECK(use(s, "u1").transform == db::Transform{1, 0, 500, 0, 1, 0});
}

// --- Unplaced components (Q-LEFDEF-85) ----------------------------------------------

TEST_CASE("unplaced and no placement", "[cap:def-read][req:Unplaced components]") {
  for (const bool compat : {true, false}) {
    Session s{compat};
    master(s, "inv", db::Rect{-10, -10, 90, 190});
    REQUIRE(s.read(components("- u7 inv + UNPLACED ;\n- u8 inv ;", 2)).ok());
    CHECK(s.said("Unplaced component \"u7\" will be put at origin."));
    CHECK(use(s, "u7").transform == db::Transform{1, 0, 10, 0, 1, 10});
    CHECK(use(s, "u7").bbox == db::Rect{0, 0, 100, 200});
    CHECK(use(s, "u8").transform == db::Transform{});
  }
}

}  // namespace
}  // namespace def_read_test
