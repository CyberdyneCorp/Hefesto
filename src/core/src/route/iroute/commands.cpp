// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): `iroute` is registered
// with its final name and fails with CORE-NOTIMPL-010, as does
// save_parameters, until IROUTE (section 11) lands them.
//
// Owner: IROUTE.

#include "hefesto/route/iroute/commands.hpp"

#include <utility>

#include "hefesto/cmd/command.hpp"
#include "hefesto/route/not_implemented.hpp"

namespace hefesto::route {

bool register_iroute_commands(cmd::CommandRegistry& registry) {
  cmd::CommandSpec spec{
      "iroute", "iroute [cmd [args]]",
      "Routes point to point with the maze router and sets its parameters (capability router-maze-iroute).",
      [](cmd::CommandContext& /*context*/, const cmd::Invocation& /*invocation*/) {
        return cmd::CommandResult::failure(not_implemented("iroute"));
      }};
  return !registry.add(std::move(spec)).has_value();
}

diag::Status save_parameters(const IrouteSession& /*session*/, const tech::Technology& /*tech*/,
                             bool /*magic_compat*/, std::ostream& /*out*/) {
  return not_implemented("route::save_parameters");
}

}  // namespace hefesto::route
