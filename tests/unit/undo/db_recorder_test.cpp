// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// undo::DbRecorder: database changes become undo events and replay exactly
// (capability layout-undo "Recorded event kinds", "Undo and redo
// semantics", "Non-recorded operations", "Undoable property changes and
// flatten in place with magic-compat off"). The db primitives report paint
// to the recorder since the M2 scaffold; label, use and property reports
// are wired by EDIT/HIER (m2 tasks 3.7, 6.6), so these tests call the
// recorder for them the way the primitives will.

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "undo_fixture.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace undo = hefesto::undo;
using db::Rect;
using hefesto::undo::test::UndoSession;

namespace {

// Every non-space tile of every plane of `def`: the geometry to compare.
std::map<tech::PlaneId, std::vector<db::TileView>> geometry(const db::CellDef& def) {
  std::map<tech::PlaneId, std::vector<db::TileView>> out;
  for (std::size_t p = 1; p < def.plane_count(); ++p) {
    const auto id = static_cast<tech::PlaneId>(p);
    out[id] = db::test::tiles(def.plane(id));
  }
  return out;
}

std::size_t undo_once(UndoSession& s) {
  auto r = s.editor.undo.undo(s.editor);
  REQUIRE(r.ok());
  return r.value().steps;
}

db::Label label(std::string text, tech::TileType type, Rect rect) {
  db::Label l;
  l.text = std::move(text);
  l.type = type;
  l.rect = rect;
  return l;
}

db::CellUse* place(UndoSession& s, db::CellDef& parent, db::CellDef& child, const std::string& id,
                   const db::Transform& t) {
  db::PlaceOptions options;
  options.id = id;
  options.transform = t;
  auto placed = db::place(parent, child, options);
  REQUIRE(placed.ok());
  s.recorder->use_placed(parent, *placed.value());
  return placed.value();
}

void remove(UndoSession& s, db::CellUse& use) {
  s.recorder->use_deleted(*use.parent, use);
  REQUIRE(db::remove_use(use).ok());
}

}  // namespace

