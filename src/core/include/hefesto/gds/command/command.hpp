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

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/compat/compat_settings.hpp"

namespace hefesto::gds {

/// Magic's option table (cmdCalmaOption), in order; the name of each entry
/// is the text up to its first blank.
[[nodiscard]] std::span<const std::string_view> gds_options() noexcept;

/// The output file of `gds write <name>` (gds-write "Write command and
/// output file naming"): `name`, with `.gds` (compression 0) or `.gds.gz`
/// (1-9) appended when it has no extension. With magic-compat ON a `.`
/// anywhere in `name`, directories included, counts as an extension, as in
/// Magic; with it OFF only the last path component is tested (Q-MASK-210).
[[nodiscard]] std::string output_file_name(std::string_view name, std::int32_t compress,
                                           const compat::CompatSettings& compat);

/// Registers `gds` and `calma` in the layout client.
bool register_gds_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::gds
