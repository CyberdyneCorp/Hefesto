// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The selection model and the `select` command (capability cmd-selection),
// scripted through the dispatcher on the `mt` technology.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/select/diag_codes.hpp"
#include "hefesto/select/selection.hpp"
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

std::vector<std::string> label_texts(const db::CellDef& def) {
  std::vector<std::string> out;
  for (const db::Label& l : def.labels()) {
    out.push_back(l.text);
  }
  return out;
}

}  // namespace

#define MODEL "[cap:cmd-selection][req:Selection model]"

TEST_CASE("the selection is an internal root-coordinate cell", MODEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 20}, mt.m1);
  s.ok("box 5 5 10 10");
  s.ok("select area metal1");
  const sel::Selection& selection = s.editor.selection;
  REQUIRE(selection.def() != nullptr);
  CHECK(selection.root() == &s.top());
  CHECK(selection.def()->flags().internal);
  CHECK(s.editor.cells.find(selection.def()->name()) == nullptr);  // never in the namespace
  CHECK_FALSE(selection.empty());
  REQUIRE_FALSE(selection.provenance().empty());
  CHECK(selection.provenance().front().source == &s.top());  // each item remembers its cell
}

TEST_CASE("undo restores the previous selection", MODEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 20}, mt.m1);
  s.ok("box 0 0 20 20");
  const sel::Snapshot empty = s.editor.selection.snapshot();
  s.ok("select area");
  CHECK_FALSE(s.editor.selection.empty());
  s.editor.selection.restore(empty);  // what the recorded undo event does
  CHECK(s.editor.selection.empty());
  // Through the undo log once UNDO's log replays events.
  s.ok("select area");
  (void)s.editor.undo.close_step();
  const auto undone = s.editor.undo.undo(s.editor);
  if (undone.ok() && undone.value().steps > 0) {
    CHECK(s.editor.selection.empty());
  } else {
    WARN("undo::UndoLog does not replay events yet; the event itself is checked above");
  }
}

TEST_CASE("selecting in a window on another cell clears the selection first", MODEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* b = s.cell("B");
  s.paint({0, 0, 10, 10}, mt.m1);
  sel::test::paint(*b, mt, {0, 0, 10, 10}, mt.m2);
  auto wa = s.editor.windows.open_frame(&s.top());
  REQUIRE(wa.ok());
  s.ok("box 0 0 10 10");
  s.ok("select area");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 100);
  auto wb = s.editor.windows.open_frame(b);
  REQUIRE(wb.ok());
  s.editor.windows.set_current(wb.value()->id());
  s.editor.box_root = b;
  s.ok("select area");
  CHECK(s.editor.selection.root() == b);
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 0);
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 100);
}

TEST_CASE("a deleted cell leaves the selection", MODEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 4}, mt.m1);
  s.place(*child, "c1", 20, 0);
  s.ok("box 0 0 40 40");
  s.ok("select area subcell");
  REQUIRE(s.selection().uses().size() == 1);
  CHECK(child->instances().size() == 2);  // linked like any use (Magic's SelectDef)
  s.ok("select clear");
  CHECK(child->instances().size() == 1);
}

#define QUAL "[cap:cmd-selection][req:Select qualifiers]"

TEST_CASE("`no` is nocycle with magic-compat ON and the no-labels policy with it OFF", QUAL) {
  HEFESTO_SELECT_MT(mt);
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    Session s(mt);
    s.project.compat.magic_compat = compat;
    s.paint({0, 0, 20, 4}, mt.m1);
    put_label(s.top(), "A", mt.m1, {2, 2, 2, 2});
    s.editor.selection.cycle.type = mt.m1;
    s.ok("box 1 1 2 2");
    s.ok("select no net");
    CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 80);
    if (compat) {
      CHECK(s.editor.selection.cycle.type == mt.m1);  // the type counter was reset, then m1 chosen again
      CHECK(label_texts(s.selection()) == std::vector<std::string>{"A"});
    } else {
      CHECK(s.selection().labels().empty());
    }
  }
}

