// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Selection commands (capability cmd-selection), registered by
// cmd::register_all_commands():
//   select, what, identify, id, delete, copy, move, stretch, clockwise,
//   rotate, sideways, upsidedown, orient, array
// `array` moved here from the db area at the M2 scaffold (design.md D3) and
// acts on the selection (Q-CMDEDIT-63).
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::select {

/// Adds the selection commands. Returns false if a name was taken.
bool register_select_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::select
