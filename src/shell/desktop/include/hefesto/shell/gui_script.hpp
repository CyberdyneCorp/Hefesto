// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// How an M4 screen action runs (openspec/changes/m4-scripting/design.md
// D13 "Commands only"): menu items, Place device and the PCell dialog run
// Tcl scripts written by Magic's wrapper or by PDK scripts
// (`sky130::importspice`, `set ::magic::w_val 1.0 ; magic::update_dialog
// ...`), so they are evaluated in the project's interpreter, where every
// engine command goes through the Dispatcher and is logged. Without an
// interpreter (no project open) the line goes through the Dispatcher's
// M0 reader instead.
//
// Owner: SHELL (design D2).

#pragma once

#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"

namespace hefesto::cmd {
class Dispatcher;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::shell {

/// Runs `script` with `origin`: `gui` for the shell's own actions, `script`
/// for text a PDK supplied (its menu items and device entries), so that the
/// sandbox's script rules apply to it (security finding S-01).
[[nodiscard]] cmd::CommandResult run_gui_script(editor::EditorState& editor, cmd::Dispatcher& dispatcher,
                                                std::string_view script, cmd::Origin origin = cmd::Origin::gui);

/// The text a result shows in the Console: its value, else its first diagnostic.
[[nodiscard]] std::string gui_result_text(const cmd::CommandResult& result);

}  // namespace hefesto::shell
