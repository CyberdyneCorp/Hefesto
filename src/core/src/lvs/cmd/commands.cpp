// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): `lvs` is registered with its final name
// and fails with CORE-NOTIMPL-009, as do layout_circuit and cross_probe.
//
// Owner: LVSCMD.

#include "hefesto/lvs/cmd/commands.hpp"

#include <utility>

#include "hefesto/cmd/command.hpp"
#include "hefesto/lvs/not_implemented.hpp"

namespace hefesto::lvs {

bool register_lvs_commands(cmd::CommandRegistry& registry) {
  cmd::CommandSpec spec{
      "lvs",
      "lvs <name1> <name2> [<setup> [<log>]] [-list] [-json] [-blackbox] [-noflatten=<file-or-list>]",
      "Compares two netlists (layout versus schematic) as netgen's lvs does (capability lvs-command).",
      [](cmd::CommandContext& /*context*/, const cmd::Invocation& /*invocation*/) {
        return cmd::CommandResult::failure(not_implemented("lvs"));
      }};
  return !registry.add(std::move(spec)).has_value();
}

diag::Outcome<Circuit> layout_circuit(editor::EditorState& /*editor*/, std::string_view /*cell*/,
                                      const netlist::spice::SpiceSettings& /*settings*/) {
  return not_implemented("lvs::layout_circuit");
}

diag::Outcome<std::vector<LayoutRef>> cross_probe(const LvsResult& /*result*/, const ObjectRef& /*object*/) {
  return not_implemented("lvs::cross_probe");
}

}  // namespace hefesto::lvs