TEST_CASE("paint is undone and redone through the recorder",
          "[cap:layout-undo][req:Recorded event kinds][req:Undo and redo semantics]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
  CHECK(undo_once(s) == 1);
  CHECK(s.rects(mt.metal1).empty());
  CHECK(s.editor.undo.redo(s.editor).steps == 1);
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("undo and redo mark the changed cells modified", "[cap:layout-undo][req:Undo and redo semantics]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  s.top->flags().modified = false;  // as after a save
  CHECK(undo_once(s) == 1);
  CHECK(s.top->flags().modified);  // Q-MODEL-12 (Magic leaves it clear)
  s.top->flags().modified = false;
  CHECK(s.editor.undo.redo(s.editor).steps == 1);
  CHECK(s.top->flags().modified);
}

TEST_CASE("undo restores every plane a contact and an erase changed",
          "[cap:layout-undo][req:Recorded event kinds][req:Undo and redo semantics]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 20, 10);
  s.paint("metal2", 5, 0, 30, 10);
  const auto two = geometry(*s.top);
  s.paint("via1", 8, 2, 12, 8);
  const auto three = geometry(*s.top);
  s.editor.box = Rect{0, 0, 40, 10};
  s.ok("erase metal1");
  const auto four = geometry(*s.top);
  CHECK(four != three);

  CHECK(undo_once(s) == 1);
  CHECK(geometry(*s.top) == three);
  CHECK(undo_once(s) == 1);
  CHECK(geometry(*s.top) == two);
  CHECK(s.editor.undo.redo(s.editor, 2).steps == 2);
  CHECK(geometry(*s.top) == four);
}

TEST_CASE("one command with two rectangles and a label is one step",
          "[cap:layout-undo][req:Event log and command grouping]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  const db::Label lab = label("a", mt.m1, Rect{1, 1, 1, 1});
  // One command (e.g. a Tcl proc): two paints and a label, then the
  // post-command update closes the step.
  REQUIRE(db::paint(*s.top, *mt.tech, Rect{0, 0, 4, 4}, mt.m1).ok());
  REQUIRE(db::paint(*s.top, *mt.tech, Rect{10, 0, 14, 4}, mt.m1).ok());
  s.top->labels().push_back(lab);
  s.recorder->label_put(*s.top, lab);
  REQUIRE(s.editor.undo.close_step());

  CHECK(undo_once(s) == 1);
  CHECK(s.rects(mt.metal1).empty());
  CHECK(s.top->labels().empty());
  CHECK(s.editor.undo.redo(s.editor).steps == 1);
  CHECK(s.rects(mt.metal1).size() == 2);
  CHECK(s.top->labels() == std::vector<db::Label>{lab});
}

TEST_CASE("changes in different cells interleave cell switches",
          "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  db::CellDef* other = s.cell("other");
  REQUIRE(db::paint(*s.top, *mt.tech, Rect{0, 0, 4, 4}, mt.m1).ok());
  REQUIRE(db::paint(*s.top, *mt.tech, Rect{0, 10, 4, 14}, mt.m1).ok());
  REQUIRE(db::paint(*other, *mt.tech, Rect{0, 0, 4, 4}, mt.m2).ok());
  REQUIRE(db::paint(*s.top, *mt.tech, Rect{0, 20, 4, 24}, mt.m1).ok());
  CHECK(s.editor.undo.open_event_count() == 3);  // top, other, top
  REQUIRE(s.editor.undo.close_step());
  CHECK(s.editor.undo.print(0, true) ==
        "(delimiter)\npaint top: 1 tile change(s)\npaint other: 1 tile change(s)\npaint top: 2 tile change(s)\n");
  CHECK(undo_once(s) == 1);
  CHECK(s.rects(mt.metal1).empty());
  CHECK(db::test::rects(other->plane(mt.metal2)).empty());
}

TEST_CASE("label erasure is undone with its full contents", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  db::Label port = label("VDD", mt.m1, Rect{0, 0, 2, 2});
  port.port_directions = 3;
  port.port_index = 4;
  port.sticky = true;
  s.top->labels().push_back(port);
  s.top->labels().push_back(label("x", mt.m1, Rect{5, 5, 5, 5}));
  s.editor.undo.clear();
  // Erase VDD (the label primitives report the full label first).
  s.recorder->label_erased(*s.top, port);
  s.top->labels().erase(s.top->labels().begin());
  REQUIRE(s.editor.undo.close_step());
  CHECK(undo_once(s) == 1);
  REQUIRE(s.top->labels().size() == 2);
  CHECK(s.top->labels().back() == port);  // re-put at the end, as DBPutFontLabel
  CHECK(s.editor.undo.redo(s.editor).steps == 1);
  CHECK(s.top->labels().size() == 1);
  CHECK(s.top->labels().front().text == "x");
}

TEST_CASE("a moved use returns with its identifier", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  db::CellDef* inv = s.cell("inv");
  REQUIRE(db::paint(*inv, *mt.tech, Rect{0, 0, 5, 5}, mt.m1).ok());
  db::recompute_bbox(*inv);
  s.editor.undo.clear();
  place(s, *s.top, *inv, "inv_0", db::Transform{});
  REQUIRE(s.editor.undo.close_step());
  // move: delete the old use, place the new one (Recorded event kinds).
  db::CellUse* use = db::find_use(*s.top, "inv_0");
  remove(s, *use);
  place(s, *s.top, *inv, "inv_0", db::Transform{1, 0, 100, 0, 1, 0});
  REQUIRE(s.editor.undo.close_step());

  CHECK(undo_once(s) == 1);
  use = db::find_use(*s.top, "inv_0");
  REQUIRE(use != nullptr);
  CHECK(use->transform == db::Transform{});
  CHECK(s.top->uses().size() == 1);
  CHECK(s.top->bbox() == Rect{0, 0, 5, 5});  // recomputed after the replay
  CHECK(undo_once(s) == 1);
  CHECK(s.top->uses().empty());
  CHECK(s.editor.undo.redo(s.editor, 2).steps == 2);
  use = db::find_use(*s.top, "inv_0");
  REQUIRE(use != nullptr);
  CHECK(use->transform == db::Transform{1, 0, 100, 0, 1, 0});
}

TEST_CASE("identifier changes and locks are undone", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  db::CellDef* inv = s.cell("inv");
  db::CellUse* use = place(s, *s.top, *inv, "inv_0", db::Transform{});
  s.editor.undo.clear();

  s.recorder->use_id_cleared(*s.top, *use, "inv_0");
  REQUIRE(db::rename_use(*use, "u1").ok());
  s.recorder->use_id_set(*s.top, *use, "u1");
  REQUIRE(s.editor.undo.close_step());
  s.recorder->use_locked(*s.top, *use, true);
  db::set_use_locked(*use, true);
  REQUIRE(s.editor.undo.close_step());

  CHECK(undo_once(s) == 1);
  CHECK_FALSE(use->locked);
  CHECK(undo_once(s) == 1);
  CHECK(use->id == "inv_0");
  CHECK(s.editor.undo.redo(s.editor, 2).steps == 2);
  CHECK(use->id == "u1");
  CHECK(use->locked);
}

TEST_CASE("work done with recording suspended is not undoable",
          "[cap:layout-undo][req:Non-recorded operations]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  {
    // A cell read (any format) runs suspended.
    undo::ScopedSuspend reading(s.editor.undo);
    REQUIRE(db::paint(*s.top, *mt.tech, Rect{0, 0, 10, 10}, mt.m1).ok());
  }
  hefesto::cmd::Invocation load;
  load.name = "load";
  hefesto::editor::post_command(s.editor, load);
  CHECK(undo_once(s) == 0);
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("deleting the recorded cell clears the history", "[cap:layout-undo][req:Non-recorded operations]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  db::CellDef* scratch = s.cell("scratch");
  REQUIRE(db::paint(*scratch, *mt.tech, Rect{0, 0, 10, 10}, mt.m1).ok());
  REQUIRE(s.editor.undo.close_step());
  REQUIRE(s.editor.cells.remove(*scratch).ok());  // reports cell_removed first
  CHECK(s.editor.undo.step_count() == 0);
  CHECK(undo_once(s) == 0);
}

TEST_CASE("events of a cell deleted since are skipped", "[cap:layout-undo][req:Undo and redo semantics]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  db::CellDef* scratch = s.cell("scratch");
  REQUIRE(db::paint(*scratch, *mt.tech, Rect{0, 0, 10, 10}, mt.m1).ok());
  REQUIRE(db::paint(*s.top, *mt.tech, Rect{0, 0, 10, 10}, mt.m1).ok());
  REQUIRE(s.editor.undo.close_step());
  // A delete that did not report itself must not crash the replay.
  {
    const db::ScopedRecorderDetach unreported(s.editor.cells);
    REQUIRE(s.editor.cells.remove(*scratch).ok());
  }
  CHECK(undo_once(s) == 1);
  CHECK(s.rects(mt.metal1).empty());
}

TEST_CASE("property changes are recorded only with magic-compat off",
          "[cap:layout-undo][req:Undoable property changes and flatten in place with magic-compat off]"
          "[req:Native undo events for Hefesto compound operations]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  const auto set = [&](const char* value) {
    const db::PropertyValue* now = s.top->properties().find("x");
    const std::optional<db::PropertyValue> old = now != nullptr ? std::optional{*now} : std::nullopt;
    const std::optional<db::PropertyValue> fresh =
        value != nullptr ? std::optional{db::PropertyValue{std::string{value}}} : std::nullopt;
    db::set_property(*s.top, "x", fresh);
    s.recorder->property_changed(*s.top, "x", old, fresh);  // as db::set_property will (HIER 6.6)
    (void)s.editor.undo.close_step();
  };

  SECTION("magic-compat ON: a plain property change stays non-recorded") {
    set("1");
    CHECK(undo_once(s) == 0);
    REQUIRE(s.top->properties().find("x") != nullptr);
  }

  SECTION("magic-compat OFF: property x is undone and redone") {
    s.project.compat.magic_compat = false;
    set("1");
    s.top->flags().modified = false;
    CHECK(undo_once(s) == 1);
    CHECK(s.top->properties().find("x") == nullptr);
    CHECK(s.top->flags().modified);
    CHECK(s.editor.undo.redo(s.editor).steps == 1);
    REQUIRE(s.top->properties().find("x") != nullptr);
    CHECK(*s.top->properties().find("x") == db::PropertyValue{std::string{"1"}});
    set(nullptr);  // removal
    CHECK(undo_once(s) == 1);
    CHECK(s.top->properties().find("x") != nullptr);
  }

  SECTION("a fixed compat flag is used without a source") {
    s.recorder->set_compat_source(nullptr);
    s.recorder->set_magic_compat(false);
    CHECK_FALSE(s.recorder->magic_compat());
    set("2");
    CHECK(undo_once(s) == 1);
  }
}

