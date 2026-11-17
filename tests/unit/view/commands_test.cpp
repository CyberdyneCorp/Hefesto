// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Core view commands through the dispatcher (capability ui-windows-and-views).

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view/commands.hpp"
#include "hefesto/view/diag_codes.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace view = hefesto::view;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  Session() {
    REQUIRE(view::register_view_commands(registry));
    REQUIRE(view::register_view_and_zoom(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    cmd::Invocation inv;
    inv.name = std::move(name);
    inv.args = std::move(args);
    return dispatcher->execute(inv);
  }

  db::CellDef* cell(const std::string& name, const db::Rect& bbox) {
    db::CellDef* def = editor.cells.create(name, editor.technology->plane_count()).value();
    def->set_bboxes(bbox, bbox);
    return def;
  }

  view::LayoutWindow& window(db::CellDef* root) {
    auto w = editor.windows.open(root, {0, 0, 499, 399});
    REQUIRE(w.ok());
    return *w.value();
  }
};

std::string text(const cmd::CommandResult& r) {
  return r.value.to_string();
}

std::string error(const cmd::CommandResult& r) {
  return r.diagnostics.empty() ? std::string() : r.diagnostics.back().text;
}

}  // namespace

TEST_CASE("view commands are registered, view and zoom apart", "[cap:ui-windows-and-views][req:view, xview and zoom]") {
  cmd::CommandRegistry registry;
  CHECK(view::register_view_commands(registry));
  CHECK(registry.find("expand") != nullptr);
  CHECK(registry.find("crosshair") != nullptr);
  CHECK(registry.find("zoom") == nullptr);  // the shell's until design.md D5 is done
  CHECK(view::register_view_and_zoom(registry));
  CHECK(registry.find("zoom") != nullptr);
}

TEST_CASE("view frames, reports and sets the view", "[cap:ui-windows-and-views][req:view, xview and zoom]") {
  Session s;
  auto& w = s.window(s.cell("inv", {0, 0, 100, 50}));
  CHECK(text(s.run("view", {"get"})) == "-11 -24 112 75");  // Tcl list
  REQUIRE(s.run("view").ok());
  CHECK(w.mapping().surface == view::ViewMapping::fit(w.mapping().screen, {-10, -5, 111, 55}).surface);
  CHECK(text(s.run("view", {"bbox"})) == "0 0 100 50");
  REQUIRE(s.run("view", {"0", "0", "10", "20"}).ok());
  CHECK(w.mapping().surface == view::ViewMapping::fit(w.mapping().screen, {0, 0, 10, 20}).surface);
  REQUIRE(s.run("view", {"0 0 10 20"}).ok());  // one argument, parsed x y x y (Q-UI-18)
  CHECK(error(s.run("view", {"a", "b"})) == "Usage: view [get|bbox|llx lly urx ury]");
}

TEST_CASE("zoom rescales about the centre and checks its factor", "[cap:ui-windows-and-views][req:view, xview and zoom]") {
  Session s;
  auto& w = s.window(s.cell("inv", {0, 0, 100, 50}));
  REQUIRE(s.run("zoom", {"2"}).ok());
  CHECK(w.mapping().surface == view::ViewMapping::fit(w.mapping().screen, {-72, -73, 174, 125}).surface);
  for (const char* bad : {"0", "20", "25", "-1"}) {
    const auto r = s.run("zoom", {bad});
    REQUIRE_FALSE(r.ok());
    CHECK(r.diagnostics.back().code == view::codes::kZoom);
    CHECK(error(r) == "zoom factor must be between 0 and 20.");
  }
  CHECK(error(s.run("zoom")) == "Usage: zoom factor");
}

TEST_CASE("view commands need a window", "[cap:ui-windows-and-views][req:view, xview and zoom]") {
  Session s;
  const auto r = s.run("zoom", {"2"});
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().code == view::codes::kNoWindow);
}

