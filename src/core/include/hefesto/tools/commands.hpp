// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The commands of the tools area (openspec/changes/m4-scripting/design.md
// D3/D11): `readspice` and `mazeroute` (Q-SCRIPT-30: a TOOLS-* diagnostic
// naming router-maze-iroute, M10). The auxiliary tools of
// scripting-auxiliary-tools map to hefesto-cli options (TOOLS task 8.3).
// Called by cmd::register_all_commands.
//
// Owner: TOOLS (design D2).

#pragma once

#include "hefesto/cmd/registry.hpp"

namespace hefesto::tools {

/// Registers `readspice` (readspice()) and `mazeroute` (TOOLS-MAZE-001
/// until M10) in cmd::kScriptClient.
bool register_tools_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::tools
