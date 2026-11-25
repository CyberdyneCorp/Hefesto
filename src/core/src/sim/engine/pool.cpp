// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The worker pool size of Q-SIMADV-13 (simulation-engine-embedding
// "Multiple simulations and corner parallelism"; design D10, D18).
//
// Owner: SIMENG.

#include "hefesto/sim/engine/pool.hpp"

#include <algorithm>
#include <string>

namespace hefesto::sim {

namespace {

constexpr std::uint64_t kMaxWorkers = 4;

}  // namespace

std::uint32_t max_worker_pool_size(const PoolInputs& inputs) noexcept {
  // An unknown core count (0) counts as one core; a single core leaves
  // physical_cores - 1 = 0, and the outer max gives 1.
  const std::uint64_t cores = std::max<std::uint64_t>(inputs.physical_cores, 1);
  std::uint64_t n = std::min<std::uint64_t>(kMaxWorkers, cores - 1);
  if (inputs.job_budget != 0) {
    // A B_job of 0 (no estimate) drops the memory term: no division by zero.
    n = std::min(n, (inputs.physical_memory / 2) / inputs.job_budget);
  }
  return static_cast<std::uint32_t>(std::max<std::uint64_t>(n, 1));
}

diag::Outcome<std::uint32_t> worker_pool_size(const PoolInputs& inputs) {
  const std::uint32_t n = max_worker_pool_size(inputs);
  if (!inputs.user_limit) {
    return n;
  }
  const std::uint32_t requested = std::max<std::uint32_t>(*inputs.user_limit, 1);  // 0 counts as 1
  if (requested > n) {
    const std::string max = std::to_string(n);
    return diag::make("SIM-ENGINE-033", "worker count " + std::to_string(requested) + " is above the maximum " + max +
                                            " of this host; the pool keeps " + max + " workers");
  }
  return requested;
}

}  // namespace hefesto::sim