TEST_CASE("scroll pans and validates its units", "[cap:ui-windows-and-views][req:scroll]") {
  Session s;
  auto& w = s.window(s.cell("inv", {0, 0, 100, 50}));
  const db::Rect before = w.mapping().surface;
  REQUIRE(s.run("scroll", {"r", ".1", "w"}).ok());
  CHECK(w.mapping().surface.x0 > before.x0);
  const db::Rect after_r = w.mapping().surface;
  REQUIRE(s.run("scroll", {"n"}).ok());
  CHECK(w.mapping().surface.y0 > after_r.y0);
  const db::Rect after_n = w.mapping().surface;
  REQUIRE(s.run("scroll", {"e", "10"}).ok());
  CHECK(w.mapping().surface.x0 == after_n.x0 + 10);
  CHECK(error(s.run("scroll", {"x", "1", "w"})).find("not a valid direction") != std::string::npos);
  CHECK(error(s.run("scroll", {"n", "1", "q"})).ends_with("'units' must be one of 'w' (window) or 'l' (layout);"));
  CHECK(error(s.run("scroll", {"n", "abc", "w"})).ends_with("'amount' is a fractional value."));
}

TEST_CASE("center recentres; findbox centres and zooms on the box", "[cap:ui-windows-and-views][req:center and findbox]") {
  Session s;
  db::CellDef* inv = s.cell("inv", {0, 0, 100, 50});
  auto& w = s.window(inv);
  REQUIRE(s.run("center", {"50", "50"}).ok());
  const db::Rect c = w.mapping().surface;
  CHECK(std::abs((c.x0 + c.x1) / 2 - 50) <= 1);
  CHECK(error(s.run("center", {"1.5", "2"})) == "Coordinates must be integer values");
  CHECK(error(s.run("center", {"h", "x"})) == "Must specify a fractional value.");
  REQUIRE(s.run("center", {"h", "0.25"}).ok());

  CHECK(error(s.run("findbox")) == "Put the box in a window first.");
  s.editor.edit_cell = inv;
  s.editor.box = db::Rect{0, 0, 10, 10};
  REQUIRE(s.run("findbox", {"zoom"}).ok());
  CHECK(w.mapping().surface == view::ViewMapping::fit(w.mapping().screen, {-2, -2, 12, 12}).surface);
  REQUIRE(s.run("findbox").ok());
  s.editor.edit_cell = s.cell("other", {0, 0, 1, 1});
  CHECK(error(s.run("findbox")) == "The box is not in the same coordinate system as the window.");
}

TEST_CASE("expand acts per window under the box", "[cap:ui-windows-and-views][req:Per-window expansion state]") {
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 100, 100});
  db::CellDef* inv = s.cell("inv", {0, 0, 10, 10});
  auto use = std::make_unique<db::CellUse>();
  use->id = "u1";
  use->def = inv;
  use->parent = top;
  use->bbox = {0, 0, 10, 10};
  const db::CellUse& u1 = *use;
  top->uses().push_back(std::move(use));
  auto& w1 = s.window(top);
  auto& w0 = s.window(top);  // current
  s.editor.edit_cell = top;
  s.editor.box = db::Rect{0, 0, 5, 5};
  REQUIRE(s.run("expand").ok());
  CHECK(w0.is_expanded(u1));
  CHECK_FALSE(w1.is_expanded(u1));
  REQUIRE(s.run("expand", {"t"}).ok());  // any prefix of toggle (Q-UI-19)
  CHECK_FALSE(w0.is_expanded(u1));
  REQUIRE(s.run("expand").ok());
  REQUIRE(s.run("unexpand").ok());  // surround: the box does not surround u1
  CHECK(w0.is_expanded(u1));
  s.editor.box = db::Rect{-1, -1, 20, 20};
  REQUIRE(s.run("unexpand").ok());
  CHECK_FALSE(w0.is_expanded(u1));
  REQUIRE(s.run("expand").ok());
  REQUIRE(s.run("xview").ok());
  CHECK_FALSE(w0.is_expanded(u1));
}

