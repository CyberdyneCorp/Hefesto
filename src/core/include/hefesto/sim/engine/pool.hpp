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
// can only lower N, never raise it. Jobs, sweeps, corners and Monte Carlo
// samples share the pool; iPadOS runs one job at a time.
//
// The scaffold declares the signature only; SIMENG implements it (task 5.5).
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
  std::uint64_t job_budget = 0;       // bytes; B_job (result budget + deck estimate)
  /// The user setting: a value below the formula's N lowers it; a larger
  /// value has no effect. nullopt = no setting.
  std::optional<std::uint32_t> user_limit;
};

[[nodiscard]] diag::Outcome<std::uint32_t> worker_pool_size(const PoolInputs& inputs);

}  // namespace hefesto::sim
