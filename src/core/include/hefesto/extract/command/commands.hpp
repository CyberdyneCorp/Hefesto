// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `extract` command and its alias `ext` (capability extract-commands
// "Command syntax and option matching"; openspec/changes/m7-extraction
// design.md D4, D12): Magic's CmdExtract option tables (commands/CmdE.c,
// cmdExtCmd, cmdExtOption, cmdExtWarn, cmdExtLength, cmdExtUniq), the
// leading `list`/`listall` word, Magic's option lookup and its
// invalid-option message, the window check and `extract help`. Called by
// cmd::register_all_commands.
//
// Every option is implemented; none reports CORE-NOTIMPL-008 any more.
//
// Owner: INTEGRATION (the registration and dispatcher); EXTCMD (the option
// parsers, design D3, D4).

#pragma once

#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::extract {

/// cmdExtCmd, in Magic's order: all, cell, do, halo, help, length, no,
/// parents, path, showparents, stepsize, style, unique, warn. The name of
/// each entry is its text up to the first blank; `extract help` prints the
/// whole entry.
[[nodiscard]] std::span<const std::string_view> extract_options() noexcept;

/// cmdExtOption (`extract do|no OPT`), in Magic's order.
[[nodiscard]] std::span<const std::string_view> extract_do_options() noexcept;

/// cmdExtWarn (`extract warn [no] OPT`).
[[nodiscard]] std::span<const std::string_view> extract_warn_options() noexcept;

/// cmdExtLength (`extract length OPT`).
[[nodiscard]] std::span<const std::string_view> extract_length_options() noexcept;

/// cmdExtUniq (`extract unique [OPT]`).
[[nodiscard]] std::span<const std::string_view> extract_unique_options() noexcept;

/// Registers `extract` and `ext` in the layout client
/// (DBWcommands.c "extract option" and "ext option ... OR").
bool register_extract_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::extract
