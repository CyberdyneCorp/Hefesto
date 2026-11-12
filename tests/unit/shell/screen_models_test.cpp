// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The M2 popovers, menus and sheets (screen_models.hpp): command history and
// completion, undo history, layer row and orientation menus, the key-binding
// sheet, the color window screen and F6 focus cycling. Entries that issue
// commands are run through a real core session.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/shell/core_command_sink.hpp"
#include "hefesto/shell/press_hold.hpp"
#include "hefesto/shell/screen_models.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "hefesto/ui/state.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "hefesto/tech/loader.hpp"
#include "shell_fixture.hpp"

using namespace hefesto::shell;
namespace db = hefesto::db;
namespace tech = hefesto::tech;
namespace view = hefesto::view;

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
    (void)hefesto::shell::test::bind_interpreter(core);
    auto& ed = core.editor();
    ed.technology = tiny();
    top = ed.cells.create("top", ed.technology->plane_count()).value();
    ed.edit_cell = top;
    auto w = ed.windows.open(top, view::ScreenRect{0, 0, 499, 499});
    REQUIRE(w.ok());
    ed.windows.set_current(w.value()->id());
  }
  CommandOutcome submit(const std::string& line) { return CoreCommandSink(core.dispatcher()).submit(line); }
  bool run(const std::string& line) { return submit(line).ok(); }
};

}  // namespace

TEST_CASE("the history popover lists the newest commands, numbered like history",
          "[cap:hefesto-input-and-touch][req:Command entry on every platform]") {
  const std::vector<std::string> history = {"box 0 0 10 10", "paint metal1", "select area metal1", "what"};
  const auto all = history_entries(history);
  REQUIRE(all.size() == 4U);
  CHECK(all.front().label == "1  box 0 0 10 10");
  CHECK(all.back().label == "4  what");
  CHECK(all.back().command == "what");
  CHECK(all.back().prompt);  // chosen: back in the command bar to edit or run
  const auto newest = history_entries(history, {}, 2);
  REQUIRE(newest.size() == 2U);
  CHECK(newest.front().label == "3  select area metal1");
  const auto filtered = history_entries(history, "METAL");
  REQUIRE(filtered.size() == 2U);
  CHECK(filtered.front().label == "2  paint metal1");
  CHECK(history_entries({}).empty());
}

TEST_CASE("the completion popover completes the last word",
          "[cap:hefesto-input-and-touch][req:Command entry on every platform]") {
  CHECK(replace_last_word("paint m", "metal1") == "paint metal1 ");
  CHECK(replace_last_word("pai", "paint") == "paint ");
  CHECK(replace_last_word("", "box") == "box ");
  Completion c{"paint metal", {"metal1", "metal2"}};
  const auto entries = completion_entries("paint metal", c);
  REQUIRE(entries.size() == 2U);
  CHECK(entries[1].label == "metal2");
  CHECK(entries[1].command == "paint metal2 ");
  CHECK(entries[1].prompt);
  Completion many{"x", std::vector<std::string>(20, "y")};
  const auto capped = completion_entries("x", many, 12);
  REQUIRE(capped.size() == 13U);
  CHECK_FALSE(capped.back().enabled);
  CHECK(capped.back().label.find("8 more") != std::string::npos);
}

