// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The commands of the binding area (openspec/changes/m4-scripting/design.md
// D3/D6): `tag` (scripting-api "Command tag callbacks"). Called by
// cmd::register_all_commands. The M4 `cif` command moved to cif/command/
// with the M5 scaffold (openspec/changes/m5-mask-gds-cif/design.md D3).
//
// Owner: BINDING (design D2).

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::script {

/// Registers `tag`.
bool register_binding_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::script