TEST_CASE("flatten in place is undone with magic-compat off",
          "[cap:layout-undo][req:Undoable property changes and flatten in place with magic-compat off]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.project.compat.magic_compat = false;
  db::CellDef* leaf = s.cell("leaf");
  REQUIRE(db::paint(*leaf, *mt.tech, Rect{0, 0, 5, 5}, mt.m1).ok());
  db::CellUse* x1 = place(s, *s.top, *leaf, "x1", db::Transform{1, 0, 100, 0, 1, 0});
  s.editor.undo.clear();

  // `flatten -doinplace x1`: the use deletion is reported (db::remove_use,
  // EDIT 3.7) and the copied paint is recorded by the paint primitives.
  s.recorder->use_deleted(*s.top, *x1);
  REQUIRE(db::flatten_in_place(*x1, *mt.tech).ok());
  REQUIRE(s.editor.undo.close_step());
  CHECK(s.top->uses().empty());
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{100, 0, 105, 5}});

  CHECK(undo_once(s) == 1);
  db::CellUse* back = db::find_use(*s.top, "x1");
  REQUIRE(back != nullptr);
  CHECK(back->transform == db::Transform{1, 0, 100, 0, 1, 0});
  CHECK(s.rects(mt.metal1).empty());
  CHECK(s.editor.undo.redo(s.editor).steps == 1);
  CHECK(s.top->uses().empty());
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{100, 0, 105, 5}});
}
