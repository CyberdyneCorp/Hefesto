// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `cif` command (capability cif-format-io "cif command dispatch";
// openspec/changes/m5-mask-gds-cif/design.md D3, D13). Called by
// cmd::register_all_commands.
//
// Owner: INTEGRATION (this header, created by the M5 scaffold); CIF owns the
// command in src/core/src/cif/command/ (design D2).

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::cif {

/// Registers `cif` in the layout client.
bool register_cif_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::cif
