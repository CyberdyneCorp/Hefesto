// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `undo` and `redo` commands (layout-undo "undo command", "redo
// command"), registered by cmd::register_all_commands().
//
// Both read the magic-compat setting for `undo 0`/`redo 0` (D-11).
//
// Owner: UNDO (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::undo {

/// Adds `undo` and `redo`. Returns false if a name was taken.
bool register_undo_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::undo
