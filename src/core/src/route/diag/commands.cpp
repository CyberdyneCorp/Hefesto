// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): `*iroute` and `*mzroute`
// are registered with their final names and fail with CORE-NOTIMPL-010
// until IROUTE (section 11) lands them.
//
// Owner: IROUTE.

#include "hefesto/route/diag/commands.hpp"

#include <string>
#include <utility>

#include "hefesto/cmd/command.hpp"
#include "hefesto/route/not_implemented.hpp"

namespace hefesto::route {

namespace {

bool add_stub(cmd::CommandRegistry& registry, std::string name, std::string usage, std::string summary) {
  std::string what = name;
  cmd::CommandSpec spec{std::move(name), std::move(usage), std::move(summary),
                        [what](cmd::CommandContext& /*context*/, const cmd::Invocation& /*invocation*/) {
                          return cmd::CommandResult::failure(not_implemented(what));
                        }};
  return !registry.add(std::move(spec)).has_value();
}

}  // namespace

bool register_route_diag_commands(cmd::CommandRegistry& registry) {
  bool ok = add_stub(registry, "*iroute", "*iroute [cmd [args]]",
                     "Debugging commands of the interactive router (capability router-maze-iroute).");
  ok = add_stub(registry, "*mzroute", "*mzroute [cmd [args]]",
                "Debugging commands of the maze router (capability router-maze-iroute).") &&
       ok;
  return ok;
}

}  // namespace hefesto::route
