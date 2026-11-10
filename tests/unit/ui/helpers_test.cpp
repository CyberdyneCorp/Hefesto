// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The tools.tcl helper commands as native commands (capability
// ui-box-stack-and-measure): box and edit stacks, redraw suspension,
// rulers, measurement, layer keys and macro auto-increment.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/ui/bindings.hpp"
#include "hefesto/ui/macros.hpp"
#include "ui_fixture.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace ui = hefesto::ui;
using hefesto::test::UiSession;

namespace {

void open_window(UiSession& s) {
  auto* def = s.editor.cells.create("top", s.editor.technology->plane_count()).value();
  REQUIRE(s.editor.windows.open(def, hefesto::view::ScreenRect{0, 0, 99, 99}).ok());
}

}  // namespace

// --- Box stack -----------------------------------------------------------------------

TEST_CASE("pushbox records the box in internal units", "[cap:ui-box-stack-and-measure][req:Box stack push]") {
  UiSession s;
  // "Push records internal units": the fake box reports microns (100 units
  // per micron) unless units are internal.
  s.box_scale = 100;
  s.editor.units = cmd::UnitsState{cmd::InputUnit::microns, false};
  s.editor.box = db::Rect{0, 0, 100, 200};
  const auto pushed = s.run("pushbox");
  REQUIRE(pushed.ok());
  CHECK(pushed.value.is_none());
  REQUIRE(s.editor.box_stack.records.size() == 1);
  CHECK(s.editor.box_stack.records[0] == "0 0 100 200");
  CHECK(s.editor.units.unit == cmd::InputUnit::microns);
  CHECK(s.editor.box == db::Rect{0, 0, 100, 200});
  // With values: pushed verbatim.
  s.run(R"(pushbox "1 2 3 4")");
  CHECK(s.editor.box_stack.records.back() == "1 2 3 4");
}

TEST_CASE("popbox and peekbox apply the top record", "[cap:ui-box-stack-and-measure][req:Box stack pop and peek]") {
  UiSession s;
  // "Empty stack" and the never-created stack.
  CHECK(UiSession::first_error(s.run("popbox")) == "UI-STACK-001: No stack");
  CHECK(s.run("popbox").raises);
  s.editor.box = db::Rect{100, 100, 101, 101};
  s.run("pushbox; popbox");
  CHECK(UiSession::first_error(s.run("popbox")) == "UI-STACK-001: Empty stack");
  // "Pop size only".
  s.run(R"(pushbox "10 10 30 50")");
  const auto popped = s.run("popbox size");
  REQUIRE(popped.ok());
  CHECK(popped.value.to_string() == "10 10 30 50");
  CHECK(s.editor.box == db::Rect{100, 100, 120, 140});
  CHECK(s.editor.box_stack.empty());
  // Position, values and an unknown type (record still removed).
  s.run(R"(pushbox "7 8 9 9")");
  s.run("popbox position");
  CHECK(s.editor.box == db::Rect{7, 8, 27, 48});
  s.run(R"(pushbox "1 1 2 2"; popbox other)");
  CHECK(s.editor.box == db::Rect{7, 8, 27, 48});
  CHECK(s.editor.box_stack.empty());
  // "Peek leaves the stack unchanged".
  s.run(R"(pushbox "0 0 5 5"; pushbox "1 1 2 2")");
  const auto peeked = s.run("peekbox");
  CHECK(peeked.value.to_string() == "1 1 2 2");
  CHECK(s.editor.box == db::Rect{1, 1, 2, 2});
  CHECK(s.editor.box_stack.records == std::vector<std::string>{"0 0 5 5", "1 1 2 2"});
  s.editor.box_stack.records.clear();
  CHECK(UiSession::first_error(s.run("peekbox")) == "UI-STACK-001: No stack");
}

// --- Edit stack -----------------------------------------------------------------------

