// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M2 editing view models (DESIGN boards docs/design/mockups/M2-*.dc.html):
// tool rail, layer palette toggles, command completion, multi-line results,
// prompt dialogs, selection inspector, undo affordances, feedback rows and
// cursor shapes.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/tech/loader.hpp"

using namespace hefesto::shell;
namespace db = hefesto::db;
namespace tech = hefesto::tech;

namespace {

hefesto::platform::PortableOptions test_options() {
  return hefesto::platform::PortableOptions{.storage_root = "/app", .pdk_roots = {}, .in_memory = true,
                                           .inline_tasks = true};
}

std::shared_ptr<const tech::Technology> tiny() {
  hefesto::platform::LocalFileSystem local;
  hefesto::diag::CollectingSink sink;
  auto loaded = tech::load_technology(local, std::string{HEFESTO_SOURCE_DIR} + "/tests/data/tech/tiny.tech",
                                      tech::LoadOptions{}, sink);
  REQUIRE(loaded.ok());
  return loaded.value();
}

/// A session on the tiny technology with a cell "top" in the current window.
struct Session {
  CoreSession core{test_options()};
  db::CellDef* top = nullptr;

  Session() {
    auto& ed = core.editor();
    ed.technology = tiny();
    top = ed.cells.create("top", ed.technology->plane_count()).value();
    ed.edit_cell = top;
    auto w = ed.windows.open(top, hefesto::view::ScreenRect{0, 0, 499, 499});
    REQUIRE(w.ok());
    ed.windows.set_current(w.value()->id());
  }
  bool run(const std::string& line) { return CoreCommandSink(core.dispatcher()).submit(line).ok(); }
};

const UnitScale kInternal{1, 1, std::nullopt};

}  // namespace

TEST_CASE("the rail shows Magic's current tool and the box tool's modes",
          "[cap:hefesto-design-system][req:Icons]") {
  CHECK(active_rail_icon("box", EditMode::none) == "box");
  CHECK(active_rail_icon("box", EditMode::paint) == "paint");
  CHECK(active_rail_icon("box", EditMode::measure) == "measure");
  CHECK(active_rail_icon("pick", EditMode::paint) == "select");  // a mode applies to the box tool only
  CHECK(active_rail_icon("wiring", EditMode::none) == "wire");
  CHECK_FALSE(active_rail_icon("nettool", EditMode::none).has_value());
  // Buttons issue `tool` commands; Paint, Label and Measure return to the box tool.
  CHECK(rail_action("select", "box")->command == "tool pick");
  CHECK(rail_action("wire", "box")->command == "tool wiring");
  CHECK(rail_action("paint", "box")->command.empty());
  CHECK(rail_action("paint", "box")->mode == EditMode::paint);
  CHECK(rail_action("label", "wiring")->command == "tool box");
  CHECK(rail_action("fit", "box")->command == "view");
  CHECK_FALSE(rail_action("place", "box").has_value());
  // Every rail tool but Place device is live; its tooltip names M4.
  for (const ToolButton& b : tool_rail()) {
    CHECK(b.enabled == (b.icon != "place"));
    CHECK_FALSE(b.tooltip.empty());
  }
  const ToolButton& paint = tool_rail()[2];
  REQUIRE(paint.icon == "paint");
  CHECK(rail_tooltip(paint, false) == "Paint: choose a layer to paint in the Layers panel");
  CHECK(rail_tooltip(paint, true) == std::string(paint.tooltip));
}

TEST_CASE("cursor glyphs map to the system pointer shapes", "[cap:ui-tools-and-input][req:Box tool buttons]") {
  CHECK(cursor_shape(0, EditMode::none) == CursorShape::arrow);
  CHECK(cursor_shape(6, EditMode::none) == CursorShape::move);  // box lower-left (move)
  CHECK(cursor_shape(5, EditMode::none) == CursorShape::resize_nesw);  // upper-right (resize)
  CHECK(cursor_shape(3, EditMode::none) == CursorShape::resize_nwse);
  CHECK(cursor_shape(19, EditMode::none) == CursorShape::crosshair);  // wiring
  CHECK(cursor_shape(21, EditMode::none) == CursorShape::move);
  CHECK(cursor_shape(0, EditMode::paint) == CursorShape::crosshair);
  CHECK(cursor_shape(0, EditMode::label) == CursorShape::ibeam);
}

