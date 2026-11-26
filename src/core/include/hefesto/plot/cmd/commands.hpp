// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `plot` command and the plotted region (capability plot-output "Plot
// command and plotted region"; openspec/changes/m10-plot-route/design.md
// D4, D7, "Area contracts" PLOTCMD). The option set is Q-MISC-51 (a):
// postscript, pnm, svg, parameters, help.
//
// Until PLOTCMD lands, `plot` fails with CORE-NOTIMPL-010.
// cmd::register_all_commands calls register_plot_command.
//
// Owner: PLOTCMD (the `svg` case: EXPORT, by agreement, design D4).

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::plot {

/// The output format of a `plot` invocation.
enum class Format : std::uint8_t { postscript, pnm, svg };

/// What a `plot` invocation asked for, after argument parsing.
struct PlotRequest {
  Format format = Format::postscript;
  std::string file;          // as typed; the extension rule is plot/io's
  std::string layers;        // the layer list as typed; empty = the window's visible layers
  int pnm_width = 1000;      // `plot pnm` pixel width (Magic's default)
};

/// The plotted region: the root cell, the area in root coordinates and
/// what is drawn (design D7).
struct Region {
  const db::CellDef* root = nullptr;  // not owned
  db::Rect area;
  tech::TypeMask layers;   // the paint types plotted
  bool labels = false;     // labels shown and dbw_labelSize >= 0
  bool subcells = true;    // subcell outlines (always, as Magic)
};

/// What a writer needs besides the region, the technology and the
/// parameters (design D4, D16).
struct PlotContext {
  bool magic_compat = true;
  const std::atomic<bool>* cancel = nullptr;  // the interrupt token; nullptr = not cancellable
  std::function<void(const std::string&)> progress;  // progress lines; empty = none
};

/// The window, area and layer mask of `request` ("Plot command and plotted
/// region").
[[nodiscard]] diag::Outcome<Region> resolve_region(const editor::EditorState& editor, const PlotRequest& request);

/// Adds `plot` to `registry`; false on a duplicate (CMD-REGISTRY-001).
[[nodiscard]] bool register_plot_command(cmd::CommandRegistry& registry);

}  // namespace hefesto::plot
