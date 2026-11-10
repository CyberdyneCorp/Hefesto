// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Editing through the selection (capability cmd-selection): the edit-cell
// restriction, delete, copy, move, stretch, rotations, orient and array,
// and the `what` and `identify` commands.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/select/diag_codes.hpp"
#include "select_fixture.hpp"

namespace db = hefesto::db;
namespace sel = hefesto::select;
namespace tech = hefesto::tech;
using db::Rect;
using sel::test::all_text;
using sel::test::area_of;
using sel::test::rects_of;
using sel::test::Session;

namespace {

void put_label(db::CellDef& def, const std::string& text, tech::TileType type, const Rect& r) {
  db::Label l;
  l.text = text;
  l.type = type;
  l.rect = r;
  REQUIRE(db::put_label(def, l).ok());
}

/// Selects metal1 of the edit cell under the box.
void select_box(Session& s, const std::string& box, const std::string& layers = "metal1") {
  s.ok("box " + box);
  s.ok("select area " + layers);
}

}  // namespace

#define RESTRICT "[cap:cmd-selection][req:Edit-cell restriction for selection operations]"

TEST_CASE("delete skips paint outside the edit cell", RESTRICT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 10, 10}, mt.poly);
  db::CellUse* u = s.place(*child, "c1");
  s.paint({0, 0, 10, 10}, mt.m1);
  auto w = s.editor.windows.open_frame(&s.top());
  REQUIRE(w.ok());
  w.value()->set_expanded(*u, true);
  s.ok("box 0 0 10 10");
  s.ok("select area metal1,poly");
  REQUIRE(area_of(s.selection(), mt.active, mt.poly) == 100);  // the child's poly is selected
  const auto r = s.run("delete");
  REQUIRE(r.ok());
  CHECK(area_of(s.top(), mt.metal1, mt.m1) == 0);
  CHECK(area_of(*child, mt.active, mt.poly) == 100);
  CHECK(all_text(r).find("You selected paint outside the edit cell.") != std::string::npos);
}

TEST_CASE("operations need an editable edit cell", RESTRICT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  select_box(s, "0 0 10 10");
  s.top().flags().no_edit = true;
  const auto r = s.run("delete");
  CHECK(all_text(r).find("The current cell is not editable.") != std::string::npos);
  CHECK(area_of(s.top(), mt.metal1, mt.m1) == 100);
}

TEST_CASE("delete removes the selection from the edit cell",
          "[cap:cmd-selection][req:Delete command]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 4}, mt.m2);
  s.place(*child, "c1", 20, 20);
  s.paint({0, 0, 10, 10}, mt.m1);
  put_label(s.top(), "A", mt.m1, {2, 2, 2, 2});
  s.ok("box 0 0 30 30");
  s.ok("select area");
  s.ok("delete");
  CHECK(area_of(s.top(), mt.metal1, mt.m1) == 0);
  CHECK(s.top().labels().empty());
  CHECK(s.top().uses().empty());
  CHECK(s.editor.selection.empty());
}

#define DISP "[cap:cmd-selection][req:Copy and move displacement]"

TEST_CASE("copy east keeps the original and moves the box", DISP) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  select_box(s, "0 0 10 10");
  s.ok("copy e 20");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}, {20, 0, 30, 10}});
  CHECK(*s.editor.box == Rect{20, 0, 30, 10});
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{20, 0, 30, 10}});
}

TEST_CASE("move by dx dy", DISP) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  select_box(s, "0 0 10 10");
  s.ok("move 5 7");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{5, 7, 15, 17}});
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{5, 7, 15, 17}});
  CHECK(*s.editor.box == Rect{5, 7, 15, 17});
}