TEST_CASE("pushstack loads a cell and remembers the view and root",
          "[cap:ui-box-stack-and-measure][req:Edit stack push]") {
  UiSession s;
  // "Generator draws inside a new cell".
  REQUIRE(s.run("pushstack foo").ok());
  CHECK(s.calls_of("load") == std::vector<std::string>{"load foo"});
  CHECK(s.editor.edit_stack == std::vector<std::string>{"0 0 1000 1000", "top"});
  // Without a name: select the cell under the box when nothing is selected.
  s.editor.box = db::Rect{0, 0, 4, 4};
  s.self_cell = "inv";
  s.run("pushstack");
  CHECK(s.calls_of("select") == std::vector<std::string>{"select cell"});
  CHECK(s.calls_of("load").back() == "load inv");
  CHECK(s.editor.box == db::Rect{0, 0, 4, 4});
  s.self_cell.clear();
  CHECK(UiSession::first_error(s.run("pushstack")) == "UI-STACK-001: No cell to push!");
  CHECK(UiSession::first_error(s.run(R"(pushstack "a b")")) == "UI-STACK-001: Too many cells selected!");
}

TEST_CASE("popstack returns to the saved cell and view", "[cap:ui-box-stack-and-measure][req:Edit stack pop and clear]") {
  UiSession s;
  CHECK(UiSession::first_error(s.run("popstack")) == "UI-STACK-001: No subcell stack!");
  // "Nested push/pop".
  s.run("pushstack a");
  s.view_rect = "5 5 50 50";
  s.window_cell = "a";
  s.run("pushstack b");
  s.calls.clear();
  REQUIRE(s.run("popstack").ok());
  CHECK(s.calls == std::vector<std::string>{"load a", "view 5 5 50 50"});
  REQUIRE(s.run("popstack").ok());
  CHECK(s.calls_of("load").back() == "load top");
  CHECK(s.calls_of("view").back() == "view 0 0 1000 1000");
  CHECK(s.editor.edit_stack.empty());
  s.run("pushstack a; clearstack");
  CHECK(s.editor.edit_stack.empty());
}

// --- Nested redraw suspension ------------------------------------------------------------

TEST_CASE("suspendall and resumeall nest", "[cap:ui-box-stack-and-measure][req:Nested redraw suspension]") {
  UiSession s;
  // "Batch mode": no window system, no effect, no error.
  CHECK(s.run("suspendall").ok());
  CHECK(s.run("resumeall; resumeall").ok());
  CHECK(s.editor.suspend_depth == 0);
  s.editor.styles.emplace();
  open_window(s);
  // "Unmatched resumeall".
  const auto unmatched = s.run("resumeall");
  CHECK(unmatched.raises);
  CHECK(UiSession::first_error(unmatched) == "UI-SUSPEND-001: resume called without suspend");
  // "Two-level nesting".
  s.run("suspendall; suspendall; resumeall");
  CHECK(s.editor.windows.display_suspended());
  s.run("resumeall");
  CHECK_FALSE(s.editor.windows.display_suspended());
}

// --- Ruler annotation ---------------------------------------------------------------------

TEST_CASE("ruler draws a dimension marker inside the box", "[cap:ui-box-stack-and-measure][req:Ruler annotation]") {
  UiSession s;
  // "First horizontal ruler with text".
  s.editor.box = db::Rect{0, 0, 100, 20};
  REQUIRE(s.run("ruler hello").ok());
  CHECK(s.calls_of("element") == std::vector<std::string>{
                                     "element add line l1_0 black 0 0 0 20",
                                     "element add line l4_0 black 100 0 100 20",
                                     "element add line l2_0 black 0 10 46 10",
                                     "element add line l3_0 black 54 10 100 10",
                                     "element add text t_0 black 50 10 hello",
                                     "element configure l2_0 flags arrowleft",
                                     "element configure l3_0 flags arrowright",
                                 });
  // The counter goes on; a vertical ruler without text.
  s.calls.clear();
  s.editor.box = db::Rect{0, 0, 4, 10};
  s.run("ruler");
  CHECK(s.calls_of("element") == std::vector<std::string>{
                                     "element add line l1_1 black 0 0 4 0",
                                     "element add line l4_1 black 0 10 4 10",
                                     "element add line l2_1 black 2 0 2 5",
                                     "element add line l3_1 black 2 5 2 10",
                                     "element configure l2_1 flags arrowbottom",
                                     "element configure l3_1 flags arrowtop",
                                 });
  // A gap meeting the box edge moves 2 units past it.
  s.calls.clear();
  s.editor.box = db::Rect{0, 0, 8, 1};
  s.run("ruler x horizontal");
  CHECK(s.calls_of("element")[2] == "element add line l2_2 black 0 0 -2 0");
  CHECK(s.calls_of("element")[3] == "element add line l3_2 black 10 0 8 0");
  CHECK(s.calls_of("element")[5] == "element configure l2_2 flags arrowright");
  CHECK(s.calls_of("element")[6] == "element configure l3_2 flags arrowleft");
}

