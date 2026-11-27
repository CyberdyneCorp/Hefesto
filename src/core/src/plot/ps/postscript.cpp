// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): write_postscript reports
// CORE-NOTIMPL-010 and writes nothing; escape_ps_text returns the text
// unchanged until PS (tasks 6.1-6.3) lands them.
//
// Owner: PS.

#include "hefesto/plot/ps/postscript.hpp"

#include "hefesto/plot/not_implemented.hpp"

namespace hefesto::plot {

diag::Status write_postscript(const Region& /*region*/, const PlotTech& /*plot_tech*/, const Params& /*params*/,
                              const PlotContext& /*context*/, std::ostream& /*out*/) {
  return not_implemented("plot::write_postscript");
}

std::string escape_ps_text(std::string_view text) { return std::string{text}; }

}  // namespace hefesto::plot
