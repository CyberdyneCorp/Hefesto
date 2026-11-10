// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `logcommands` log (capability ui-command-dispatch, requirement
// "Command logging"; Tcl-build file format, Q-UI-14). The dispatcher calls
// log_command() after every command it runs (nested commands, such as the
// ones `source` or `*bypass` run, are not logged); the log file is written
// through the platform FileSystem.
// (Magic ref: textio/txCommands.c:703-992, windows/windCmdAM.c:917-996)
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

/// The log line of one command: its words joined by single spaces, a word
/// containing a space wrapped in `"`; empty for the commands never logged
/// (`logc...`, `*bypass`, `setpoint`, `wire show`).
[[nodiscard]] std::string log_line(const cmd::Invocation& invocation);

/// Appends `invocation` to the open log (followed by `updatedisplay` in
/// update mode); does nothing without an open, unsuspended log.
void log_command(editor::EditorState& editor, platform::FileSystem& files, const cmd::Invocation& invocation);

/// Appends a button event line `pushbutton left|middle|right down|up`.
void log_button(editor::EditorState& editor, platform::FileSystem& files, std::string_view button,
                std::string_view action);

}  // namespace hefesto::ui