TEST_CASE("copy and move direction forms", DISP) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.editor.grid = tech::GridScale{2, 1};  // 1 lambda = 2 internal units
  s.paint({0, 0, 10, 10}, mt.m1);
  select_box(s, "0 0 10 10");
  s.ok("move n");  // one lambda
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 2, 10, 12}});
  s.ok("move sw 3");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{-3, -1, 7, 9}});
  s.ok("move nw 1 2");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{-4, 1, 6, 11}});
  s.ok("move to 0 0");  // the box lower-left goes to (0,0)
  CHECK(*s.editor.box == Rect{0, 0, 10, 10});
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
  const auto bad = s.run("copy 5");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.diagnostics.back().text == "Improperly defined copy. . . direction needed.");
  CHECK_FALSE(s.run("copy a b c d").ok());
  s.ok("copy center");  // zero displacement
}

TEST_CASE("copy makes instance identifiers unique", DISP) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 4}, mt.m2);
  s.place(*child, "c1");
  s.ok("box 0 0 4 4");
  s.ok("select area subcell");
  s.ok("copy e 10");
  REQUIRE(s.top().uses().size() == 2);
  CHECK(s.top().uses()[0]->id == "c1");
  CHECK(s.top().uses()[1]->id != "c1");
  CHECK(s.top().uses()[1]->bbox == Rect{10, 0, 14, 4});
}

TEST_CASE("move origin shifts the root cell's contents", "[cap:cmd-selection][req:Move origin]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({10, 10, 20, 20}, mt.m1);
  put_label(s.top(), "A", mt.m1, {12, 12, 12, 12});
  s.ok("box 10 10 20 20");
  s.ok("move origin e 10");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 10, 10, 20}});
  CHECK(s.top().labels().front().rect == Rect{2, 12, 2, 12});
  CHECK(*s.editor.box == Rect{0, 10, 10, 20});  // the box stays on the geometry
}

TEST_CASE("stretch extends the material behind the selection", "[cap:cmd-selection][req:Stretch command]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 30, 4}, mt.m1);
  select_box(s, "20 0 30 4");
  s.ok("stretch e 10");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 40, 4}});
  CHECK_FALSE(s.run("stretch ne 3").ok());  // not Manhattan
}

TEST_CASE("stretch clears the swept path", "[cap:cmd-selection][req:Stretch command]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 4}, mt.m1);
  s.paint({12, 0, 20, 4}, mt.m1);  // in the path
  select_box(s, "0 0 10 4");
  s.ok("stretch e 6");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{6, 0, 20, 4}});
}

#define ROT "[cap:cmd-selection][req:Rotation and flip commands]"

TEST_CASE("clockwise keeps the lower-left corner", ROT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({10, 10, 30, 20}, mt.m1);
  select_box(s, "10 10 30 20");
  s.ok("clockwise");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{10, 10, 20, 30}});
  CHECK(*s.editor.box == Rect{10, 10, 20, 30});
}

TEST_CASE("a negative angle gets 360 added and other angles are refused", ROT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({10, 10, 30, 20}, mt.m1);
  select_box(s, "10 10 30 20");
  s.ok("rotate -90");  // clockwise 270
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{10, 10, 20, 30}});
  const auto r = s.run("clockwise 45");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Rotation angle must be 90, 180, or 270 degrees");
}

TEST_CASE("rotation about the origin", ROT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({10, 10, 30, 20}, mt.m1);
  select_box(s, "10 10 30 20");
  s.ok("clockwise 90 -orig");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{10, -30, 20, -10}});
}

TEST_CASE("flips mirror labels and keep the corner", ROT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({10, 10, 30, 20}, mt.m1);
  db::Label l;
  l.text = "A";
  l.type = mt.m1;
  l.rect = Rect{12, 12, 12, 12};
  l.justification = db::kEast;
  REQUIRE(db::put_label(s.top(), l).ok());
  select_box(s, "10 10 30 20", "metal1,labels");
  s.ok("sideways");
  REQUIRE(s.top().labels().size() == 1);
  CHECK(s.top().labels().front().rect == Rect{28, 12, 28, 12});
  CHECK(s.top().labels().front().justification == db::kWest);
  s.ok("upsidedown");
  CHECK(s.top().labels().front().rect == Rect{28, 18, 28, 18});
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{10, 10, 30, 20}});
}

