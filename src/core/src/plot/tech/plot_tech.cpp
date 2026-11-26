// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (m10-plot-route tasks.md 3.1): read_plot_section records
// whether the technology has a `plot` section and reads nothing else.
// PLOTTECH (tasks 4.1, 4.2) replaces it with the section reader.
//
// Owner: PLOTTECH.

#include "hefesto/plot/tech/plot_tech.hpp"

namespace hefesto::plot {

PlotTech read_plot_section(const tech::RawSection* section, const tech::Technology& /*tech*/,
                           const StyleSource& /*styles*/, diag::DiagnosticSink& /*sink*/) {
  PlotTech result;
  result.has_section = section != nullptr;
  return result;
}

}  // namespace hefesto::plot
