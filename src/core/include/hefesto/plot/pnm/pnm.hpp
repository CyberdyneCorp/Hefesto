// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PNM writer (capability plot-output "PNM rasterisation and file
// format", "Platform mechanism of plot output"; openspec/changes/
// m10-plot-route/design.md D4, D9, "Area contracts" PNM): a port of Magic's
// PlotPNM writing a binary `P6` image, rows top first.
//
// Owner: PNM.

#pragma once

#include <ostream>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/plot/cmd/commands.hpp"
#include "hefesto/plot/params/params.hpp"
#include "hefesto/plot/tech/plot_tech.hpp"

namespace hefesto::plot {

/// What write_pnm produced.
struct PnmInfo {
  int width = 0;       // image size in pixels
  int height = 0;
  int downsample = 0;  // the downsampling applied (the parameter, or the forced value)
};

/// Writes the PNM plot of `region`, `width` pixels wide, to `out`.
[[nodiscard]] diag::Outcome<PnmInfo> write_pnm(const Region& region, const PlotTech& plot_tech, const Params& params,
                                               const PlotContext& context, int width, std::ostream& out);

}  // namespace hefesto::plot
