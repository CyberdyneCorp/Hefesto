// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Undo through a whole session (M2 integration, tasks.md 11.1): every
// EditorState installs an undo::DbRecorder on its cells, the dispatcher's
// post-command update closes each command's step, and the operations of
// layout-undo "Non-recorded operations" stay out of the history (cell reads,
// cell rename and delete).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hier_session.hpp"
#include "hefesto/undo/commands.hpp"
#include "hefesto/undo/db_recorder.hpp"

namespace hefesto::cell::test {
namespace {

constexpr const char* kLeaf = "magic\ntech tiny\ntimestamp 3\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n";

struct UndoSession : HierSession {
  explicit UndoSession(bool magic_compat = true) : HierSession(magic_compat) {
    REQUIRE(undo::register_undo_commands(registry));
  }

  [[nodiscard]] tech::TileType metal1_at(const db::CellDef& def, db::Point p) const {
    return def.plane(*editor.technology->home_plane(type("metal1"))).type_at(p);
  }
};

#define NON_RECORDED "[cap:layout-undo][req:Non-recorded operations]"

TEST_CASE("a session records its edits without any setup", "[cap:layout-undo][req:Event log and command grouping]") {
  UndoSession s;
  REQUIRE(s.editor.cells.recorder() == s.editor.undo_recorder.get());
  s.ok("box 0 0 10 10");
  s.ok("paint metal1");
  CHECK(s.metal1_at(*s.top, {5, 5}) == s.type("metal1"));
  s.ok("undo");
  CHECK(s.metal1_at(*s.top, {5, 5}) == tech::kSpace);
  s.ok("redo");
  CHECK(s.metal1_at(*s.top, {5, 5}) == s.type("metal1"));
}

TEST_CASE("reading a cell is not undoable", NON_RECORDED) {
  UndoSession s;
  s.write("/work/leaf.mag", kLeaf);
  s.ok("load leaf");
  const db::CellDef* leaf = s.editor.cells.find("leaf");
  REQUIRE(leaf != nullptr);
  CHECK(s.metal1_at(*leaf, {1, 1}) == s.type("metal1"));
  CHECK(s.editor.undo.step_count() <= 1);  // at most the edit-cell change
  (void)s.run("undo");
  CHECK(s.metal1_at(*leaf, {1, 1}) == s.type("metal1"));
}

TEST_CASE("deleting a recorded cell clears the history", NON_RECORDED) {
  UndoSession s;
  db::CellDef* scratch = s.cell("scratch");
  REQUIRE(db::paint(*scratch, *s.editor.technology, db::Rect{0, 0, 4, 4}, s.type("metal1")).ok());
  REQUIRE(s.editor.undo.close_step());
  REQUIRE(s.editor.cells.remove(*scratch).ok());
  CHECK(s.editor.undo.step_count() == 0);
}

TEST_CASE("renaming a cell records nothing", NON_RECORDED) {
  UndoSession s;
  db::CellDef* other = s.cell("other");
  REQUIRE(s.editor.cells.rename(*other, "renamed", false).ok());
  CHECK_FALSE(s.editor.undo.close_step());  // the step stayed empty
}

#define COMPAT_OFF "[cap:layout-undo][req:Undoable property changes and flatten in place with magic-compat off]"

TEST_CASE("with magic-compat off a property command is undone", COMPAT_OFF) {
  UndoSession s(false);
  s.ok("property color red");
  REQUIRE(s.top->properties().find("color") != nullptr);
  s.ok("undo");
  CHECK(s.top->properties().find("color") == nullptr);

  UndoSession compat;  // magic-compat ON: not recorded, nothing to undo
  compat.ok("property color red");
  (void)compat.run("undo");
  CHECK(compat.top->properties().find("color") != nullptr);
}

TEST_CASE("with magic-compat off flatten -doinplace is undone", COMPAT_OFF) {
  UndoSession s(false);
  db::CellDef* leaf = s.cell("leaf");
  s.paint(*leaf, "metal1", db::Rect{0, 0, 5, 5});
  s.place(*s.top, *leaf, db::translation(100, 0), "x1");
  REQUIRE(s.editor.undo.close_step());
  s.ok("flatten -doinplace x1");
  REQUIRE(s.top->uses().empty());
  s.ok("undo");
  REQUIRE(s.top->uses().size() == 1);
  CHECK(s.top->uses().front()->id == "x1");
  CHECK(s.metal1_at(*s.top, {102, 2}) == tech::kSpace);
}

}  // namespace
}  // namespace hefesto::cell::test
