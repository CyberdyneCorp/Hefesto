// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the HIER-owned MODEL commands (commands_labels.cpp,
// commands_cells.cpp): pieces one command group lends to the other.
// Owner: HIER (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string>
#include <vector>

#include "db/command_util.hpp"

namespace hefesto::db::cmdutil {

/// CmdDoProperty on `def` (`property ...` and `cellname property NAME ...`;
/// cmd-labels-properties "Property command"). `args` are the words after
/// the command (or after the cell name).
[[nodiscard]] cmd::CommandResult property_on(const Session& s, CellDef& def, const std::vector<std::string>& args);

/// The target of `property`: the current window's root cell, else the edit
/// cell (Q-CMDEDIT-93); nullptr when there is neither.
[[nodiscard]] CellDef* property_target(const Session& s);

}  // namespace hefesto::db::cmdutil