TEST_CASE("the undo history issues undo N and redo N",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  Session s;
  CHECK(undo_history(s.core.editor().undo).entries.empty());
  CHECK(next_undo_step(s.core.editor().undo, false).empty());
  REQUIRE(s.run("box 0 0 10 10"));
  REQUIRE(s.run("paint metal1"));
  REQUIRE(s.run("box 0 0 4 4"));
  REQUIRE(s.run("erase metal1"));
  const hefesto::undo::UndoLog& log = s.core.editor().undo;
  UndoHistory h = undo_history(log);
  REQUIRE(h.entries.size() >= 2U);
  CHECK(h.entries.front().command == "undo 1");
  CHECK(h.entries[1].command == "undo 2");
  CHECK(h.title.find(" steps · limit ") != std::string::npos);
  const std::string step = next_undo_step(log, false);
  CHECK_FALSE(step.empty());
  CHECK(h.entries.front().label.find(step) != std::string::npos);
  for (const MenuEntry& e : h.entries) CHECK(e.label.size() <= 64U);  // menu width
  // Choosing "undo 2" undoes two steps; the redo tail then leads the list.
  const std::size_t before = log.position();
  REQUIRE(s.run(h.entries[1].command));
  CHECK(log.position() == before - 2);
  h = undo_history(log);
  REQUIRE(h.entries.size() >= 3U);
  CHECK(h.entries[0].command == "redo 2");
  CHECK(h.entries[1].command == "redo 1");
  CHECK(h.entries[2].command.starts_with("undo"));
  CHECK(h.entries[2].separator_before);
  CHECK_FALSE(next_undo_step(log, true).empty());
  REQUIRE(s.run(h.entries[1].command));
  CHECK(log.position() == before - 1);
  // While recording is off Magic refuses to undo: only redo stays enabled.
  REQUIRE(s.run("undo disable"));
  for (const MenuEntry& e : undo_history(log).entries) CHECK(e.enabled == e.command.starts_with("redo"));
  REQUIRE(s.run("undo enable"));
}

TEST_CASE("the undo history names each step after the command that made it",
          "[cap:hefesto-app-ux][req:Undo history]") {
  Session s;
  REQUIRE(s.run("box 0 0 10 10"));
  REQUIRE(s.run("paint metal1"));
  REQUIRE(s.run("box 0 0 4 4"));
  REQUIRE(s.run("erase metal1"));
  const hefesto::undo::UndoLog& log = s.core.editor().undo;
  UndoHistory h = undo_history(log);
  REQUIRE(h.entries.size() >= 2U);
  CHECK(h.entries[0].label == "↶ erase metal1");
  CHECK(h.entries[0].command == "undo 1");
  CHECK(next_undo_step(log, false) == "erase metal1");
  REQUIRE(s.run("undo"));
  h = undo_history(log);
  CHECK(h.entries[0].label == "↷ erase metal1");
  CHECK(h.entries[0].command == "redo 1");
  CHECK(next_undo_step(log, true) == "erase metal1");
  // Scenario "Undo two steps from the history": the message names the older of the two
  // undone steps by its command.
  REQUIRE(s.run("redo"));
  const UndoMark mark = undo_mark(log);
  REQUIRE(s.run("undo 2"));
  const auto moved = undo_move_message(mark, log);
  REQUIRE(moved.has_value());
  CHECK(moved->starts_with("Undid " + next_undo_step(log, true)));
}

TEST_CASE("pressing and holding Undo opens the history; a short press undoes",
          "[cap:hefesto-app-ux][req:Undo history]") {
  STATIC_REQUIRE(kHoldDelay == std::chrono::milliseconds{500});
  PressHold hold;
  // A press released before the delay is a click (Undo runs `undo`).
  hold.press();
  CHECK(hold.pending());
  CHECK(hold.release());
  CHECK_FALSE(hold.elapse(true));  // the delay of a released press does nothing
  // Held past the delay: the history opens and the release is not a click.
  hold.press();
  CHECK(hold.elapse(true));
  CHECK(hold.held());
  CHECK_FALSE(hold.release());
  CHECK_FALSE(hold.held());
  // The button came up elsewhere (no release on the widget): no history.
  hold.press();
  CHECK_FALSE(hold.elapse(false));
  CHECK_FALSE(hold.release());
}

TEST_CASE("after an undo the status bar names the step",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  CHECK(undo_status_message(false, "paint metal1", 23) == "Undid paint metal1 · 23 more steps");
  CHECK(undo_status_message(false, "paint metal1", 1) == "Undid paint metal1 · 1 more step");
  CHECK(undo_status_message(true, "move", 0) == "Redid move");
  CHECK_FALSE(undo_status_message(false, "", 3).has_value());
}

