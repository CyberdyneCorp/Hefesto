// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Toolkit-free view models of the M2 popovers, menus and sheets (DESIGN
// boards docs/design/mockups/M2-Command-Bar, M2-Undo-Redo, M2-Layer-Palette,
// M2-Selection, M2-Key-Bindings, M2-Color-Window, M2-Panels): the command
// history and completion popovers, the undo history, the layer row menu,
// the inspector's orientation menu, the key-binding sheet, the color window
// screen and the F6 focus order. Every entry carries the core command it
// issues, so the Raven layer only shows what these functions return and
// every action stays a logged, replayable command.
//
// Owner: SHELL (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/shell/editing_models.hpp"
#include "hefesto/shell/view_commands.hpp"
#include "hefesto/shell/view_models.hpp"

namespace hefesto::shell {

/// One entry of a popover or menu.
struct MenuEntry {
  std::string label;
  std::string command;          // what choosing it issues (empty: nothing)
  bool prompt = false;          // true: put `command` in the command bar instead of running it
  bool checked = false;         // the current choice (orientation, paint layer)
  bool enabled = true;
  bool separator_before = false;

  friend bool operator==(const MenuEntry&, const MenuEntry&) = default;
};

// --- command bar popovers ---------------------------------------------------------

/// The history popover (board M2-Command-Bar, tile 2): the newest `max`
/// entries, oldest first, each "NN  line" numbered like Magic's `history`
/// (1 = the oldest kept); choosing one puts the line in the command bar.
/// Entries containing `filter` (case-insensitive) only, when it is not empty.
[[nodiscard]] std::vector<MenuEntry> history_entries(const std::vector<std::string>& history,
                                                     std::string_view filter = {}, std::size_t max = 12);

/// `line` with its last word replaced by `word` and a space (a completion
/// candidate chosen in the popover).
[[nodiscard]] std::string replace_last_word(std::string_view line, std::string_view word);

/// The completion popover (tile 1): one entry per candidate (at most
/// `max`, then "… N more"), choosing one puts the completed line in the
/// command bar.
[[nodiscard]] std::vector<MenuEntry> completion_entries(std::string_view line, const Completion& completion,
                                                        std::size_t max = 12);

// --- undo history ------------------------------------------------------------------

/// The undo history popover (board M2-Undo-Redo, tile 3): the redo tail
/// (nearest last, `redo N`), then the steps that can be undone (newest
/// first, `undo N`); each step is named after the user command that made it
/// ("paint metal1", UndoLog::step_label; an unlabelled step by its newest
/// event's description). `title` reads "24 steps · limit 1000".
struct UndoHistory {
  std::string title;
  std::vector<MenuEntry> entries;
};
[[nodiscard]] UndoHistory undo_history(const undo::UndoLog& log, std::size_t max_each = 8);

/// The status message after an undo or redo (tile 4): "Undid STEP · 23 more
/// steps" / "Redid STEP"; std::nullopt when nothing was undone or redone.
[[nodiscard]] std::optional<std::string> undo_status_message(bool redo, std::string_view step,
                                                             std::size_t remaining);

/// The name of the step `undo` would undo (`redo`: redo), its
/// UndoLog::step_label; empty when there is none.
[[nodiscard]] std::string next_undo_step(const undo::UndoLog& log, bool redo);

/// The undo log's position and step count, taken after each command so that
/// the next one can tell whether it moved the log (`undo`, `undo 3`, the
/// `u`/`U` keys, `paint m1; undo`, the undo history menu).
struct UndoMark {
  std::size_t position = 0;
  std::size_t steps = 0;
};
[[nodiscard]] UndoMark undo_mark(const undo::UndoLog& log) noexcept;

/// "Undid STEP · N more steps" / "Redid STEP" when the log moved back or
/// forward over existing steps since `before` (a new step is not a move);
/// std::nullopt otherwise. STEP is the first step the move passed (the
/// newest undone, the oldest redone); the log is printed only on a move.
[[nodiscard]] std::optional<std::string> undo_move_message(const UndoMark& before, const undo::UndoLog& log);

// --- layer palette and inspector menus ------------------------------------------------

/// The layer row context menu (board M2-Layer-Palette): each item shows the
/// command it issues: paint layer, paint and erase on the box, select on
/// the box, show or hide, lock or unlock.
[[nodiscard]] std::vector<MenuEntry> layer_menu(const LayerRow& row);

/// The inspector's orientation menu: Magic's eight orientations (`orient N`
/// ... `orient FW`), the current one checked.
[[nodiscard]] std::vector<MenuEntry> orient_menu(std::string_view current);

// --- key-binding sheet -------------------------------------------------------------------

/// One row of the key-binding sheet (board M2-Key-Bindings): the key's
/// display name, its command ("imacro: TEXT" for interactive macros), its
/// help, and the `macro TABLE KEY "TEXT"` line that edits it.
struct KeyBindingRow {
  std::string key;
  std::string command;
  std::string help;
  std::string edit;
};

/// The tables in Magic's order, labelled for the sheet ("Layout (box tool)").
struct KeyBindingTable {
  std::string name;   // the macro table (`layout`, `wiring`, ...)
  std::string label;  // shown in the TABLES column
};
[[nodiscard]] std::vector<KeyBindingTable> key_binding_tables(const ::hefesto::ui::MacroTables& macros);
/// The bindings of `table` containing `filter` in their key or command
/// (case-insensitive), in the table's order; deleted entries are skipped.
[[nodiscard]] std::vector<KeyBindingRow> key_binding_rows(const ::hefesto::ui::MacroTables& macros, std::string_view table,
                                                          std::string_view filter = {});

// --- color window screen ------------------------------------------------------------------

/// The desktop's color window screen (board M2-Color-Window): the chrome
/// (caption, "map modified", index in octal and decimal, value strip) and
/// the six bars with their pump commands. Every action is a core command:
/// pumps and bars are `setpoint X Y ID` + `pushbutton left` at the pump or
/// bar point in the color window (Magic's own path), the stepper `color N`.
struct ColorBarView {
  std::string name;   // "Red", ..., "Value"
  double value = 0.0;  // 0..1
  Rgba fill;          // the bar's display style color
  std::string minus;  // the "-" pump's command line
  std::string plus;   // the "+" pump's command line
};

struct ColorWindowView {
  int window = 0;
  int index = 0;
  std::string caption;     // "COLOR = 047"
  std::string octal;       // "047"
  std::string decimal;     // "39 decimal"
  bool modified = false;
  Rgba swatch;
  std::array<ColorBarView, 6> bars;
  std::string values;      // "R 184 G 73 B 83   H 0.985 S 0.603 V 0.722   #B84953"
  std::string previous;    // `color N-1` (empty at 0)
  std::string next;        // `color N+1` (empty at the last entry)
};

/// The view of the active color window (the most recently opened);
/// std::nullopt when no color window is open or no styles are loaded.
[[nodiscard]] std::optional<ColorWindowView> color_window_view(editor::EditorState& editor);

/// The command line of a click at `fraction` (0..1) along bar `bar` (0..5).
[[nodiscard]] std::string bar_click_line(editor::EditorState& editor, std::size_t bar, double fraction);

// --- panels and focus ------------------------------------------------------------------------

/// The regions F6 cycles through (board M2-Panels), in order; panels that
/// are hidden are skipped.
enum class FocusRegion : std::uint8_t { tools, layers, canvas, inspector, console, command };
struct FocusRegions {
  bool layers = true;
  bool inspector = true;
  bool console = false;
};
/// The region after `current` (Shift-F6: before), skipping hidden panels.
[[nodiscard]] FocusRegion next_focus_region(FocusRegion current, const FocusRegions& shown, bool backward) noexcept;

/// Where a docked panel puts its 1 px edge separator among its two
/// children (the edge and the panel body): on the canvas side, so a panel
/// docked left has it last (index 1) and one docked right first (index 0).
[[nodiscard]] int panel_edge_index(DockSide side) noexcept;

}  // namespace hefesto::shell
