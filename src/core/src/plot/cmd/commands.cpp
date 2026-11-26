// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): `plot` is registered with
// its final name and fails with CORE-NOTIMPL-010, as does resolve_region.
//
// Owner: PLOTCMD.

#include "hefesto/plot/cmd/commands.hpp"

#include <utility>

#include "hefesto/cmd/command.hpp"
#include "hefesto/plot/not_implemented.hpp"

namespace hefesto::plot {

diag::Outcome<Region> resolve_region(const editor::EditorState& /*editor*/, const PlotRequest& /*request*/) {
  return not_implemented("plot::resolve_region");
}

bool register_plot_command(cmd::CommandRegistry& registry) {
  cmd::CommandSpec spec{
      "plot", "plot type [args]",
      "Writes a hardcopy plot of the box area as PostScript, PNM or SVG; `plot parameters` lists and sets the "
      "plot parameters (capability plot-output).",
      [](cmd::CommandContext& /*context*/, const cmd::Invocation& /*invocation*/) {
        return cmd::CommandResult::failure(not_implemented("plot"));
      }};
  return !registry.add(std::move(spec)).has_value();
}

}  // namespace hefesto::plot
