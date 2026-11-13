// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Commands of the CELL area, registered by cmd::register_all_commands():
//   load, save, writeall, path, addpath, locking, crashbackups
// (minimal forms needed by the M1 golden cases, tests/golden/CELL/CASES.md).
// Handlers reach the session through CommandContext::editor().
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <string_view>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::cell {

/// Adds the CELL commands to `registry`. Returns false if a name was taken.
bool register_cell_commands(cmd::CommandRegistry& registry);

/// Writes `def` to `file` with the session's cell I/O settings (DBCellWrite
/// through the command's I/O session), for commands of other areas that
/// save an internal cell under a file name: `select save` (cmd-selection
/// "Selection utility options"). Fails without an editor session or when
/// the write fails.
[[nodiscard]] cmd::CommandResult save_def_as(cmd::CommandContext& ctx, db::CellDef& def, std::string_view file);

/// The definition of cell `name` as DBWloadWindow finds it for a new window
/// (`openwindow NAME`): an existing one, else read through the search paths
/// (a directory part and `.mag` are dropped from the cell name, "Cell NAME
/// read from path ..."), else created empty ("Creating new cell"). Windows
/// and the edit cell are untouched (UI-G-033, UI-G-036).
[[nodiscard]] diag::Outcome<db::CellDef*> load_def(cmd::CommandContext& ctx, std::string_view name);

/// `cellname dereference NAME` (Magic's cmdFlushCell(def, TRUE) then
/// SelectClear): drops the stored file path of `def`, re-reads it through
/// the search paths and clears the selection, without asking.
[[nodiscard]] cmd::CommandResult dereference_cell(cmd::CommandContext& ctx, db::CellDef& def);

/// The layout client's exit check run by `quit` without -noprompt (DBWexit,
/// app-startup "quit command and exit status"): true when no cell is
/// modified or the user answers `yes` to `N Magic cell(s) has/have been
/// modified. Do you want to exit magic and lose it/them?`. A question that
/// cannot be asked (no prompt host, a headless host, a script host whose
/// `tk_dialog` gives no integer) quits with `magic_compat` ON, as Magic's -1
/// does, and keeps the session with it OFF (Q-SCRIPT-48).
[[nodiscard]] bool check_cells_written(editor::EditorState& editor, bool magic_compat);

}  // namespace hefesto::cell
