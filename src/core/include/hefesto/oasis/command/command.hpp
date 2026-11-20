// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `oasis` command (capability oasis-command;
// openspec/changes/m6-lef-def-oasis/design.md D3, D15): `oasis`, `oasis
// read <file>`, `oasis write <file>`, `oasis help` and one option per row of
// "Options shared with the gds command" and "OASIS-only options". A bare
// `oasis` writes the root cell of the current window. `oasis` has no alias.
// Options match by unique prefix, as for `gds`; an unknown or ambiguous
// option is an argument error (oasis-command "Option syntax"). Called by
// cmd::register_all_commands.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); OASCMD owns
// the command in src/core/src/oasis/command/ (design D2).

#pragma once

#include <span>
#include <string_view>

#include "hefesto/cmd/registry.hpp"

namespace hefesto::oasis {

/// The option table: `help`, the `gds` options in the order of Magic's
/// cmdCalmaOption (with `read` and `write`), then the OASIS-only options
/// strict, validation, repetitions, layernames and circlepoints. The name
/// of each entry is the text up to its first blank; `oasis help` prints the
/// whole entry.
[[nodiscard]] std::span<const std::string_view> oasis_options() noexcept;

/// Registers `oasis` in the layout client.
bool register_oasis_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::oasis
