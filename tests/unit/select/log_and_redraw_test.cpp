// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// What selection commands leave for the command log and the display:
// the pointer forms are logged in a pointer-independent form (Magic's
// TxRebuildCommand calls in CmdSelect, CmdMove, CmdCopy, CmdStretch), a
// selection change asks the highlight layer to redraw the old and new
// selection (selRedisplay), and `move origin` keeps the layout in place on
// the screen (WindTranslate).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/view/highlights.hpp"
#include "hefesto/view/window.hpp"
#include "select_fixture.hpp"

namespace db = hefesto::db;
namespace sel = hefesto::select;
namespace view = hefesto::view;
using db::Rect;
using sel::test::Session;

namespace {

/// The logged form of a result as one line ("" when the command is logged
/// as typed).
std::string logged(const hefesto::cmd::CommandResult& r) {
  if (!r.logged_as) {
    return {};
  }
  std::string out = r.logged_as->name;
  for (const std::string& a : r.logged_as->args) {
    out += " " + a;
  }
  return out;
}

/// A window on the edit cell whose pixels are layout units.
view::WindowId unit_window(Session& s) {
  auto w = s.editor.windows.open(&s.top(), view::ScreenRect{0, 0, 499, 499});
  REQUIRE(w.ok());
  w.value()->move_to(Rect{0, 0, 499, 499});
  return w.value()->id();
}

}  // namespace

TEST_CASE("a pointer selection is logged with `at` and the point's root corner",
          "[cap:ui-command-dispatch][req:Command logging][cap:cmd-selection][req:Point selection and cycling]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.editor.command_point = hefesto::cmd::PointerState{0, {5, 6}, {5, 6, 6, 7}};
  const auto r = s.run("select");
  REQUIRE(r.ok());
  CHECK(logged(r) == "select at 5i 6i");
  s.editor.command_point = hefesto::cmd::PointerState{0, {5, 6}, {5, 6, 6, 7}};
  CHECK(logged(s.run("select more metal1")) == "select more metal1 at 5i 6i");
  // Forms that do not use the pointer are logged as typed.
  CHECK(logged(s.run("select at 5 5")).empty());
  s.ok("box 0 0 10 10");
  CHECK(logged(s.run("select box")).empty());
}

TEST_CASE("move, copy and stretch at the pointer are logged as `to` and direction forms",
          "[cap:ui-command-dispatch][req:Command logging][cap:cmd-selection][req:Move command]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.ok("box 0 0 10 10");
  s.ok("select area metal1");
  s.editor.command_point = hefesto::cmd::PointerState{0, {30, 40}, {30, 40, 30, 40}};
  const auto moved = s.run("move");
  REQUIRE(moved.ok());
  CHECK(logged(moved) == "move to 30i 40i");
  CHECK(*s.editor.box == Rect{30, 40, 40, 50});
  CHECK(logged(s.run("move e 5")).empty());

  s.editor.command_point = hefesto::cmd::PointerState{0, {100, 40}, {100, 40, 100, 40}};
  const auto copied = s.run("copy");
  REQUIRE(copied.ok());
  CHECK(logged(copied) == "copy to 100i 40i");

  // stretch: the pointer minus the box corner, along its larger axis.
  s.editor.command_point = hefesto::cmd::PointerState{0, {100, 47}, {100, 47, 100, 47}};
  const auto stretched = s.run("stretch");
  REQUIRE(stretched.ok());
  CHECK(logged(stretched) == "stretch n 7i");
  s.editor.command_point = hefesto::cmd::PointerState{0, {90, 54}, {90, 54, 90, 54}};
  CHECK(logged(s.run("stretch")) == "stretch w 10i");
}

TEST_CASE("a selection change redraws the old and new selection highlights",
          "[cap:cmd-selection][req:Selection model]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const view::WindowId id = unit_window(s);
  s.paint({0, 0, 10, 10}, mt.m1);
  s.paint({50, 50, 60, 60}, mt.m1);
  s.ok("box 0 0 10 10");
  (void)s.editor.highlights.take_frame(id);
  s.ok("select area metal1");
  const view::HighlightFrame* frame = s.editor.highlights.frame(id);
  REQUIRE(frame != nullptr);
  CHECK_FALSE(frame->scene.items.empty());  // the selection outline is drawn

  (void)s.editor.highlights.take_frame(id);
  s.editor.command_point = hefesto::cmd::PointerState{id, {55, 55}, {55, 55, 56, 56}};
  s.ok("select");  // replaces the selection: the old one is erased
  frame = s.editor.highlights.frame(id);
  REQUIRE(frame != nullptr);
  CHECK_FALSE(frame->erase.empty());

  (void)s.editor.highlights.take_frame(id);
  s.editor.undo.close_step();
  REQUIRE(s.editor.undo.undo(s.editor).ok());  // undo redraws as well
  CHECK(s.editor.highlights.has_pending());
}

TEST_CASE("move origin translates every window's view", "[cap:cmd-selection][req:Move origin]") {
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  const view::WindowId id = unit_window(s);
  s.paint({10, 10, 20, 20}, mt.m1);
  s.ok("box 10 10 20 20");
  const view::ViewMapping before = s.editor.windows.find(id)->mapping();
  s.ok("move origin e 10");
  // WindTranslate: WindMove of the surface area moved by (-10, 0).
  const Rect shifted{before.surface.x0 - 10, before.surface.y0, before.surface.x1 - 10, before.surface.y1};
  const view::ViewMapping& after = s.editor.windows.find(id)->mapping();
  CHECK(after.surface == view::ViewMapping::fit(before.screen, shifted).surface);
  // The geometry keeps its screen place, within the pixel WindMove's
  // re-centering of the (slightly larger) surface area may shift it.
  const int drift = after.screen_x(0) - before.screen_x(10);
  CHECK((drift >= -1 && drift <= 1));
}

TEST_CASE("undoing the first selection of a use unlinks the selected copy from its cell",
          "[cap:cmd-selection][req:Selection model]") {
  // Regression: restoring the empty "before" snapshot destroyed the selection
  // cell without unlinking its uses, leaving dangling instance pointers.
  HEFESTO_SELECT_MT(mt);
  Session s(mt);
  db::CellDef* child = s.cell("child");
  sel::test::paint(*child, mt, {0, 0, 4, 4}, mt.m2);
  s.place(*child, "c1");
  s.ok("box 0 0 4 4");
  s.ok("select area subcell");
  REQUIRE(child->instances().size() == 2);  // the layout use and the selected copy
  REQUIRE(s.editor.undo.undo(s.editor).ok());
  CHECK(child->instances().size() == 1);
  CHECK(child->instances().front()->parent == &s.top());
}