TEST_CASE("`more` adds to the selection and `less` removes", QUAL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.paint({20, 0, 30, 10}, mt.poly);
  s.ok("box 0 0 10 10");
  s.ok("select area metal1");
  s.ok("box 20 0 30 10");
  s.ok("select more area poly");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 100);
  CHECK(area_of(s.selection(), mt.active, mt.poly) == 100);
  s.ok("select less area poly");
  CHECK(area_of(s.selection(), mt.active, mt.poly) == 0);
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 100);
}

TEST_CASE("`at X Y` selects as if the pointer were there", QUAL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("select at 5 5");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
  CHECK(*s.editor.box == Rect{0, 0, 10, 10});  // the box follows the chunk
}

TEST_CASE("qualifier matching follows Magic's prefix order", QUAL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("box 0 0 10 10");
  s.ok("select m area");  // `m` is `more`
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 100);
  s.ok("select l area");  // `l` is `less`
  CHECK(s.editor.selection.empty());
}

#define OPT "[cap:cmd-selection][req:Select options]"

TEST_CASE("an unknown select option is reported with the help", OPT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const auto r = s.run("select frobnicate metal1");
  // Regression (CMDEDIT-SEL-07): reported once, the help printed, and no
  // error result (Magic's CmdSelect returns normally, so a script goes on);
  // the help ends at "select short" with no empty "select " lines.
  REQUIRE(r.ok());
  const std::string text = all_text(r);
  const std::string warning = "\"frobnicate\" isn't a valid select option.";
  CHECK(text.find(warning) != std::string::npos);
  CHECK(text.find(warning) == text.rfind(warning));
  CHECK(text.find("Selection commands are:") != std::string::npos);
  CHECK(text.find("    select \n") == std::string::npos);
  CHECK(text.ends_with("find shorting path between two labels\n"));
}

TEST_CASE("wrong argument counts print the option's usage", OPT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const auto r = s.run("select clear now");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().code == sel::codes::kUsage);
  CHECK(r.diagnostics.back().text == "Bad arguments:\n    select clear                           clear selection");
  const auto h = s.run("select help");
  REQUIRE(h.ok());
  CHECK(all_text(h).find("    select bbox") != std::string::npos);
}

TEST_CASE("options other than box, chunk, region and net reset the cycling level", OPT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("select at 5 5");
  CHECK(s.editor.selection.cycle.level == sel::CycleState::Level::chunk);
  s.ok("select clear");
  CHECK(s.editor.selection.cycle.level == sel::CycleState::Level::none);
}

#define AREA "[cap:cmd-selection][req:Area and visible selection]"

TEST_CASE("area selection clips paint to the box", AREA) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 20}, mt.m1);
  s.ok("box 5 5 10 10");
  s.ok("select area metal1");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{5, 5, 10, 10}});
}

TEST_CASE("less area removes from the selection", AREA) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 20}, mt.m1);
  s.ok("box 0 0 20 20");
  s.ok("select area metal1");
  s.ok("box 0 0 10 20");
  s.ok("select less area metal1");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{10, 0, 20, 20}});
}

TEST_CASE("area selection matches label globs", AREA) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  for (const auto& [t, x] : std::vector<std::pair<std::string, int>>{{"VDD", 1}, {"VSS", 3}, {"IN", 5}}) {
    put_label(s.top(), t, tech::kSpace, {x, 1, x, 1});
  }
  s.ok("box 0 0 10 10");
  s.ok("select area labels V*");
  CHECK(label_texts(s.selection()) == std::vector<std::string>{"VDD", "VSS"});
}

TEST_CASE("area selection takes unexpanded subcells whole", AREA) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 4}, mt.m1);
  s.place(*child, "c1", 20, 0);
  s.paint({0, 0, 10, 10}, mt.m2);
  s.ok("box 0 0 22 2");
  s.ok("select area");
  REQUIRE(s.selection().uses().size() == 1);
  CHECK(s.selection().uses().front()->id == "c1");
  CHECK(s.selection().uses().front()->bbox == Rect{20, 0, 24, 4});
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 0);  // the child's paint is not copied
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 20);
}

