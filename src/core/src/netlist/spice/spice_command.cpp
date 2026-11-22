// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `ext2spice` and `exttospice` commands (netlist/spice/spice.hpp;
// openspec/changes/m7-extraction/design.md D4, D14): Magic's option table,
// the first-word rule of Q-NETLIST-13 and `help`. The run and every other
// option answer CORE-NOTIMPL-008 until SPICE lands them.
//
// Ported from Magic 8.3.694 (fc5daa9e) ext2spice/ext2spice.c CmdExtToSpice
// (cmdExtToSpcOption, the lookup and the usage loop), Copyright (C) 1985,
// 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: INTEGRATION (the registration and dispatcher); SPICE (the option
// parsers, run() and render()).

#include <array>

#include "hefesto/cmd/m7_stubs.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/netlist/spice/spice.hpp"
#include "hefesto/script/pdk/commands.hpp"
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

constexpr std::size_t kScale = 8;
constexpr std::size_t kHelp = 16;

// M4's `ext2spice scale` (scripting-pdk-integration "PDK startup file
// contract" step 5, script/pdk/commands.hpp) stays implemented; SPICE takes
// it over with the rest of the options.
std::optional<cmd::CommandResult> implemented(cmd::CommandContext& ctx, const cmd::Invocation& invocation,
                                              std::size_t option) {
  if (option == kScale) {
    return script::ext2spice_scale(ctx, invocation);
  }
  return std::nullopt;
}

cmd::CommandResult spice_command(cmd::CommandContext& ctx, const cmd::Invocation& invocation) {
  const detail::TableCommand command{kOptions, kHelp,
                                     [](cmd::CommandContext& c, std::vector<std::string> args) {
                                       return run(*c.editor(), SpiceRun{std::move(args)});
                                     },
                                     implemented};
  return detail::dispatch(ctx, invocation, command);
}

}  // namespace

cmd::CommandResult run(editor::EditorState& /*editor*/, const SpiceRun& /*request*/) {
  return cmd::m7_not_implemented("ext2spice run");
}

diag::Outcome<std::string> render(editor::EditorState& /*editor*/, std::string_view /*cell*/,
                                  const SpiceSettings& /*settings*/) {
  return cmd::m7_not_implemented_diagnostic("ext2spice render");
}

std::string format_si(double /*value*/) {
  return {};
}

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
