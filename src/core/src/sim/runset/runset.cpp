// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (tasks.md 4.2): generate and execute fail with
// CORE-NOTIMPL-009; sample_seed answers the nominal seed 1.
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

std::uint32_t sample_seed(std::uint32_t /*base*/, std::uint32_t /*k*/) {
  return 1;
}

}  // namespace hefesto::sim