TEST_CASE("layer palette rows carry the paint layer and lock toggles",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  LayerRow row;
  row.name = "metal2";
  row.plane = "metal2";
  CHECK(row.meta() == "metal2");
  row.paint = true;
  CHECK(row.meta() == "paint");
  row.locked = true;
  CHECK(row.meta() == "paint · locked");
  CHECK(row.accessible_name() == "metal2, plane metal2, visible, paint layer, locked");
  CHECK(row.lock_command() == "tech unlock metal2");
  row.locked = false;
  CHECK(row.lock_command() == "tech lock metal2");
  row.visible = false;
  CHECK(row.meta() == "hidden");
  // Click zones: eye, lock, else the row (paint layer).
  const LayerRowIcons icons = layer_row_icons(228.0F, 40.0F);
  CHECK(layer_row_hit(icons.eye + 5.0F, 228.0F, 40.0F) == LayerRowHit::visibility);
  CHECK(layer_row_hit(icons.lock + 5.0F, 228.0F, 40.0F) == LayerRowHit::lock);
  CHECK(layer_row_hit(40.0F, 228.0F, 40.0F) == LayerRowHit::paint);
}

TEST_CASE("Layer palette paint: the paint layer is painted and erased on the box",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  Session s;
  auto& ed = s.core.editor();
  REQUIRE(s.run("box 0 0 10 10"));
  // The footer buttons and the Paint tool issue `paint <layer>` / `erase <layer>`.
  REQUIRE(s.run("paint metal1"));
  const tech::TileType m1 = *ed.technology->find_type("metal1").id;
  const tech::PlaneId plane = *ed.technology->home_plane(m1);
  CHECK(s.top->plane(plane).type_at({5, 5}) == m1);
  REQUIRE(s.run("tech lock metal1"));
  CHECK(ed.locked_types.test(m1));
  REQUIRE(s.run("erase metal1"));
  CHECK(s.top->plane(plane).type_at({5, 5}) == m1);  // a locked layer is not erased
  REQUIRE(s.run("tech unlock metal1"));
  REQUIRE(s.run("erase metal1"));
  CHECK(s.top->plane(plane).type_at({5, 5}) == tech::kSpace);
}

TEST_CASE("Tab completes commands, cells and layers through ui::complete",
          "[cap:hefesto-input-and-touch][req:Command entry on every platform]") {
  Session s;
  auto& ed = s.core.editor();
  (void)ed.cells.create("inverter", ed.technology->plane_count());
  (void)ed.cells.create("invx2", ed.technology->plane_count());
  const auto& reg = s.core.registry();
  // One candidate completes the word and adds a space.
  CHECK(complete_command_line("getc", reg, ed).line == "getcell ");
  // Several extend the word to their common prefix and are listed.
  Completion c = complete_command_line("load inv", reg, ed);
  CHECK(c.line == "load inv");
  CHECK(c.candidates == std::vector<std::string>{"inverter", "invx2"});
  CHECK(complete_command_line("load inve", reg, ed).line == "load inverter ");
  // Layers after paint, erase, see and tech lock.
  CHECK(complete_command_line("paint metal", reg, ed).candidates.size() == 2U);
  CHECK(complete_command_line("paint metal2", reg, ed).line == "paint metal2 ");
  c = complete_command_line("tech lock ndif", reg, ed);  // a name and its alias
  CHECK(c.line == "tech lock ndiff");
  CHECK(c.candidates == std::vector<std::string>{"ndiff", "ndiffusion"});
  // A new word after `see` lists every layer; other arguments are left alone.
  CHECK(complete_command_line("see ", reg, ed).candidates.size() > 5U);
  CHECK(complete_command_line("box 1", reg, ed).line == "box 1");
  CHECK(complete_command_line("zzzz", reg, ed).line == "zzzz");
}

TEST_CASE("a multi-line result shows its first line and how many more are in the Console",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  CHECK(result_line("ok").text() == "ok");
  CHECK(result_line("a\nb\nc").text() == "a (+2 lines, see Console)");
  CHECK(result_line("a\nb\n").text() == "a (+1 line, see Console)");
  CHECK(result_line("a\n").more_lines == 0U);
  CHECK(result_line("").text().empty());
  // What a command prints (`help`) is collected for the summary.
  Session s;
  const CommandOutcome help = CoreCommandSink(s.core.dispatcher()).submit("help box");
  REQUIRE(help.ok());
  CHECK(result_line(help.printed).more_lines > 0U);
}