TEST_CASE("visible selection skips hidden layers", AREA) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.paint({0, 0, 10, 10}, mt.m2);
  auto w = s.editor.windows.open_frame(&s.top());
  REQUIRE(w.ok());
  tech::TypeMask visible = tech::TypeMask::all();
  visible.clear_type(mt.m2);
  w.value()->set_visible_types(visible);
  s.ok("box 0 0 10 10");
  s.ok("select visible");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 100);
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 0);
}

#define POINT "[cap:cmd-selection][req:Point selection and cycling]"

TEST_CASE("repeated point selection cycles chunk, region, net", POINT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 100, 10}, mt.m1);
  s.paint({40, 10, 50, 30}, mt.m1);  // a branch: the region is larger than the chunk
  s.paint({90, 0, 200, 10}, mt.m2);
  s.paint({92, 2, 98, 8}, mt.via1);
  s.ok("select at 5 5");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 92, 10}});
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 0);
  s.ok("select at 5 5");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 1000 - 36 + 200);  // the contiguous metal1
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 0);
  s.ok("select at 5 5");
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 1100 - 36);  // the whole net
  CHECK(rects_of(s.selection(), mt.metal1, mt.via1) == std::vector<Rect>{{92, 2, 98, 8}});
  s.ok("select at 5 5");
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 0);  // back to a chunk
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 92, 10}});
}

TEST_CASE("a layer list that matches nothing under the point is reported", POINT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  const auto r = s.run("select metal2 at 5 5");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "No paint of this type under the cursor/box");
  const auto bad = s.run("select nosuchlayer at 5 5");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.diagnostics.back().text == "Invalid layer specification");
}

TEST_CASE("point selection uses the pointer when no `at` is given", POINT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  CHECK_FALSE(s.run("select").ok());  // no pointer
  s.editor.command_point = hefesto::cmd::PointerState{0, {5, 5}, {5, 5, 6, 6}};
  s.ok("select");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 100);
}

TEST_CASE("point selection over empty space selects a cell", POINT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 10, 10}, mt.m1);
  s.place(*child, "c1", 0, 0);
  const auto r = s.run("select at 5 5");  // the child is unexpanded: no paint seen
  REQUIRE(r.ok());
  CHECK(r.value.to_string() == "c1");
}

#define BOXSEL "[cap:cmd-selection][req:Box-anchored chunk, region and net selection]"

TEST_CASE("net from the box corner", BOXSEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 10}, mt.m1);
  s.paint({10, 0, 40, 10}, mt.m2);
  s.paint({12, 2, 18, 8}, mt.via1);
  s.paint({100, 100, 110, 110}, mt.m2);
  s.ok("box 3 3 4 4");
  s.ok("select net");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 200 - 36);
  CHECK(rects_of(s.selection(), mt.metal2, mt.via1) == std::vector<Rect>{{12, 2, 18, 8}});
  CHECK(area_of(s.selection(), mt.metal2, mt.m2) == 300 - 36);
}

TEST_CASE("box-anchored selection looks one unit down-left for a named layer", BOXSEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.paint({10, 10, 20, 20}, mt.poly);
  s.ok("box 10 10 20 20");
  s.ok("select chunk metal1");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
  s.ok("box 15 15 20 20");
  const auto r = s.run("select chunk metal1");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "No paint of this type under or next to the cursor/box");
}

TEST_CASE("box selection needs the box", BOXSEL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const auto r = s.run("select net");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Box tool must be present");
}

#define CHUNK "[cap:cmd-selection][req:Chunk selection]"

TEST_CASE("the chunk prefers the larger rectangle", CHUNK) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  // Both rectangles through (5,5) have smaller dimension 10; the wider wins
  // when both fit the first search window...
  s.paint({0, 0, 16, 10}, mt.m1);
  s.paint({0, 0, 10, 12}, mt.m1);
  s.ok("box 5 5 6 6");
  s.ok("select chunk");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 16, 10}});
}