TEST_CASE("regression: expand over part of a cell searches its use index",
          "[cap:gds-write][req:Structure ordering]") {
  // DBExpandAll searches the uses under the box through the index, and a
  // partial search of 10 or more unbinned uses builds the bins; the writers
  // then emit sibling structures in the bin order (MASK-UO-002..004/007/008).
  // `expand` looped over the use list instead, so the index stayed the
  // newest-first list and `gds write` put the structures in another order.
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 1200, 100});
  db::CellDef* inv = s.cell("inv", {0, 0, 10, 10});
  for (db::Coord i = 0; i < 12; ++i) {
    db::PlaceOptions o;
    o.transform = db::translation(100 * i, 0);
    REQUIRE(db::place(*top, *inv, o).ok());
  }
  (void)s.window(top);
  s.editor.edit_cell = top;
  s.editor.box = db::Rect{0, 0, 5, 5};
  REQUIRE_FALSE(top->use_index().state_for_test().binned);
  REQUIRE(s.run("expand").ok());
  CHECK(top->use_index().state_for_test().binned);
}

TEST_CASE("see hides layers, labels and subcells per window", "[cap:ui-windows-and-views][req:Layer visibility (see)]") {
  hefesto::tech::TechnologyBuilder b;
  b.set_name("t");
  REQUIRE(b.add_plane("metal1").ok());
  const auto m1 = b.add_type("metal1", "metal1,m1").value();
  Session s;
  s.editor.technology = b.finalize().value();
  db::CellDef* top = s.cell("top", {0, 0, 10, 10});
  auto& other = s.window(top);
  auto& w = s.window(top);
  REQUIRE(s.run("see", {"no", "metal1"}).ok());
  CHECK_FALSE(w.visible_types().test(m1));
  CHECK(other.visible_types().test(m1));
  CHECK(w.visible_types().test(hefesto::tech::kSpace));
  REQUIRE(s.run("see", {"allSame"}).ok());
  CHECK(w.all_same());
  REQUIRE(s.run("see", {"no", "allSame"}).ok());
  CHECK_FALSE(w.all_same());
  REQUIRE(s.run("see", {"no", "labels"}).ok());
  CHECK_FALSE(w.labels_visible());
  CHECK(error(s.run("see", {"no", "a", "b", "c"})) == "Usage: see [no] layers|allSame");
  REQUIRE(s.run("see").ok());
  CHECK(w.visible_types().test(m1));
  CHECK(w.labels_visible());
}

TEST_CASE("grid command forms", "[cap:ui-windows-and-views][req:Grid display and grid command]") {
  Session s;
  auto& w = s.window(s.cell("top", {0, 0, 10, 10}));
  REQUIRE(s.run("grid", {"10", "20", "5", "0"}).ok());
  CHECK(w.grid().visible);
  CHECK(text(s.run("grid", {"box"})) == "5 0 15 20");  // Tcl result: "%d %d %d %d"
  CHECK(text(s.run("grid", {"state"})) == "1");  // Tcl_NewBooleanObj
  REQUIRE(s.run("grid").ok());
  CHECK(text(s.run("grid", {"state"})) == "0");
  REQUIRE(s.run("grid", {"on"}).ok());
  REQUIRE(s.run("grid", {"0"}).ok());
  CHECK_FALSE(w.grid().visible);
  CHECK(error(s.run("grid", {"-1"})) == "Grid spacing must be greater than zero.");
  CHECK(error(s.run("grid", {"1", "2", "3"})).starts_with("Usage: grid"));
  // Out-of-range multiples are rejected without being stored (Q-UI-21).
  CHECK_FALSE(s.run("grid", {"multiple", "300"}).ok());
  CHECK_FALSE(w.grid().multiple.has_value());
  REQUIRE(s.run("grid", {"multiple", "4"}).ok());
  CHECK(text(s.run("grid", {"multiple"})) == "4");
}

