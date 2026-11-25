// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The size of the worker pool (openspec/changes/m9-lvs-simulation design.md
// D10, D18; Q-SIMADV-13, approved 2026-11-22; capability
// simulation-engine-embedding "Multiple simulations and corner
// parallelism"):
//
//   N = max(1, min(4, physical_cores - 1, floor(B_total / B_job)))
//
// where B_total is half the physical memory and B_job the job's result
// memory budget plus an estimate of its prepared deck size. A user setting
// can only lower N: a setting above N is refused with SIM-ENGINE-033, which
// names the maximum N, and the pool stays at N (scenario "Setting cannot
// raise the pool"). Degenerate inputs still give N >= 1: a
// user_limit of 0 counts as 1 (no empty pool, so no hang), a job_budget of 0
// drops the memory term (no division by zero), and physical_cores 0 counts as
// one core. Jobs, sweeps, corners and Monte Carlo
// samples share the pool; iPadOS runs one job at a time.
//
// Owner: SIMENG.

#pragma once

#include <cstdint>
#include <optional>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::sim {

struct PoolInputs {
  std::uint32_t physical_cores = 1;
  std::uint64_t physical_memory = 0;  // bytes; B_total = physical_memory / 2
  std::uint64_t job_budget = 0;       // bytes; B_job (result budget + deck estimate); 0 = no memory term
  /// The user setting: a value up to the formula's N sets the pool size; a
  /// larger value is refused (see worker_pool_size); 0 counts as 1. nullopt
  /// = no setting.
  std::optional<std::uint32_t> user_limit;
};

/// The pool size: the formula's N lowered by `user_limit`. Fails with
/// SIM-ENGINE-033 ("worker count <k> is above the maximum <N> of this
/// host") when `user_limit` is larger than N; the caller then keeps N
/// (max_worker_pool_size), so at most N jobs run.
[[nodiscard]] diag::Outcome<std::uint32_t> worker_pool_size(const PoolInputs& inputs);

/// Added in M9 R1: the formula's N alone, ignoring `user_limit` (the
/// maximum a setting may ask for). Always at least 1.
[[nodiscard]] std::uint32_t max_worker_pool_size(const PoolInputs& inputs) noexcept;

}  // namespace hefesto::sim
