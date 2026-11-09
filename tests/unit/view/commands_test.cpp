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

TEST_CASE("crosshair is shown in every window on the same root", "[cap:ui-windows-and-views][req:Crosshair]") {
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
