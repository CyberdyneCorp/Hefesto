// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The ui area's session state beyond the EditorState members of design.md
// D4 (`macros`, `tools`, `box_stack`, `edit_stack`, `suspend_depth`,
// `command_point`, `command_count`): the client of the running command, the
// cursor glyph, the box tool's button state, the drag state of the scripted
// tools, the command history, the command log, the ruler and key counters,
// the interrupt flag, the sleeper and the client update handlers.
//
// It is the EditorState member `ui`, reached through ui::state(editor).
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/ui/history.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

/// Pointer buttons of "pushbutton and setpoint" (Magic's TX_*_BUTTON).
enum class Button : std::uint8_t { left = 1, middle = 2, right = 4 };
/// A box corner of the box tool (Magic's TOOL_BL ...).
enum class Corner : std::uint8_t { bl, br, tl, tr };

/// "Box tool buttons": the buttons held and the chosen corner.
struct BoxToolState {
  std::uint8_t held = 0;  // Button bits
  Corner corner = Corner::bl;
};

/// `logcommands` (Magic's txLogFile and txLogFlags).
struct CommandLogState {
  std::optional<std::string> path;  // the open log file
  bool update = false;              // `updatedisplay` after each logged command
  bool suspended = false;
};

struct UiState {
  /// The client the running command was sent to (`layout` by default; the
  /// dispatcher and `send` set it). `help` and `macro` read it.
  std::string client = "layout";
  /// The cursor glyph shown by the front end (`cursor N`, tools, buttons).
  int cursor_glyph = 0;
  BoxToolState box_tool;
  /// Non-empty while a scripted tool drag is in progress (tools.tcl's
  /// Opts(motion)): `tool` refuses to switch. The front end runs it on each
  /// pointer motion.
  std::string motion;
  /// The pick tool's drag origin (Opts(origin)), internal units.
  db::Point drag_origin;
  CommandHistory history;
  CommandLogState log;
  /// tools.tcl Opts(rulers) / Opts(keys): -1 until the first ruler / key.
  int rulers = -1;
  int keys = -1;
  /// A point forced by `setpoint`: kept for the rest of a script, dropped
  /// after the next interactive command.
  bool point_forced = false;
  bool point_sticky = false;
  /// The window pixel of the forced point (`setpoint` reports it).
  db::Point pointer_pixel;
  /// Set by Ctrl-C (ui::request_interrupt); the dispatcher reports
  /// `[Interrupted]` after the running command and clears it.
  std::atomic<bool> interrupt_pending{false};
  /// Waits for the given time; returns false when interrupted. Tests
  /// replace it; the default sleeps in short steps watching the interrupt.
  std::function<bool(std::chrono::milliseconds)> sleeper;
  /// Client update handlers run by the post-command update, in order.
  std::vector<std::function<void(editor::EditorState&)>> update_handlers;
};

/// The ui session state of `editor`.
[[nodiscard]] UiState& state(editor::EditorState& editor);
[[nodiscard]] const UiState& state(const editor::EditorState& editor);

/// Requests an interrupt (Ctrl-C). Returns true when one was already
/// pending (the second Ctrl-C, which exits the program in Magic).
bool request_interrupt(editor::EditorState& editor);

/// Runs every client update handler (step 3 of the post-command update).
void run_client_updates(editor::EditorState& editor);

}  // namespace hefesto::ui
