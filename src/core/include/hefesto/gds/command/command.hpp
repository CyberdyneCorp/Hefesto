// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `gds` and `calma` commands (capabilities gds-read, gds-write;
// openspec/changes/m5-mask-gds-cif/design.md D3, D15): Magic's CmdCalma
// (commands/CmdCD.c:81-1600) with its option table; a bare `gds` writes.
// Called by cmd::register_all_commands.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); GDSW owns the
// command in src/core/src/gds/command/ (design D2).

#pragma once

#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::gds {

/// Magic's option table (cmdCalmaOption), in order; the name of each entry
/// is the text up to its first blank.
[[nodiscard]] std::span<const std::string_view> gds_options() noexcept;

/// Registers `gds` and `calma` in the layout client.
bool register_gds_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::gds