TEST_CASE("crosshair is shown in every window on the same root",
          "[cap:ui-windows-and-views][req:Crosshair][cap:cmd-misc-edit][req:Crosshair command]") {
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 10, 10});
  db::CellDef* inv = s.cell("inv", {0, 0, 10, 10});
  auto& w2 = s.window(inv);
  auto& w1 = s.window(top);
  auto& w0 = s.window(top);
  REQUIRE(s.run("crosshair", {"100", "100"}).ok());
  CHECK(w0.crosshair() == db::Point{100, 100});
  CHECK(w1.crosshair() == db::Point{100, 100});
  CHECK_FALSE(w2.crosshair().has_value());
  REQUIRE(s.run("crosshair", {"off"}).ok());
  CHECK_FALSE(w0.crosshair().has_value());
  CHECK(error(s.run("crosshair", {"1"})) == "Usage: crosshair off|x y");
}

TEST_CASE("window chrome defaults affect only new windows", "[cap:ui-windows-and-views][req:Window chrome flags]") {
  Session s;
  auto& old = s.window(s.cell("top", {0, 0, 10, 10}));
  CHECK(error(s.run("windowcaption", {"off"})) == "New windows will not have a title caption.");  // printed
  REQUIRE(s.run("openwindow").ok());
  CHECK_FALSE(s.editor.windows.current()->chrome().caption);
  CHECK(old.chrome().caption);
  CHECK(error(s.run("windowborder", {"of"})) == "New windows will not have a border.");
  CHECK(error(s.run("windowscrollbars", {"on"})) == "New windows will have scroll bars.");
  CHECK(text(s.run("windowscrollbars", {"on"})).empty());
  CHECK_FALSE(s.run("windowscrollbars").ok());
  CHECK(text(s.run("windowborder")) == "on");  // Tcl result
}

TEST_CASE("captions name the edit cell", "[cap:ui-windows-and-views][req:Window captions]") {
  Session s;
  db::CellDef* top = s.cell("top", {0, 0, 10, 10});
  db::CellDef* inv = s.cell("inv", {0, 0, 10, 10});
  db::CellDef* nand2 = s.cell("nand2", {0, 0, 10, 10});
  auto use = std::make_unique<db::CellUse>();
  use->def = inv;
  use->parent = top;
  top->uses().push_back(std::move(use));
  s.editor.edit_cell = inv;
  s.window(nand2);
  CHECK(text(s.run("windowcaption")) == "nand2 [NOT BEING EDITED]");
  s.window(top);
  CHECK(text(s.run("windowcaption")) == "top EDITING inv");
}

TEST_CASE("openwindow, closewindow and window listings",
          "[cap:ui-windows-and-views][req:Opening and closing windows][req:windowpositions and windownames]") {
  Session s;
  db::CellDef* inv = s.cell("inv", {0, 0, 100, 40});
  s.editor.edit_cell = inv;
  s.editor.box = db::Rect{0, 0, 100, 40};
  REQUIRE(s.run("openwindow").ok());
  auto* w = s.editor.windows.current();
  REQUIRE(w != nullptr);
  CHECK(w->root() == inv);
  CHECK(w->mapping().surface == view::ViewMapping::fit(w->mapping().screen, {-5, -2, 105, 42}).surface);
  CHECK(text(s.run("windownames")) == "0");  // Tcl: the window id
  REQUIRE(s.run("openwindow", {"nand2", "second"}).ok());
  CHECK(text(s.run("windownames", {"all"})) == "1 0");  // ids, top window first
  REQUIRE(s.run("closewindow", {"second"}).ok());
  CHECK(error(s.run("closewindow", {"nosuch"})) == "Window named nosuch cannot be found");
  REQUIRE(s.run("openwindow").ok());
  CHECK(s.editor.windows.current()->id() == 1);  // id reuse
  const auto positions = s.run("windowpositions");
  REQUIRE(positions.ok());
  CHECK(text(positions).find("layout") != std::string::npos);
  REQUIRE(s.run("closewindow").ok());
  REQUIRE(s.run("closewindow").ok());
  CHECK(error(s.run("closewindow")) == "Point to a window first");
}

