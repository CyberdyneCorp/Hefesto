// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Commands of the MODEL area, registered by cmd::register_all_commands().
// The subset of Magic's editing commands that the M1 golden cases
// (tests/golden/MODEL/CASES.md) need to build layouts headless:
//   box, snap, paint, erase, splitpaint, getcell, flatten, label, port,
//   property, cellname, instance, array (minimal forms; full cmd-*
//   capabilities follow in M2).
// Handlers reach the session through CommandContext::editor().
//
// Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::db {

/// Adds the MODEL commands to `registry`. Returns false if a name was taken.
/// `getcell` and `flatten` are registered by the CELL area
/// (cell::register_cell_commands), which first reads the cells they need
/// from files and then calls the handlers below.
bool register_db_commands(cmd::CommandRegistry& registry);

/// `getcell name [orientation]` for a definition already in memory.
[[nodiscard]] cmd::CommandResult getcell_command(cmd::CommandContext& ctx, const cmd::Invocation& inv);
/// `flatten [options] name` over cells already in memory.
[[nodiscard]] cmd::CommandResult flatten_command(cmd::CommandContext& ctx, const cmd::Invocation& inv);

}  // namespace hefesto::db
