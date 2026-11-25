// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): both functions fail with CORE-NOTIMPL-009.
//
// Owner: SIMMEAS.

#include "hefesto/sim/meas/meas.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<MeasTable> evaluate_measurements(const ResultStore& /*store*/, const MeasSet& /*set*/) {
  return not_implemented("sim::evaluate_measurements");
}

diag::Outcome<StatsReport> statistics(const MeasTable& /*table*/, const Specs& /*specs*/) {
  return not_implemented("sim::statistics");
}

}  // namespace hefesto::sim