TEST_CASE("redraw, reset and updatedisplay", "[cap:ui-windows-and-views][req:redraw, reset and updatedisplay]") {
  Session s;
  s.window(s.cell("top", {0, 0, 10, 10}));
  CHECK(s.run("redraw").ok());
  const auto reset = s.run("reset");
  REQUIRE(reset.ok());
  CHECK(reset.diagnostics.front().code == view::codes::kReset);
  REQUIRE(s.run("updatedisplay", {"suspend"}).ok());
  CHECK(s.editor.windows.display_suspended());
  REQUIRE(s.run("updatedisplay", {"resume"}).ok());
  CHECK_FALSE(s.editor.windows.display_suspended());
  CHECK_FALSE(s.run("updatedisplay", {"bogus"}).ok());
}

TEST_CASE("regression: unexpand and expand search below unexpanded uses with magic-compat ON",
          "[cap:cmd-hierarchy][req:Expand and unexpand commands]") {
  // dbExpandFunc always goes on with DBCellSrArea into the use it found,
  // expanded or not, in the frame of the first array element overlapping
  // the area (database/DBexpand.c:251, DBcellsrch.c:1205-1225). Hefesto
  // stopped at unexpanded uses and always took element (xlo, ylo), so a
  // partial `unexpand` never binned an unexpanded child's index (the writers'
  // structure order) and an expand over a later element used the wrong frame.
  for (const bool magic_compat : {true, false}) {
    Session s;
    s.project.compat.magic_compat = magic_compat;
    db::CellDef* top = s.cell("top", {0, 0, 4000, 100});
    db::CellDef* mid = s.cell("mid", {0, 0, 1110, 10});
    db::CellDef* leaf = s.cell("leaf", {0, 0, 10, 10});
    std::vector<db::CellUse*> leaves;
    for (db::Coord i = 0; i < 12; ++i) {
      db::PlaceOptions o;
      o.transform = db::translation(100 * i, 0);
      leaves.push_back(db::place(*mid, *leaf, o).value());
    }
    db::PlaceOptions o;
    o.array = db::ArrayInfo{0, 1, 0, 0, 2000, 0};  // elements at x 0 and 2000
    const db::CellUse* arr = db::place(*top, *mid, o).value();
    view::LayoutWindow& w = s.window(top);
    s.editor.edit_cell = top;
    // Part of element 1 only, over leaf 3 there (x 2300..2310).
    s.editor.box = db::Rect{2300, 0, 2305, 5};
    REQUIRE_FALSE(w.is_expanded(*arr));
    REQUIRE_FALSE(mid->use_index().state_for_test().binned);
    REQUIRE(s.run("unexpand").ok());
    CHECK(mid->use_index().state_for_test().binned == magic_compat);
    REQUIRE(s.run("expand").ok());
    CHECK(w.is_expanded(*arr));
    CHECK(w.is_expanded(*leaves[3]) == magic_compat);
  }
}

namespace {

/// `top` holding one use of `mid`, whose 12 uses of `leaf` sit at x = 100 i.
struct NestedCase {
  Session s;
  db::CellDef* top = nullptr;
  db::CellDef* mid = nullptr;
  db::CellUse* m = nullptr;
  std::vector<db::CellUse*> leaves;

  explicit NestedCase(bool magic_compat, const db::ArrayInfo& array = {}) {
    s.project.compat.magic_compat = magic_compat;
    top = s.cell("top", {0, 0, 4000, 100});
    mid = s.cell("mid", {0, 0, 1110, 10});
    db::CellDef* leaf = s.cell("leaf", {0, 0, 10, 10});
    for (db::Coord i = 0; i < 12; ++i) {
      db::PlaceOptions o;
      o.transform = db::translation(100 * i, 0);
      leaves.push_back(db::place(*mid, *leaf, o).value());
    }
    db::PlaceOptions o;
    o.array = array;
    m = db::place(*top, *mid, o).value();
    (void)s.window(top);
    s.editor.edit_cell = top;
  }
  [[nodiscard]] view::LayoutWindow& window() { return *s.editor.windows.current(); }
};

}  // namespace

