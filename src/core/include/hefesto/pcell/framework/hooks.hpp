// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The top-level statements of toolkit.tcl, which Magic sources in every
// Tcl session (Magic ref: tcltk/magic.tcl.in:264-272, tcltk/toolkit.tcl
// :107-117; capability pcell-toolkit "Selection-driven dialog refresh and
// key binding"):
//   - `magic::macro ^P "magic::gencell {} ; raise .params"` — the text is
//     kGencellKeyMacro; the binding itself is the first entry of the
//     startup bindings (ui/bindings.cpp), because Magic defines it before
//     the system startup file and the macro table's listing order depends
//     on that;
//   - `magic::tag add select "magic::gencell_update %1"` — install_toolkit;
// and Tk's `raise` for the parameter dialog `.params`, which the ^P macro
// and gencell_dialog run (raise_window; the command `raise`).
//
// Owner: PCELL (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/pcell/framework/state.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::pcell {

/// The Control-P binding of layout windows (toolkit.tcl:111).
inline constexpr std::string_view kGencellKeyMacro = "magic::gencell {} ; raise .params";

/// Tk's `raise window ?aboveThis?` (Tk 8.6 tkCmds.c Tk_RaiseObjCmd) for the
/// framework's window: `.params` raises the open parameter dialog
/// (DialogModel::raise_count). Hefesto has no other Tk window, so any other
/// path, and `.params` while the dialog is closed, is Tk's
/// `bad window path name "<path>"` error.
[[nodiscard]] cmd::CommandResult raise_window(FrameworkState& state, const std::vector<std::string>& args);

/// Installs toolkit.tcl's interpreter hooks in `interp`, the project
/// interpreter of `editor` after script::bind_registry: the `select` tag
/// that refreshes an open dialog, and the global Tcl command `::raise`
/// (raise_window on editor.script.gencell). Run it once per interpreter,
/// before any startup or PDK script (the PDK widget interception then keeps
/// this `raise` instead of skipping the statement).
cmd::CommandResult install_toolkit(script::Interpreter& interp, editor::EditorState& editor);

}  // namespace hefesto::pcell
