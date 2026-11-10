// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Dispatch, input and script-helper commands (capabilities
// ui-command-dispatch, ui-tools-and-input, ui-box-stack-and-measure; the
// `tool` command of ui-gui-managers), registered by
// cmd::register_all_commands():
//   help (replaces the M0 built-in; global and per-client listings, wizard)
//   macro, imacro, tool, cursor, logcommands, pushbutton, setpoint, sleep,
//   send, history, source, *bypass, pushbox, popbox, peekbox, pushstack,
//   popstack, clearstack, suspendall, resumeall, ruler, measure, unmeasure,
//   genkey, autoincr, the pick tool helpers keepselect,
//   startselect, cancelselect, plus `netlist` (unavailable until M12).
// `echo` stays the M0 built-in (Magic's Tcl build has no echo command).
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <span>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

/// Adds the ui commands. Returns false if a name was taken.
bool register_ui_commands(cmd::CommandRegistry& registry);

/// `macro`/`imacro` on `editor` for the client `client` (the logic of the
/// commands, shared with the key-binding file reader). Prints through
/// UI-REPORT-001 diagnostics in the result.
[[nodiscard]] cmd::CommandResult macro_command(editor::EditorState& editor, std::span<const std::string> args,
                                               bool interactive, std::string_view client = "layout");

}  // namespace hefesto::ui
