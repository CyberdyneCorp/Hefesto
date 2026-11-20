// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `lef` and `def` commands (capabilities lef-read, lef-write, def-read,
// def-write; openspec/changes/m6-lef-def-oasis/design.md D3, D14): one
// dispatcher for both, as Magic's CmdLef (lef/lefCmd.c:50-562). Both
// commands look their option up in the `lef` table (cmdLefOption), so `def
// nocheck` and `def datestamp` are found; the `def` table (cmdDefOption,
// with its split `read` entry) is used only by `def help`. `def writeall` is
// refused with Magic's message. Each other option calls its workstream's
// entry point. Called by
// cmd::register_all_commands.
//
// Owner: INTEGRATION (this header and the dispatcher in
// src/core/src/lef/command/); LEFR, LEFW, DEFR and DEFW own their argument
// parsers (design D2, D14).

#pragma once

#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::lef {

/// Magic's `lef` option table (cmdLefOption, lefCmd.c:152-169), in order:
/// read, write, writeall, nocheck, datestamp, help. The name of each entry
/// is the text up to its first blank; the help prints the whole entry.
[[nodiscard]] std::span<const std::string_view> lef_options() noexcept;

/// Magic's `def` option table (cmdDefOption, lefCmd.c:171-181), printed by
/// `def help` only. Its second entry continues the `read` entry, so its
/// text starts with blanks.
[[nodiscard]] std::span<const std::string_view> def_options() noexcept;

/// Registers `lef` and `def` in the layout client (DBWCommands.c:648-657).
bool register_lef_def_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::lef
