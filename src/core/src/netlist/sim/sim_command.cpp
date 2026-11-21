// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2sim` and `exttosim` commands (netlist/sim/sim.hpp;
// openspec/changes/m7-extraction/design.md D4, D15): Magic's option table,
// the first-word rule and `help`. The run and every other option answer
// CORE-NOTIMPL-008 until SIM lands them.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2sim/ext2sim.c CmdExtToSim
// (cmdExtToSimOption, the lookup and the usage loop), Copyright (C) 1985,
// 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration and dispatcher); SIM (the option
// parsers and run()).

#include <array>

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/netlist/sim/sim.hpp"
#include "netlist/table_command.hpp"

namespace hefesto::netlist::sim {

namespace {

// cmdExtToSimOption, in Magic's order (EXTTOSIM_RUN .. EXTTOSIM_HELP).
constexpr std::array<std::string_view, 10> kOptions{
    "[run] [options]\trun exttosim on current cell\n\t\t\tuse \"run -help\" to get standard options",
    "alias on|off\t\tenable/disable alias (.al) file",
    "labels on|off\t\tenable/disable labels (.nodes) file",
    "default\t\treset to default values",
    "format MIT|SU|LBL\tset output format",
    "rthresh [value]\tset resistance threshold value",
    "cthresh [value]\tset capacitance threshold value",
    "merge [option]\t\tmerge parallel transistors",
    "extresist on|off\tincorporate extresist output",
    "help\t\t\tprint help information",
};

constexpr std::size_t kHelp = 9;

cmd::CommandResult sim_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  const detail::TableCommand command{kOptions, kHelp, [](cmd::CommandContext& c, std::vector<std::string> args) {
                                       return run(*c.editor(), SimRun{std::move(args)});
                                     },
                                     {}};
  return detail::dispatch(ctx, invocation, command);
}

}  // namespace

cmd::CommandResult run(editor::EditorState& /*editor*/, const SimRun& /*request*/) {
  return cmd::m7_not_implemented("ext2sim run");
}

std::span<const std::string_view> sim_options() noexcept {
  return kOptions;
}

// ext2sim.c Exttosim_Init: "exttosim" and its alias "ext2sim".
bool register_sim_commands(cmd::CommandRegistry& registry) {
  bool ok = !registry
                 .add(cmd::CommandSpec{"ext2sim", "ext2sim [args]",
                                       "convert extracted file(s) to a sim format file; type \"ext2sim help\" for "
                                       "information on options",
                                       sim_command},
                      cmd::kLayoutClient)
                 .has_value();
  ok = !registry
            .add(cmd::CommandSpec{"exttosim", "exttosim [args]",
                                  "convert extracted file(s) to a sim format file; type \"exttosim help\" for "
                                  "information on options",
                                  sim_command},
                 cmd::kLayoutClient)
            .has_value() &&
       ok;
  return ok;
}

}  // namespace hefesto::netlist::sim