TEST_CASE("PromptHost questions become dialogs whose buttons say what they do",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  const std::vector<std::string> no_yes{"no", "yes"};
  PromptDialog d = prompt_dialog("Really throw away all changes made to cell top? ", no_yes, 0);
  CHECK(d.title == "Really throw away all changes made to cell top?");
  REQUIRE(d.buttons.size() == 2U);
  CHECK(d.buttons[0].label == "Discard changes");
  CHECK(d.buttons[0].answer == 1U);
  CHECK(d.buttons[1].label == "Cancel");  // the default answer, last and primary
  CHECK(d.buttons[1].primary);
  d = prompt_dialog("File top.mag already exists.\n  Overwrite it with top? ", no_yes, 0);
  CHECK(d.title == "File top.mag already exists.");
  CHECK(d.buttons[0].label == "Overwrite");
  d = prompt_dialog("Recover from backup file /tmp/x?", no_yes, 1);
  CHECK(d.buttons.back().label == "Recover");
  // Answers without a known verb keep their words.
  const std::vector<std::string> five{"write", "flush", "skip", "abort", "autowrite"};
  d = prompt_dialog("top has been modified.", five, 0);
  REQUIRE(d.buttons.size() == 5U);
  CHECK(d.buttons.back().label == "Write");
  CHECK(d.buttons.front().label == "Flush");
  // Escape (0) and out-of-range results give the default answer.
  CHECK(dialog_answer(0, 2, 1) == 1U);
  CHECK(dialog_answer(1, 2, 1) == 0U);
  CHECK(dialog_answer(9, 2, 0) == 0U);
}

TEST_CASE("the inspector shows the selection by kind with its actions",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  Session s;
  auto& ed = s.core.editor();
  CHECK_FALSE(selection_inspector_view(ed, s.top, kInternal).has_value());  // nothing selected: box or cell
  REQUIRE(s.run("box 0 0 10 4"));
  REQUIRE(s.run("paint metal1"));
  REQUIRE(s.run("select area"));
  std::vector<InspectorAction> actions;
  auto view = selection_inspector_view(ed, s.top, kInternal, &actions);
  REQUIRE(view.has_value());
  CHECK(view->section == "SELECTION");
  CHECK(view->title == "metal1");
  CHECK(view->subtitle == "1 tile on 1 layer");
  CHECK(actions.front().command == "select net");
  // A label.
  REQUIRE(s.run("select clear"));
  REQUIRE(s.run("box 2 2 2 2"));
  REQUIRE(s.run("label VDD"));
  REQUIRE(s.run("select area labels"));
  actions.clear();
  view = selection_inspector_view(ed, s.top, kInternal, &actions);
  REQUIRE(view.has_value());
  CHECK(view->title == "VDD");
  CHECK(view->subtitle == "Label");
  CHECK(actions.front().command == "setlabel text ");
  // Another window's root: not shown there.
  CHECK(selection_inspector_view(ed, nullptr, kInternal).has_value());
  db::CellDef* other = ed.cells.create("other", ed.technology->plane_count()).value();
  CHECK_FALSE(selection_inspector_view(ed, other, kInternal).has_value());
}

TEST_CASE("undo and redo affordances follow the undo log", "[cap:hefesto-app-ux][req:Main window layout]") {
  Session s;
  auto& ed = s.core.editor();
  UndoAffordance a = undo_affordance(ed);
  CHECK_FALSE(a.can_undo);
  CHECK(a.undo_tooltip == "Undo (u): nothing to undo");
  REQUIRE(s.run("box 0 0 4 4"));
  REQUIRE(s.run("paint metal1"));
  a = undo_affordance(ed);
  CHECK(a.can_undo);
  CHECK_FALSE(a.can_redo);
  REQUIRE(s.run("undo"));
  a = undo_affordance(ed);
  CHECK(a.can_redo);
  CHECK(a.redo_tooltip.starts_with("Redo (U): 1 step"));
}

TEST_CASE("the Feedback tab lists the feedback areas with `feedback find`",
          "[cap:ui-feedback-and-highlights][req:Feedback display]") {
  Session s;
  CHECK(feedback_rows(s.core.editor(), kInternal).empty());
  REQUIRE(s.run("box 0 0 4 2"));
  REQUIRE(s.run("feedback add \"too narrow\""));
  const auto rows = feedback_rows(s.core.editor(), kInternal);
  REQUIRE(rows.size() == 1U);
  CHECK(rows[0].text == "1  (0, 0) – (4, 2)  too narrow");
  CHECK(rows[0].find == "feedback find 1");
}
