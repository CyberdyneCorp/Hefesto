// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 5.2): `schematic` and `xschem` answer
// CORE-NOTIMPL-007 for every call. Neither is registered yet.
//
// Owner: CMD.

#include "hefesto/schem/cmd/commands.hpp"

#include "hefesto/script/interp.hpp"
#include "hefesto/schem/not_implemented.hpp"

namespace hefesto::schem {

std::optional<diag::Diagnostic> register_schematic_command(cmd::CommandRegistry& registry) {
  return registry.add(cmd::CommandSpec{
      "schematic", "schematic option ?arg ...?", "Schematic capture (capability schematic-command).",
      [](cmd::CommandContext& /*ctx*/, const cmd::Invocation& /*inv*/) {
        return cmd::CommandResult::failure(not_implemented("schematic"));
      }});
}

void register_xschem_command(script::Interpreter& interpreter, Session& /*session*/) {
  interpreter.define_command("xschem", [](const cmd::Invocation& /*inv*/) {
    return cmd::CommandResult::failure(not_implemented("xschem"));
  });
}

}  // namespace hefesto::schem
