// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Tcl-visible GUI manager commands (openspec/changes/m4-scripting/
// design.md D3/D8/D12): `drc_save_report`, `drc_load_report` and the entry
// commands of the wrapper and managers (gui_entry_command_names()), under
// the `magic::` names of wrapper.tcl, cellmgr.tcl, libmgr.tcl,
// drcmgr.tcl, texthelper.tcl and toolbar.tcl. Called by
// cmd::register_all_commands.
//
// Owner: GUIMGR (design D2).

#pragma once

#include <string>
#include <vector>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::gui {

/// The wrapper and manager entry commands, in the order of Magic's scripts.
[[nodiscard]] const std::vector<std::string>& gui_entry_command_names();

/// Registers `drc_save_report`, `drc_load_report` and
/// gui_entry_command_names(). SCAFFOLD: every command is a CORE-NOTIMPL-004
/// stub.
bool register_gui_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::gui
