// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` and `exttospice` commands (netlist/spice/spice.hpp;
// openspec/changes/m7-extraction/design.md D4, D14): Magic's option table,
// the first-word rule of Q-NETLIST-13, `help`, the options (options.cpp)
// and the run (run.cpp). M4's partial `ext2spice scale` is folded into the
// `scale` option (user decision M7-3).
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c CmdExtToSpice
// (cmdExtToSpcOption, the lookup and the usage loop), Copyright (C) 1985,
// 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration and dispatcher); SPICE (the option
// parsers, run() and render()).

#include <array>

#include "db/command_util.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "netlist/spice/options.hpp"
#include "netlist/spice/run_impl.hpp"
#include "netlist/table_command.hpp"

namespace hefesto::netlist::spice {

namespace {

// cmdExtToSpcOption, in Magic's order (EXTTOSPC_RUN .. EXTTOSPC_HELP).
constexpr std::array<std::string_view, 17> kOptions{
    "[run] [options]\trun exttospice on current cell\n\t\t\tuse \"run -help\" to get standard options",
    "default\t\treset to default values",
    "format [<type>]\tset output format",
    "rthresh [<value>]\tset resistance threshold value",
    "cthresh [<value>]\tset capacitance threshold value",
    "merge [<type>]\t\tmerge parallel transistors",
    "extresist [on|off]\tincorporate information from extresist",
    "resistor tee [on|off]\tmodel resistor capacitance as a T-network",
    "scale [on|off]\t\tuse .option card for scaling",
    "short [voltage|resistor|none]\n\t\t\tset method for handling shorted ports",
    "subcircuits [top|descend] [on|off|auto]\n\t\t\tstandard cells become subcircuit calls",
    "hierarchy [on|off]\toutput hierarchical spice for LVS",
    "blackbox [on|off]\toutput abstract views as black-box entries",
    "renumber [on|off]\ton = number instances X1, X2, etc.\n\t\t\toff = keep instance ID names",
    "global [on|off]\ton = merge unconnected global nets by name",
    "lvs    \t\tapply typical default settings for LVS",
    "help\t\t\tprint help information",
};

constexpr std::size_t kHelp = 16;

cmd::CommandResult run_command(cmd::CommandContext& ctx, std::vector<std::string> args) {
  auto session = db::cmdutil::session(ctx);
  if (!session) {
    return db::cmdutil::fail(session.error());
  }
  compat::CompatSettings compat;
  compat.magic_compat = session.value().magic_compat;
  return detail::run_session(*session.value().editor, args, compat, ctx.services().files);
}

cmd::CommandResult spice_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  const netlist::detail::TableCommand command{
      kOptions, kHelp, run_command,
      [](cmd::CommandContext& c, const cmd::Invocation& inv, std::size_t option) {
        return detail::spice_option(c, inv, option, kOptions);
      }};
  return netlist::detail::dispatch(ctx, invocation, command);
}

}  // namespace

std::span<const std::string_view> spice_options() noexcept {
  return kOptions;
}

// ext2spice.c Exttospice_Init: "exttospice" and its alias "ext2spice".
bool register_spice_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"ext2spice", "ext2spice [args]",
                                       "convert extracted file(s) to a SPICE format file; type \"ext2spice help\" "
                                       "for information on options",
                                       spice_command},
                      cmd::kLayoutClient)
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"exttospice", "exttospice [args]",
                                  "convert extracted file(s) to a SPICE format file; type \"exttospice help\" for "
                                  "information on options",
                                  spice_command},
                 cmd::kLayoutClient)
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::netlist::spice