TEST_CASE("the layer row menu shows the command of every item",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  Session s;
  LayerRow row;
  row.name = "metal1";
  row.plane = "metal1";
  row.paint = true;
  const auto menu = layer_menu(row);
  REQUIRE(menu.size() == 6U);
  CHECK(menu[0].checked);
  CHECK(menu[0].command == "palette paint metal1");
  for (const MenuEntry& e : menu) {
    CHECK(e.label.find(e.command) != std::string::npos);  // the board: every item names its command
    CHECK_FALSE(e.prompt);
  }
  REQUIRE(s.run("box 0 0 10 10"));
  for (std::size_t i = 1; i < menu.size(); ++i) {
    const CommandOutcome out = s.submit(menu[i].command);
    INFO(menu[i].command << " -> " << out.message);
    CHECK(out.ok());
  }
  row.locked = true;
  row.visible = false;
  const auto locked = layer_menu(row);
  CHECK_FALSE(locked[1].enabled);  // Paint on box: locked
  CHECK(locked[4].command == "see metal1");
  CHECK(locked[5].command == "tech unlock metal1");
}

TEST_CASE("the inspector sets an instance's orientation and array",
          "[cap:hefesto-app-ux][req:Main window layout]") {
  const auto menu = orient_menu("FN");
  REQUIRE(menu.size() == 8U);
  CHECK(std::count_if(menu.begin(), menu.end(), [](const MenuEntry& e) { return e.checked; }) == 1);
  CHECK(std::find_if(menu.begin(), menu.end(), [](const MenuEntry& e) { return e.checked; })->command == "orient FN");

  Session s;
  auto& ed = s.core.editor();
  db::CellDef* leaf = ed.cells.create("leaf", ed.technology->plane_count()).value();
  REQUIRE(s.run("box 0 0 10 10"));
  REQUIRE(s.run("paint metal1"));
  (void)leaf;
  REQUIRE(s.run("load leaf"));
  REQUIRE(s.run("box 0 0 10 10"));
  REQUIRE(s.run("paint metal1"));
  REQUIRE(s.run("load top"));
  REQUIRE(s.run("box 0 0 10 10"));
  REQUIRE(s.run("getcell leaf"));
  std::vector<InspectorAction> actions;
  const auto view = selection_inspector_view(ed, ed.windows.current()->root(), UnitScale{1, 1, std::nullopt}, &actions);
  REQUIRE(view.has_value());
  const auto orient = std::find_if(actions.begin(), actions.end(), [](const InspectorAction& a) { return a.menu; });
  REQUIRE(orient != actions.end());
  CHECK(orient->command == "orient");
  const auto array = std::find_if(actions.begin(), actions.end(),
                                  [](const InspectorAction& a) { return a.command.starts_with("array"); });
  REQUIRE(array != actions.end());
  CHECK(array->command == "array ");
  CHECK(array->label == "Array…");
  // The menu's and the prompt's commands act on the selected instance.
  REQUIRE(s.run("orient E"));
  REQUIRE(s.run("array 3 2"));
  const auto rotated = selection_inspector_view(ed, ed.windows.current()->root(), UnitScale{1, 1, std::nullopt});
  REQUIRE(rotated.has_value());
  const auto has_row = [&rotated](const std::string& key, const std::string& value) {
    return std::any_of(rotated->rows.begin(), rotated->rows.end(),
                       [&](const InspectorRow& r) { return r.key == key && r.value == value; });
  };
  CHECK(has_row("Orientation", "E"));
  CHECK((has_row("Array", "3 × 2") || has_row("Array", "2 × 3")));
}