#define ORIENT "[cap:cmd-selection][req:Orient command]"

TEST_CASE("orient FN and 180v are a horizontal flip", ORIENT) {
  HEFESTO_SELECT_MT(mt);
  for (const std::string name : {"FN", "180v", "fn"}) {
    INFO(name);
    Session s(mt);
    s.paint({10, 10, 30, 20}, mt.m1);
    put_label(s.top(), "A", mt.m1, {12, 12, 12, 12});
    select_box(s, "10 10 30 20", "metal1,labels");
    const auto r = s.run("orient " + name);
    REQUIRE(r.ok());
    CHECK(all_text(r).find("Usage") == std::string::npos);
    CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{10, 10, 30, 20}});
    CHECK(s.top().labels().front().rect == Rect{28, 12, 28, 12});
  }
}

TEST_CASE("an ambiguous orientation prints the usage", ORIENT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.ok("box 0 0 1 1");
  const auto r = s.run("orient 9");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Usage: orient [orientation]");
}

TEST_CASE("orientation is relative to the current one", ORIENT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("inv");
  sel::test::paint(*child, mt, {0, 0, 10, 20}, mt.m1);
  db::PlaceOptions o;
  o.id = "i1";
  o.transform = db::Transform{0, 1, 0, -1, 0, 10};  // clockwise 90
  REQUIRE(db::place(s.top(), *child, o).ok());
  db::recompute_bbox(s.top());
  s.ok("box 0 0 20 10");
  s.ok("select area subcell");
  s.ok("orient 90");
  const db::CellUse* u = db::find_use(s.top(), "i1");
  REQUIRE(u != nullptr);
  CHECK(db::orientation(u->transform) == db::Orientation::S);
  CHECK(u->bbox == Rect{0, 0, 10, 20});  // the lower-left corner is kept
}

#define ARRAY "[cap:cmd-selection][req:Array command]"

TEST_CASE("array a cell", ARRAY) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* inv = s.cell("inv");
  sel::test::paint(*inv, mt, {0, 0, 10, 20}, mt.m1);
  s.place(*inv, "i1");
  s.ok("box 0 0 10 20");
  s.ok("select area subcell");
  s.ok("box 0 0 12 25");
  s.ok("array 3 2");
  const db::CellUse* u = db::find_use(s.top(), "i1");
  REQUIRE(u != nullptr);
  CHECK(u->array == db::ArrayInfo{0, 2, 0, 1, 12, 25});
  CHECK(u->bbox == Rect{0, 0, 34, 45});
}

TEST_CASE("array numbers labels", ARRAY) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  put_label(s.top(), "D0", tech::kSpace, {1, 1, 1, 1});
  s.ok("box 0 0 5 5");
  s.ok("select area labels");
  s.ok("box 0 0 10 10");
  s.ok("array 4 1");
  std::vector<std::pair<std::string, db::Coord>> got;
  for (const db::Label& l : s.top().labels()) {
    got.emplace_back(l.text, l.rect.x0);
  }
  std::sort(got.begin(), got.end());
  CHECK(got == std::vector<std::pair<std::string, db::Coord>>{{"D0", 1}, {"D1", 11}, {"D2", 21}, {"D3", 31}});
}

TEST_CASE("array replicates paint", ARRAY) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 4, 4}, mt.m1);
  select_box(s, "0 0 4 4");
  s.ok("box 0 0 10 10");
  s.ok("array 2 2");
  CHECK(area_of(s.top(), mt.metal1, mt.m1) == 64);
}

