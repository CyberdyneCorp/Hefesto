// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `def read` option of the shared `lef`/`def` dispatcher (capability
// def-read "def read command", "Command context", "DEF file lookup";
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D14): its arguments, the
// file lookup, the read through the session's progress host, and the load
// of the design into the only layout window. It needs no window, box or
// selection (Q-LEFDEF-86), so the dispatcher calls it before its window
// check.
//
// Owner: DEFR (design D2, D14).

#pragma once

#include <span>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/def/read/read.hpp"

namespace hefesto::def {

/// The switches after the file name (lefCmd.c:224-250): an argument
/// beginning with `-label`, `-anno` (which implies labels) or `-noblock`
/// sets its flag, `-import` is accepted, and every other argument is
/// ignored silently.
[[nodiscard]] DefReadOptions parse_read_arguments(std::span<const std::string> after_name);

/// `def read <file> [options]`: `invocation.args` starts with the option
/// word ("read"). Without a file name it prints the `def` options.
[[nodiscard]] cmd::CommandResult read_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation);

}  // namespace hefesto::def
