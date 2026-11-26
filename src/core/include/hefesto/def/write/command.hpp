// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `def write` (capability def-write "def write command", "def command
// argument validation"; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D14):
// the argument parser the shared `lef`/`def` dispatcher calls for `def
// write`, which finds its own target cell as CmdLef does.
//
// Owner: DEFW (design D2, D14).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"

namespace hefesto::def {

/// Runs `def write [filename] [-units <n>] [-allspecial] [-anal]`
/// (`invocation.args` starts with the option word). The target cell is the
/// selected cell, else the current window's root, else the box's cell
/// ("Point to a window first" when none). Arguments are matched by leading
/// text as in CmdLef: a misplaced word, an unknown option or a wrong count
/// prints the `def` help and writes nothing; the `lef write` options print
/// "only for lef write"; `-units` takes the next word, a non-integer
/// leaving the units at 1000 and a value <= 0 failing (Q-LEFDEF-104 fix).
[[nodiscard]] cmd::CommandResult write_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation);

}  // namespace hefesto::def
