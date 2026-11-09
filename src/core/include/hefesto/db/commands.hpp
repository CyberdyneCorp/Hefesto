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

#include "hefesto/cmd/registry.hpp"

namespace hefesto::db {

/// Adds the MODEL commands to `registry`. Returns false if a name was taken.
bool register_db_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::db