TEST_CASE("the key-binding sheet lists Magic's tables and edits through macro",
          "[cap:ui-tools-and-input][req:Default key bindings]") {
  Session s;
  const auto& macros = s.core.editor().macros;
  const auto tables = key_binding_tables(macros);
  REQUIRE_FALSE(tables.empty());
  const auto layout = std::find_if(tables.begin(), tables.end(), [](const KeyBindingTable& t) { return t.name == "box"; });
  REQUIRE(layout != tables.end());
  CHECK(layout->label == "Layout (box tool)");
  const auto rows = key_binding_rows(macros, "box");
  REQUIRE_FALSE(rows.empty());
  for (const KeyBindingRow& row : rows) {
    INFO(row.key << " " << row.command);
    CHECK((row.edit.starts_with("macro box ") || row.edit.starts_with("imacro box ")));
  }
  // Filtering by key or command, case-insensitive.
  const auto undo_rows = key_binding_rows(macros, "box", "UNDO");
  REQUIRE_FALSE(undo_rows.empty());
  for (const KeyBindingRow& row : undo_rows) CHECK(row.command.find("undo") != std::string::npos);
  // A row's edit line round-trips through `macro`.
  const KeyBindingRow& first = rows.front();
  REQUIRE(s.run(first.edit));
  const auto again = key_binding_rows(macros, "box");
  REQUIRE_FALSE(again.empty());
  CHECK(again.front().command == first.command);
  // An edited binding shows its new text.
  REQUIRE(s.run("macro box z \"echo sheet-check\""));
  const auto z = key_binding_rows(macros, "box", "sheet-check");
  REQUIRE(z.size() == 1U);
  CHECK(z.front().key == "z");
  CHECK(z.front().edit == "macro box z \"echo sheet-check\"");
}

TEST_CASE("the color window screen shows the edited color and issues Magic's button presses",
          "[cap:ui-color-window][req:Color window buttons]") {
  Session s;
  auto& ed = s.core.editor();
  CHECK_FALSE(color_window_view(ed).has_value());  // no color window
  ed.styles.emplace();
  ed.styles->colors.entries.resize(64);
  ed.styles->colors.entries[39] = {184, 73, 83, {}};
  REQUIRE(s.run("specialopen color 047"));
  auto v = color_window_view(ed);
  REQUIRE(v.has_value());
  CHECK(v->caption == "COLOR = 047");
  CHECK(v->octal == "047");
  CHECK(v->decimal == "39 decimal");
  CHECK_FALSE(v->modified);
  CHECK(v->values == "R 184 G 73 B 83   H 0.985 S 0.603 V 0.722   #B84953");
  CHECK(v->bars[0].name == "Red");
  CHECK(v->bars[5].name == "Value");
  CHECK(v->previous == "color 046");
  CHECK(v->next == "color 050");
  for (const ColorBarView& b : v->bars) {
    CHECK(b.minus.find("; pushbutton left") != std::string::npos);
    CHECK(b.plus.starts_with("setpoint "));
  }
  // "+" on Red: one 0.0078 step up (184 -> 186), an undoable map change.
  REQUIRE(s.run(v->bars[0].plus));
  v = color_window_view(ed);
  REQUIRE(v.has_value());
  CHECK(ed.styles->colors.entries[39].r == 186);
  CHECK(v->modified);
  // A click at the middle of the Green bar sets it to one half.
  REQUIRE(s.run(bar_click_line(ed, 1, 0.5)));
  CHECK(ed.styles->colors.entries[39].g >= 126);
  CHECK(ed.styles->colors.entries[39].g <= 129);
  CHECK(bar_click_line(ed, 6, 0.5).empty());
  // The stepper edits another entry.
  // Tcl now (m4-scripting 13.2): the binding sends `color` to the color
  // window under the `setpoint` point, as Magic's Tcl dispatcher does.
  const CommandOutcome stepped = s.submit("setpoint 10 10 " + std::to_string(v->window) + "; " + v->next);
  for (const std::string& d : stepped.diagnostics) UNSCOPED_INFO(d);
  REQUIRE(stepped.ok());
  v = color_window_view(ed);
  REQUIRE(v.has_value());
  CHECK(v->caption == "COLOR = 050");
}