TEST_CASE("ruler reads the box in internal units under lambda units",
          "[cap:ui-box-stack-and-measure][req:Ruler annotation]") {
  UiSession s;
  // "Ruler under lambda units": the fake box reports lambda (2 units each)
  // unless units are internal.
  s.box_scale = 2;
  s.editor.units = cmd::UnitsState{cmd::InputUnit::lambda, false};
  s.editor.box = db::Rect{0, 0, 100, 20};
  s.run("ruler hello");
  CHECK(s.calls_of("element")[2] == "element add line l2_0 black 0 10 46 10");
  CHECK(s.editor.units.unit == cmd::InputUnit::lambda);
}

// --- Measure and unmeasure ------------------------------------------------------------------

TEST_CASE("measure labels the ruler in microns", "[cap:ui-box-stack-and-measure][req:Measure and unmeasure]") {
  UiSession s;
  // The built-in technology has no CIF output style: 0.01 um per unit.
  // "Measure text" and "Measure under micron units".
  s.editor.box = db::Rect{0, 0, 250, 10};
  s.run("measure");
  CHECK(s.calls_of("element")[4] == "element add text t_0 black 125 5 2.5 um");
  s.calls.clear();
  s.box_scale = 100;
  s.editor.units = cmd::UnitsState{cmd::InputUnit::microns, false};
  s.run("measure");
  CHECK(s.calls_of("element")[4] == "element add text t_1 black 125 5 2.5 um");
  s.calls.clear();
  s.run("measure vertical");
  CHECK(s.calls_of("element")[4] == "element add text t_2 black 125 5 0.1 um");
}

TEST_CASE("unmeasure deletes whole rulers inside the box", "[cap:ui-box-stack-and-measure][req:Measure and unmeasure]") {
  UiSession s;
  // "Unmeasure removes the whole ruler".
  s.inbox = {"l2_3", "t_3", "l1_3", "box7", "t_x"};
  REQUIRE(s.run("unmeasure").ok());
  CHECK(s.calls_of("element") == std::vector<std::string>{"element inbox", "element delete t_3", "element delete l1_3",
                                                          "element delete l2_3", "element delete l3_3",
                                                          "element delete l4_3"});
}

// --- Layer key annotation ----------------------------------------------------------------------

TEST_CASE("genkey paints a key square and labels it", "[cap:ui-box-stack-and-measure][req:Layer key annotation]") {
  UiSession s;
  // "First key".
  s.editor.box = db::Rect{0, 0, 1, 1};
  REQUIRE(s.run("genkey poly").ok());
  CHECK(s.calls_of("paint") == std::vector<std::string>{"paint poly"});
  CHECK(s.calls_of("element") ==
        std::vector<std::string>{"element add text key0 white 6 2 poly", "element configure key0 flags east"});
  CHECK(s.editor.box == db::Rect{4, 0, 8, 4});
  s.calls.clear();
  s.run("genkey m1 2");
  CHECK(s.calls_of("element")[0] == "element add text key1 white 7 1 m1");
}

// --- Macro auto-increment ------------------------------------------------------------------------

TEST_CASE("autoincr renumbers the repeat macro", "[cap:ui-box-stack-and-measure][req:Macro auto-increment]") {
  UiSession s;
  ui::install_defaults(s.editor);
  // "Label renumbering": `+` runs `autoincr 1`.
  s.editor.macros.set("box", ui::Key{'.', 0}, ui::Macro{"getcell A12", {}, false});
  const ui::Macro* plus = s.editor.macros.find("box", ui::Key{'+', 0});
  REQUIRE(plus != nullptr);
  REQUIRE(s.run(plus->text).ok());
  CHECK(s.calls_of("getcell") == std::vector<std::string>{"getcell A13"});
  CHECK(s.editor.macros.find("box", ui::Key{'.', 0})->text == "getcell A13");
  s.run("autoincr -5");
  CHECK(s.calls_of("getcell").back() == "getcell A8");
  // No number: nothing happens.
  s.editor.macros.set("box", ui::Key{'.', 0}, ui::Macro{"getcell x", {}, false});
  s.calls.clear();
  s.run("autoincr");
  CHECK(s.calls.empty());
  CHECK(s.run("autoincr two").raises);
}
