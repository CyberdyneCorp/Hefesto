// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::Session: the simulation state of one session,
// editor::EditorState::sim (openspec/changes/m9-lvs-simulation design.md D4).
//
// Owner: SCAFFOLD owns the struct; each member belongs to the named
// workstream.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/sim/deck/deck.hpp"
#include "hefesto/sim/engine/job.hpp"
#include "hefesto/sim/engine/job_manager.hpp"
#include "hefesto/sim/result/result_store.hpp"
#include "hefesto/sim/runset/runset.hpp"

namespace hefesto::sim {

/// Whether a worker can start (the Simulate button's state, design D23).
enum class EngineStatus : std::uint8_t { unknown, available, unavailable };

struct Session {
  // SIMDECK / SIMCMD: the project's testbenches, in creation order.
  std::vector<Testbench> testbenches;

  // SIMENG: the job queue and the worker channels.
  JobManager jobs;

  // SIMADV: the run sets of this session.
  std::vector<RunSet> run_sets;

  // SIMRES: the result store and its index (the stored jobs, oldest first).
  ResultStore results;
  std::vector<JobId> stored_jobs;

  // SIMENG: the engine status and, when unavailable, the reason shown in
  // `simulation engine unavailable: <reason>`.
  EngineStatus engine = EngineStatus::unknown;
  std::string engine_reason;
};

}  // namespace hefesto::sim
