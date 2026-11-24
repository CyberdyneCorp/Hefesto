// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Measurements, specifications and statistics (openspec/changes/
// m9-lvs-simulation design.md D15, "Area contracts" SIMMEAS; capability
// simulation-measurements-and-statistics). All arithmetic is binary64 and
// sums run in point-index order with Neumaier compensation, so the results
// are the same on every platform and for any worker count.
//
// Owner: SIMMEAS.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/job.hpp"
#include "hefesto/sim/result/result_store.hpp"

namespace hefesto::sim {

struct MeasDefinition {
  std::string name;
  std::string expression;
};

struct MeasSet {
  std::vector<MeasDefinition> definitions;
  std::vector<JobId> jobs;  // the jobs to measure, in point-index order
};

/// One row per job, one column per definition; nullopt = no result.
struct MeasTable {
  std::vector<std::string> columns;
  std::vector<std::vector<std::optional<double>>> rows;
};

struct SpecLimit {
  std::string measurement;
  std::optional<double> min;
  std::optional<double> max;
};

struct Specs {
  std::vector<SpecLimit> limits;
};

struct ColumnStatistics {
  std::string measurement;
  double mean = 0.0;
  double stddev = 0.0;
  double min = 0.0;
  double max = 0.0;
  std::optional<double> yield;  // with a spec
  std::optional<double> cpk;    // with a two-sided spec
};

struct StatsReport {
  std::vector<ColumnStatistics> columns;
};

[[nodiscard]] diag::Outcome<MeasTable> evaluate_measurements(const ResultStore& store, const MeasSet& set);
[[nodiscard]] diag::Outcome<StatsReport> statistics(const MeasTable& table, const Specs& specs);

}  // namespace hefesto::sim