TEST_CASE("F6 cycles focus across the docks, skipping hidden panels",
          "[cap:hefesto-app-ux][req:Accessibility and localization]") {
  FocusRegions all{true, true, true};
  CHECK(next_focus_region(FocusRegion::tools, all, false) == FocusRegion::layers);
  CHECK(next_focus_region(FocusRegion::command, all, false) == FocusRegion::tools);
  CHECK(next_focus_region(FocusRegion::tools, all, true) == FocusRegion::command);
  FocusRegions few{false, false, false};
  CHECK(next_focus_region(FocusRegion::tools, few, false) == FocusRegion::canvas);
  CHECK(next_focus_region(FocusRegion::canvas, few, false) == FocusRegion::command);
  CHECK(next_focus_region(FocusRegion::canvas, few, true) == FocusRegion::tools);
}

TEST_CASE("panels dock left or right and reset to the default layout",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  std::vector<std::pair<PanelId, DockSide>> docked;
  bool reset = false;
  bool keys = false;
  ViewActions actions;
  actions.panel = [&keys](PanelId id, std::optional<bool> show) {
    if (id == PanelId::keys) keys = show.value_or(!keys);
    return id == PanelId::keys ? keys : true;
  };
  actions.panel_shown = [&keys](PanelId id) { return id == PanelId::keys ? keys : true; };
  actions.dock = [&docked](PanelId id, DockSide side) {
    if (id != PanelId::layers && id != PanelId::inspector) return false;
    docked.emplace_back(id, side);
    return true;
  };
  actions.reset = [&reset] { reset = true; };
  Session s;
  REQUIRE(register_view_commands(s.core.registry(), actions));
  (void)hefesto::shell::test::bind_interpreter(s.core);  // binds `panel` too
  CHECK(s.submit("panel inspector dock left").ok());
  CHECK(s.submit("panel layers dock right").ok());
  REQUIRE(docked.size() == 2U);
  CHECK(docked[0] == std::pair{PanelId::inspector, DockSide::left});
  CHECK(docked[1] == std::pair{PanelId::layers, DockSide::right});
  CHECK_FALSE(s.submit("panel console dock left").ok());
  CHECK_FALSE(s.submit("panel layers dock up").ok());
  CHECK(s.submit("panel reset").ok());
  CHECK(reset);
  CHECK(s.submit("panel keys show").ok());
  CHECK(keys);
  CHECK(s.submit("panel keys").message == "shown");
}

// Regression (M2 task 7.5): lines typed in the command bar feed the session's command history and are
// expanded with Readline rules before they run, so `history` and `!` designators see them.
TEST_CASE("typed command-bar lines are history-expanded and recorded in the session history",
          "[cap:ui-command-dispatch][req:Command history]") {
  Session s;
  auto& ed = s.core.editor();
  const TypedLine first = expand_typed_line(ed, "box 0 0 10 10");
  REQUIRE(first.run);
  CHECK(*first.run == "box 0 0 10 10");
  CHECK(s.run(*first.run));
  const TypedLine again = expand_typed_line(ed, "!!");
  REQUIRE(again.run);
  CHECK(*again.run == "box 0 0 10 10");
  const TypedLine swapped = expand_typed_line(ed, "^10^20^");
  REQUIRE(swapped.run);
  CHECK(*swapped.run == "box 0 0 20 10");  // `^old^new^` replaces the first occurrence
  const TypedLine shown = expand_typed_line(ed, "!1:p");
  CHECK_FALSE(shown.run);
  CHECK(shown.print == "box 0 0 10 10");
  const TypedLine missing = expand_typed_line(ed, "!nosuch");
  CHECK_FALSE(missing.run);
  REQUIRE(missing.error);
  CHECK(missing.error->text.find("event not found") != std::string::npos);
  const CommandOutcome listed = s.submit("history");
  REQUIRE(listed.ok());
  CHECK((listed.output + listed.printed).find("box 0 0 20 10") != std::string::npos);
}