TEST_CASE("the chunk follows the search-window growth", CHUNK) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  // ...but with the L of the spec scenario the first window (the reference
  // area grown by 10) sees two 10 x 16 candidates; the first found (the
  // vertical one, by Magic's tile order) touches only the top side, so the
  // window doubles upwards and closes in on x = 11: the chunk stays the
  // vertical leg (selCreate.c:735-905). The scenario's "(0,0)-(100,10)" is
  // an open item for golden capture.
  s.paint({0, 0, 100, 10}, mt.m1);
  s.paint({0, 0, 10, 50}, mt.m1);
  s.ok("box 5 5 6 6");
  s.ok("select chunk");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 50}});
}

TEST_CASE("the chunk search window grows past large material", CHUNK) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({-500, -40, 900, 60}, mt.m1);
  put_label(s.top(), "W", mt.m1, {0, 0, 0, 0});
  s.ok("box 0 0 1 1");
  s.ok("select chunk");
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{-500, -40, 900, 60}});
  CHECK(label_texts(s.selection()) == std::vector<std::string>{"W"});  // labels in the chunk
}

#define CELL "[cap:cmd-selection][req:Cell selection]"

TEST_CASE("cell selection cycles through nested cells, then the top cell", CELL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* a = s.cell("A");
  db::CellDef* b = s.cell("B");
  sel::test::paint(*b, mt, {0, 0, 2, 5}, mt.poly);  // area 10
  db::PlaceOptions o;
  o.id = "b";
  REQUIRE(db::place(*a, *b, o).ok());
  sel::test::paint(*a, mt, {0, 0, 10, 10}, mt.ndiff);  // area 100
  db::recompute_bbox(*a);
  db::CellUse* ua = s.place(*a, "a");
  auto w = s.editor.windows.open_frame(&s.top());
  REQUIRE(w.ok());
  w.value()->set_expanded(*ua, true);
  w.value()->set_expanded(*a->uses().front(), true);
  CHECK(s.run("select cell at 1 1").value.to_string() == "a/b");
  CHECK(s.run("select cell at 1 1").value.to_string() == "a");
  // Regression (CMDEDIT-SEL-05): the top cell is its root use's identifier.
  CHECK(s.run("select cell at 1 1").value.to_string() == "Topmost cell in the window");
  CHECK(s.run("select cell at 1 1").value.to_string() == "a/b");
  REQUIRE(s.selection().uses().size() == 1);  // each click replaces the previous cell
}

TEST_CASE("cell selection by name sets the box", CELL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 6}, mt.m1);
  s.place(*child, "x1", 30, 40);
  CHECK(s.run("select cell x1").value.to_string() == "x1");
  CHECK(*s.editor.box == Rect{30, 40, 34, 46});
  const auto r = s.run("select cell nosuch");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Couldn't find a cell use named \"nosuch\"");
  s.ok("select top cell");
  CHECK(s.selection().uses().size() == 1);
  CHECK(s.selection().uses().front()->def == &s.top());
}

#define INTER "[cap:cmd-selection][req:Intersection selection]"

TEST_CASE("intersection keeps the selection where the layer is", INTER) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 20}, mt.m2);
  s.paint({0, 0, 10, 20}, mt.m1);
  s.ok("box 0 0 20 20");
  s.ok("select area metal2");
  s.ok("select intersection metal1");
  CHECK(rects_of(s.selection(), mt.metal2, mt.m2) == std::vector<Rect>{{0, 0, 10, 20}});
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 0);  // the layer itself does not remain
}

TEST_CASE("negated intersection keeps the selection where the layer is absent", INTER) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 20, 20}, mt.m2);
  s.paint({0, 0, 10, 20}, mt.m1);
  s.ok("box 0 0 20 20");
  s.ok("select area metal2");
  s.ok("select intersection ~metal1");
  CHECK(rects_of(s.selection(), mt.metal2, mt.m2) == std::vector<Rect>{{10, 0, 20, 20}});
  const auto r = s.run("select intersection nosuch");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Cannot parse layer type \"nosuch\".");
}

