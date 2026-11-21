// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `schematic` command and the `xschem` Tcl compatibility command
// (openspec/changes/m8-schematic design.md D12, "Area contracts" CMD;
// capability schematic-command "Command and option matching",
// "Subcommand table", "xschem compatibility command", Q-SCHEM-10).
//
// Neither is registered by the scaffold: CMD wires `schematic` into
// cmd::register_all_commands and `xschem` into the project interpreter
// when it implements them (an INTEGRATION open item).
//
// Owner: CMD.

#pragma once

#include <optional>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::schem {

struct Session;

/// Adds `schematic` to `registry`; CMD-REGISTRY-001 on a duplicate.
[[nodiscard]] std::optional<diag::Diagnostic> register_schematic_command(cmd::CommandRegistry& registry);

/// Defines the `xschem` command in `interpreter` over `session`: the
/// inventoried subcommands, and `xschem command '<cmd>' is not supported`
/// for any other. `session` must outlive the command.
void register_xschem_command(script::Interpreter& interpreter, Session& session);

}  // namespace hefesto::schem