// Regression (round 3 review): after `panel layers dock right; panel inspector dock left` the panels kept
// their edge separators on the side they were built with (no line next to the canvas, a doubled edge by
// the tool rail). The edge goes on the canvas side of each dock.
TEST_CASE("a docked panel's edge separator is on the canvas side",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  CHECK(panel_edge_index(DockSide::left) == 1);   // body, then the edge next to the canvas
  CHECK(panel_edge_index(DockSide::right) == 0);  // the edge next to the canvas, then the body
}

// Regression (round 3 review): the "Undid … · N more steps" message appeared only when the typed line was
// exactly `undo` or `redo`; `undo 3`, `paint m1; undo`, the `u`/`U` keys and the undo history menu gave
// none. The message now comes from the undo log's movement around any command.
TEST_CASE("any command that moves the undo log names the step it passed",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  Session s;
  const hefesto::undo::UndoLog& log = s.core.editor().undo;
  REQUIRE(s.run("box 0 0 10 10"));
  REQUIRE(s.run("paint metal1"));
  REQUIRE(s.run("box 0 0 4 4"));
  REQUIRE(s.run("erase metal1"));
  UndoMark mark = undo_mark(log);
  const std::string newest = next_undo_step(log, false);
  REQUIRE_FALSE(newest.empty());
  // A new step is not a move.
  REQUIRE(s.run("box 0 0 2 2"));
  CHECK_FALSE(undo_move_message(mark, log).has_value());
  mark = undo_mark(log);
  const std::string top = next_undo_step(log, false);
  // `undo 2`: names the older undone step (the next one to redo), counts the steps left.
  REQUIRE(s.run("undo 2"));
  const auto undid = undo_move_message(mark, log);
  REQUIRE(undid.has_value());
  CHECK(undid->starts_with("Undid " + next_undo_step(log, true)));
  CHECK_FALSE(undid->starts_with("Undid " + top));
  CHECK(undid->find(std::to_string(log.position()) + " more step") != std::string::npos);
  // `redo` through a multi-command line: names the step it redid.
  mark = undo_mark(log);
  const std::string next = next_undo_step(log, true);
  REQUIRE(s.run("redo"));
  const auto redid = undo_move_message(mark, log);
  REQUIRE(redid.has_value());
  CHECK(redid->starts_with("Redid " + next));
  mark = undo_mark(log);
  CHECK_FALSE(undo_move_message(mark, log).has_value());  // nothing moved
}

// As the desktop checks it: in a client update handler, after the post-command update closed each
// command's step, so `paint m2; undo` in one line still reports the undo.
TEST_CASE("the undo message holds for an undo inside a multi-command line",
          "[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]") {
  Session s;
  auto& ed = s.core.editor();
  UndoMark mark;
  std::vector<std::string> messages;
  hefesto::ui::state(ed).update_handlers.emplace_back([&](hefesto::editor::EditorState& e) {
    if (auto moved = undo_move_message(mark, e.undo)) messages.push_back(*moved);
    mark = undo_mark(e.undo);
  });
  REQUIRE(s.run("box 0 0 10 10; paint metal1; box 0 0 3 3; paint metal2; undo"));
  REQUIRE(messages.size() == 1U);
  CHECK(messages.front().starts_with("Undid "));
  REQUIRE(s.run("paint metal1"));  // a new step after an undo is not a redo
  CHECK(messages.size() == 1U);
  REQUIRE(s.run("undo; redo"));
  REQUIRE(messages.size() == 3U);
  CHECK(messages[2].starts_with("Redid "));
}
