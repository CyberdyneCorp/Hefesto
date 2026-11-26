// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stub (m10-plot-route tasks.md 3.1): write_pnm reports
// CORE-NOTIMPL-010 and writes nothing until PNM (tasks 7.1-7.3) lands it.
//
// Owner: PNM.

#include "hefesto/plot/pnm/pnm.hpp"

#include "hefesto/plot/not_implemented.hpp"

namespace hefesto::plot {

diag::Outcome<PnmInfo> write_pnm(const Region& /*region*/, const PlotTech& /*plot_tech*/, const Params& /*params*/,
                                 const PlotContext& /*context*/, int /*width*/, std::ostream& /*out*/) {
  return not_implemented("plot::write_pnm");
}

}  // namespace hefesto::plot
