// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (m10-plot-route tasks.md 3.1): the defaults are final;
// print writes nothing and set reports CORE-NOTIMPL-010 until PLOTCMD
// (task 5.2) lands them.
//
// Owner: PLOTCMD.

#include "hefesto/plot/params/params.hpp"

#include "hefesto/plot/not_implemented.hpp"

namespace hefesto::plot {

void Params::print(std::ostream& /*out*/) const {}

diag::Status Params::set(std::string_view /*name*/, std::string_view /*value*/) {
  return not_implemented("plot::Params::set");
}

}  // namespace hefesto::plot
