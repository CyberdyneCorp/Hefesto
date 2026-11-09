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

#include "hefesto/cmd/registry.hpp"

namespace hefesto::cell {

/// Adds the CELL commands to `registry`. Returns false if a name was taken.
bool register_cell_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::cell