TEST_CASE("array -list follows magic-compat", ARRAY) {
  HEFESTO_SELECT_MT(mt);
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(mt);
    s.project.compat.magic_compat = compat;
    db::CellDef* inv = s.cell("inv");
    sel::test::paint(*inv, mt, {0, 0, 4, 4}, mt.m1);
    db::PlaceOptions o;
    o.id = "a1";
    o.array = db::ArrayInfo{0, 2, 0, 1, 10, 10};
    REQUIRE(db::place(s.top(), *inv, o).ok());
    o.id = "a2";
    o.transform = db::translation(100, 0);
    o.array = db::ArrayInfo{0, 3, 0, 0, 10, 10};
    REQUIRE(db::place(s.top(), *inv, o).ok());
    db::recompute_bbox(s.top());
    s.ok("box 0 0 200 200");
    s.ok("select area subcell");
    // Reported in the order Magic's linked list gives (a2 first here, the
    // area selection having taken the newest use first): magic-compat keeps
    // the last use's values, native mode one sublist per use.
    const auto r = s.run("array -list count");
    REQUIRE(r.ok());
    CHECK(r.value.to_string() == (compat ? "0 2 0 1" : "{0 3 0 0} {0 2 0 1}"));
    s.ok("select cell a1");
    CHECK(s.run("array -list count").value.to_string() == (compat ? "0 2 0 1" : "{0 2 0 1}"));
    const auto set = s.run("array width 20");
    REQUIRE(set.ok());
    CHECK(all_text(set) == "Unimplemented function.\n");
    CHECK(db::find_use(s.top(), "a1")->array.xsep == 10);
  }
}

TEST_CASE("array needs the box for its spacing and prints help on bad arguments", ARRAY) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const auto help = s.run("array");
  CHECK(all_text(help).find("count") != std::string::npos);
}

#define WHAT "[cap:cmd-selection][req:What command]"

TEST_CASE("what -list describes paint, labels and subcells", WHAT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* inv = s.cell("inv");
  sel::test::paint(*inv, mt, {0, 0, 4, 4}, mt.m2);
  s.place(*inv, "x1", 50, 50);
  s.paint({0, 0, 10, 10}, mt.m1);
  put_label(s.top(), "A", mt.m1, {2, 2, 2, 2});
  s.ok("box 0 0 60 60");
  s.ok("select area");
  CHECK(s.run("what -list").value.to_string() == "metal1 {{A metal1 {}}} {{x1 inv}}");
  const auto printed = s.run("what");
  const std::string text = all_text(printed);
  CHECK(text.find("Selected mask layers:\n    metal1   ( top )") != std::string::npos);
  CHECK(text.find("\"A\" is attached to metal1 in cell def top") != std::string::npos);
  CHECK(text.find("Selected subcell(s):\n    Instance \"x1\" of cell \"inv\"") != std::string::npos);
  CHECK_FALSE(s.run("what -bogus extra").ok());
}

TEST_CASE("what -listall names the cells holding each layer", WHAT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("box 0 0 10 10");
  s.ok("select area metal1");
  CHECK(s.run("what -listall").value.to_string() == "{{metal1 top}} {} {}");
}

#define ID "[cap:cmd-selection][req:Identify command]"

TEST_CASE("identify renames the selected instance", ID) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* inv = s.cell("inv");
  sel::test::paint(*inv, mt, {0, 0, 4, 4}, mt.m2);
  s.place(*inv, "inv_0");
  s.place(*inv, "buf1", 20, 0);
  s.ok("box 0 0 4 4");
  s.ok("select area subcell");
  const auto dup = s.run("identify buf1");
  REQUIRE_FALSE(dup.ok());
  CHECK(dup.diagnostics.back().code == sel::codes::kIdNotUnique);
  CHECK(db::find_use(s.top(), "inv_0") != nullptr);
  s.ok("id buf2");
  CHECK(db::find_use(s.top(), "buf2") != nullptr);
  CHECK(s.selection().uses().front()->id == "buf2");
  CHECK(s.top().flags().modified);
  CHECK(s.run("identify a/b").diagnostics.back().code == sel::codes::kIdIllegal);
  s.ok("select clear");
  CHECK(s.run("identify x").diagnostics.back().text == "There isn't a selected subcell;  can't change ids.");
}
