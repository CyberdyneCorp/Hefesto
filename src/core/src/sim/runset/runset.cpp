// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): generate, execute and sample_seed fail
// with CORE-NOTIMPL-009.
//
// Owner: SIMADV.

#include "hefesto/sim/runset/runset.hpp"

#include "hefesto/sim/not_implemented.hpp"

namespace hefesto::sim {

diag::Outcome<std::vector<RunPoint>> RunSet::generate(const RunSetSpec& /*spec*/) {
  return not_implemented("sim::RunSet::generate");
}

diag::Status execute(RunSet& /*run_set*/, JobManager& /*jobs*/) {
  return not_implemented("sim::execute");
}

diag::Outcome<std::uint32_t> sample_seed(std::uint32_t /*base*/, std::uint32_t /*k*/) {
  return not_implemented("sim::sample_seed");
}

}  // namespace hefesto::sim