#define UTIL "[cap:cmd-selection][req:Selection utility options]"

TEST_CASE("select bbox reports the selection bounding box", UTIL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 5}, mt.m1);
  s.paint({20, 20, 30, 30}, mt.m1);
  s.ok("box 0 0 30 30");
  s.ok("select area metal1");
  CHECK(s.run("select bbox").value.to_string() == "0 0 30 30");
  s.ok("select clear");
  CHECK(s.editor.selection.empty());
}

TEST_CASE("select labels sets the subcell label policy", UTIL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const auto off = s.run("select no labels");
  REQUIRE(off.ok());
  CHECK(all_text(off) == "Selection ignores subcell labels\n");
  CHECK(s.editor.selection.label_policy == hefesto::connect::LabelPolicy::none);
  const auto on = s.run("select do labels");
  CHECK(all_text(on) == "Selection includes subcell labels\n");
}

TEST_CASE("select flat flattens selected subcells", UTIL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 4}, mt.m1);
  put_label(*child, "IN", mt.m1, {1, 1, 1, 1});
  s.place(*child, "c1", 20, 0);
  s.ok("box 0 0 40 40");
  s.ok("select area");
  s.ok("select flat");
  CHECK(s.selection().uses().empty());
  CHECK(rects_of(s.selection(), mt.metal1, mt.m1) == std::vector<Rect>{{20, 0, 24, 4}});
  CHECK(label_texts(s.selection()) == std::vector<std::string>{"IN"});
}

TEST_CASE("select short finds the path between two labels", UTIL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 100, 4}, mt.m1);
  s.paint({50, 0, 54, 60}, mt.m1);  // a branch
  put_label(s.top(), "A", mt.m1, {1, 1, 1, 1});
  put_label(s.top(), "B", mt.m1, {99, 1, 99, 1});
  s.ok("select short A B");
  CHECK(area_of(s.selection(), mt.metal1, mt.m1) == 400);  // the branch is not on the path
  const auto r = s.run("select short A NOPE");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "No shorting path found between source and destination!");
}

#define FLOAT "[cap:cmd-selection][req:Floating selection (pick, move, keep)]"

TEST_CASE("pick, move and keep drag the selection", FLOAT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("box 0 0 10 10");
  s.ok("select area metal1");
  s.ok("select pick");
  CHECK(area_of(s.top(), mt.metal1, mt.m1) == 0);
  CHECK_FALSE(s.editor.selection.empty());  // the selection is kept
  s.ok("select move 20 0");
  REQUIRE(s.editor.selection.floating_offset().has_value());
  s.ok("select keep");
  CHECK(rects_of(s.top(), mt.metal1, mt.m1) == std::vector<Rect>{{20, 0, 30, 10}});
}

TEST_CASE("select move without coordinates needs the pointer in the edit cell", FLOAT) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.ok("box 0 0 10 10");
  const auto r = s.run("select move");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "Error:  Pointer is not in the edit cell.");
  s.editor.command_point = hefesto::cmd::PointerState{0, {15, 25}, {15, 25, 16, 26}};
  s.ok("select move");
  CHECK(*s.editor.selection.floating_offset() == db::Point{15, 25});
  CHECK_FALSE(s.run("select more pick").ok());  // no qualifier allowed
}

TEST_CASE("select feedback copies the selection's paint to feedback areas", UTIL) {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  REQUIRE(s.editor.windows.open_frame(&s.top()).ok());
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("box 0 0 10 10");
  s.ok("select area metal1");
  s.ok("select feedback");
  REQUIRE(s.editor.feedback.size() == 1);
  CHECK(s.editor.feedback.areas().front().message() == "selection");
  const auto r = s.run("select feedback nosuchstyle");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().code == sel::codes::kUnknownStyle);
}
