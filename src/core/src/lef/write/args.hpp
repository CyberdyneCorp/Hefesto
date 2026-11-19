// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lef write`, `lef writeall` and `lef nocheck` options (capability
// lef-write "Command dispatch and target cell", "lef write argument
// grammar", "Distance option values", "lef writeall arguments and scope",
// "lef nocheck net list", "Help text"; design D14): their argument parsers,
// called by the lef/def dispatcher (src/core/src/lef/command/command.cpp)
// for the `lef` command (`def nocheck` and `def writeall` are answered by
// the dispatcher).
//
// Owner: LEFW (design D2, D14).

#pragma once

#include <cstdint>

#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/command.hpp"

namespace hefesto::lef {

enum class WriteCommand : std::uint8_t { write, write_all, nocheck };

/// Runs `lef write`, `lef writeall` or `lef nocheck` (`invocation.args`
/// starts with the option word). Finds the target cell as CmdLef does (the
/// selected cell, else the current window's root, else the box's cell;
/// "Point to a window first" when none), parses the arguments with
/// Magic's leading-text matching (a usage error prints the help) and calls
/// lef::write or lef::write_all.
[[nodiscard]] cmd::CommandResult run_write_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                                   WriteCommand which);

}  // namespace hefesto::lef