TEST_CASE("regression: unexpand with magic-compat ON acts below unexpanded uses, as dbExpandFunc does",
          "[cap:cmd-hierarchy][req:Expand and unexpand commands]") {
  // dbExpandFunc acts on every use its DBCellSrArea search reaches, below
  // expanded and unexpanded uses alike (database/DBexpand.c:182-252).
  // Hefesto stopped at unexpanded uses, so a leaf stayed expanded under a
  // closed parent; magic-compat OFF keeps that rule (Q-MASK-418, proposed).
  for (const bool magic_compat : {true, false}) {
    NestedCase c(magic_compat);
    view::LayoutWindow& w = c.window();
    REQUIRE(c.s.run("expand", {"all"}).ok());
    REQUIRE(w.is_expanded(*c.leaves[3]));
    w.set_expanded(*c.m, false);
    c.s.editor.box = db::Rect{295, -5, 315, 15};  // surrounds leaf 3 only
    REQUIRE(c.s.run("unexpand").ok());
    CHECK_FALSE(w.is_expanded(*c.m));
    CHECK(w.is_expanded(*c.leaves[3]) != magic_compat);
    CHECK(w.is_expanded(*c.leaves[4]));
  }
}

TEST_CASE("regression: unexpand surround with magic-compat ON uses dbExpandFunc's test",
          "[cap:cmd-hierarchy][req:Expand and unexpand commands]") {
  // dbExpandFunc's surround test acts unless the child's bounding box
  // surrounds the area without the area surrounding it
  // (database/DBexpand.c:207-208): a box over part of a leaf unexpands it.
  for (const bool magic_compat : {true, false}) {
    NestedCase c(magic_compat);
    view::LayoutWindow& w = c.window();
    REQUIRE(c.s.run("expand", {"all"}).ok());
    c.s.editor.box = db::Rect{305, 5, 395, 50};  // part of leaf 3, nothing whole
    REQUIRE(c.s.run("unexpand").ok());
    CHECK(w.is_expanded(*c.leaves[3]) != magic_compat);
    CHECK(w.is_expanded(*c.m) != magic_compat);  // the box is not inside mid either
    c.s.editor.box = db::Rect{302, 2, 308, 8};  // inside leaf 3
    REQUIRE(c.s.run("expand", {"all"}).ok());
    REQUIRE(c.s.run("unexpand").ok());
    CHECK(w.is_expanded(*c.leaves[3]));  // its bounding box surrounds the box
  }
}

TEST_CASE("regression: expand with magic-compat ON skips an array whose elements miss the box",
          "[cap:cmd-hierarchy][req:Expand and unexpand commands]") {
  // dbCellSrFunc calls dbExpandFunc only for array elements that overlap the
  // area (DBArrayOverlap, database/DBcellsrch.c:1205-1225): a box in the
  // gap between two elements expands nothing, although it overlaps the
  // array's bounding box.
  for (const bool magic_compat : {true, false}) {
    NestedCase c(magic_compat, db::ArrayInfo{0, 1, 0, 0, 2000, 0});  // elements at x 0 and 2000
    c.s.editor.box = db::Rect{1500, 0, 1505, 5};
    REQUIRE(c.s.run("expand").ok());
    CHECK(c.window().is_expanded(*c.m) != magic_compat);
  }
}

TEST_CASE("regression: expand all toggle expands with magic-compat ON, as CmdExpand does",
          "[cap:cmd-hierarchy][req:Expand and unexpand commands]") {
  // CmdExpand passes DB_EXPAND for `all`, with or without toggle
  // (commands/CmdE.c:950-966); magic-compat OFF toggles (Q-MASK-418, proposed).
  for (const bool magic_compat : {true, false}) {
    NestedCase c(magic_compat);
    REQUIRE(c.s.run("expand", {"all"}).ok());
    REQUIRE(c.s.run("expand", {"all", "toggle"}).ok());
    CHECK(c.window().is_expanded(*c.m) == magic_compat);
  }
}
