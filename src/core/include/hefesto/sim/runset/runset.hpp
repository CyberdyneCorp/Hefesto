// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Run sets: sweeps, corners and Monte Carlo (openspec/changes/
// m9-lvs-simulation design.md D14, D18, "Area contracts" SIMADV;
// capabilities simulation-sweeps, simulation-corners,
// simulation-monte-carlo). Points are generated corner-outer, sweep
// lexicographic in declaration order, sample last; each point is one job on
// the worker pool (sim/engine/pool.hpp) and results are stored by point
// index.
//
// Owner: SIMADV.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/job.hpp"

namespace hefesto::sim {

class JobManager;

/// Caps of design D18 (Q-SIMADV, Q-SIM-25).
inline constexpr std::size_t kMaxRunSetPoints = 100'000;
inline constexpr std::size_t kMaxMonteCarloSamples = 100'000;

struct SweepVariable {
  std::string name;
  std::vector<double> values;
};

struct RunSetSpec {
  std::string id;
  std::vector<std::string> corners;  // empty = the testbench's corner
  std::vector<SweepVariable> sweeps;
  std::uint32_t samples = 0;         // Monte Carlo samples; 0 = none
  std::uint32_t base_seed = 1;       // B of "Sample seeds"
};

/// One point of a run set (the contract's `Point`).
struct RunPoint {
  std::size_t index = 0;
  std::string corner;
  std::vector<double> values;  // one per sweep variable, in declaration order
  std::uint32_t sample = 0;    // 0 = nominal
};

struct RunSet {
  RunSetSpec spec;
  std::vector<RunPoint> points;
  std::vector<JobId> jobs;  // by point index

  /// The points of `spec` in the fixed order; an error above the caps.
  [[nodiscard]] static diag::Outcome<std::vector<RunPoint>> generate(const RunSetSpec& spec);
};

/// Submits every point of `run_set` to `jobs`.
[[nodiscard]] diag::Status execute(RunSet& run_set, JobManager& jobs);

/// The ngspice seed of sample `k` with base seed `base` ("Sample seeds"); 1
/// for the nominal sample. The scaffold stub returns 1 for every sample.
[[nodiscard]] std::uint32_t sample_seed(std::uint32_t base, std::uint32_t k);

}  // namespace hefesto::sim
