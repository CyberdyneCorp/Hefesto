// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::JobManager: the job queue of one session (openspec/changes/
// m9-lvs-simulation design.md D9, D10, "Area contracts" SIMENG; capability
// simulation-analyses-and-results "Job lifecycle"). It owns the queue and
// the worker channels on one core thread and never blocks the UI thread.
// Fault policy (D-25): every fault replaces the worker; a healthy worker is
// reused for the next job.
//
// Owner: SIMENG.

#pragma once

#include <optional>
#include <string>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/deck/deck.hpp"
#include "hefesto/sim/engine/job.hpp"

namespace hefesto::sim {

struct JobOptions {
  std::string testbench;
  std::string corner;
  /// The run-set point id `<run set id>-p<index>`; nullopt for a single job.
  std::optional<JobId> run_set_point;
};

class JobManager {
 public:
  /// Queues `deck` and returns the job's id.
  [[nodiscard]] diag::Outcome<JobId> submit(PreparedDeck deck, JobOptions options);
};

}  // namespace hefesto::sim
