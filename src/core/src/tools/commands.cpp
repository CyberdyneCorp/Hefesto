// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/tools/commands.hpp"

#include "hefesto/cmd/context.hpp"
#include "hefesto/tools/diag_codes.hpp"
#include "hefesto/tools/readspice.hpp"

namespace hefesto::tools {

namespace {

using cmd::CommandContext;
using cmd::CommandResult;
using cmd::Invocation;

// Tcl's error for a procedure called with the wrong number of arguments.
CommandResult wrong_args(std::string_view usage) {
  return CommandResult::raised(diag::make(codes::kUsage, "wrong # args: should be \"" + std::string{usage} + "\""));
}

// `readspice netfile` (readspice.tcl:16).
CommandResult cmd_readspice(CommandContext& ctx, const Invocation& inv) {
  if (inv.args.size() != 1) {
    return wrong_args("readspice netfile");
  }
  ReadSpiceOptions options;
  const bool magic_compat = ctx.project().compat.magic_compat;
  options.continuation = magic_compat ? ContinuationMode::magic : ContinuationMode::fixed;
  options.magic_bus_brackets = magic_compat;
  return readspice(ctx, inv.args[0], options);
}

// Q-SCRIPT-30: mazeroute drives `iroute` (router-maze-iroute, M10).
CommandResult cmd_mazeroute(CommandContext& /*ctx*/, const Invocation& /*inv*/) {
  return CommandResult::failure(diag::make(
      codes::kMazeRoute, "mazeroute is not available yet: it arrives with the maze router iroute "
                         "(router-maze-iroute, M10)"));
}

}  // namespace

bool register_tools_commands(cmd::CommandRegistry& registry) {
  // Tcl procedures in Magic (readspice.tcl, mazeroute.tcl): exact-name table cmd::kScriptClient.
  bool ok = !registry
                 .add({"readspice", "readspice <netlist>",
                       "Annotate the loaded cells with the port order and directions of a SPICE, CDL or Spectre "
                       "netlist.",
                       cmd_readspice},
                      cmd::kScriptClient)
                 .has_value();
  ok = !registry
            .add({"mazeroute", "mazeroute <netfile>",
                  "Route the nets of a netlist file with the maze router (arrives with router-maze-iroute, M10).",
                  cmd_mazeroute},
                 cmd::kScriptClient)
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::tools
