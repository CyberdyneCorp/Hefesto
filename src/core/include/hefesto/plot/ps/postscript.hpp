// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PostScript writer (capability plot-output "PostScript file layout",
// "PostScript paint output", "PostScript cell outlines", "PostScript
// labels", "PostScript label margins"; openspec/changes/m10-plot-route/
// design.md D4, D8, "Area contracts" PS): a port of Magic's PlotPS with the
// embedded prologue assets/plot/magicps.pro.
//
// Owner: PS.

#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/plot/cmd/commands.hpp"
#include "hefesto/plot/params/params.hpp"
#include "hefesto/plot/tech/plot_tech.hpp"

namespace hefesto::plot {

/// Writes the PostScript plot of `region` to `out`.
[[nodiscard]] diag::Status write_postscript(const Region& region, const PlotTech& plot_tech, const Params& params,
                                            const PlotContext& context, std::ostream& out);

/// `text` as a PostScript string body: `(`, `)` and `\` escaped (Q-MISC-42,
/// both modes). Used for labels, cell names and instance ids.
[[nodiscard]] std::string escape_ps_text(std::string_view text);

}  // namespace hefesto::plot
