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
// Added in M9 R1 (task 5.5), all additive:
//   - JobManagerOptions: the engine factory (one call per worker started;
//     WorkerEngine through make_worker_engine in the product, a fake in the
//     tests), the pool inputs (worker_pool_size) and a clock for job ids;
//   - poll(): the owning thread applies the engine events (callbacks only
//     queue them) and starts queued jobs on free workers;
//   - cancel, pause, resume, job and record lookups, and counters.
//
// Engine events, per worker: EngineCallbacks::on_state(running) moves the
// job from `loading` to `running`; on_state(paused) to `paused` and back
// on running; on_state(idle or loaded) after `running` ends the job
// (`completed`, or `failed` when an error-stream line starts with `Error`,
// or `cancelled` after cancel), and its vectors are copied with vectors().
// on_output lines starting with `stderr ` are error-stream lines (the
// prefix is removed), as the adapter receives them from SendChar.
//
// A fault is on_state(failed) (WorkerEngine reports a crash, a controlled
// exit, a missed heartbeat, a malformed or oversize frame this way) or an
// error returned by any Engine call. The job fails with SIM-ENGINE-030
// `simulation engine terminated` followed by its last 50 error-stream lines
// (SIM-ENGINE-032), the engine is destroyed, and the next job on that
// worker starts in a new engine from the factory.
//
// Owner: SIMENG.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/deck/deck.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/job.hpp"
#include "hefesto/sim/engine/pool.hpp"

namespace hefesto::sim {

struct JobOptions {
  std::string testbench;
  std::string corner;
  /// The run-set point id `<run set id>-p<index>`; nullopt for a single job.
  std::optional<JobId> run_set_point;
};

/// Added in M9 R1: starts one engine (a worker on desktop). Called when a
/// job needs a worker that has none: the first job, and the first job after
/// a fault.
using EngineFactory = std::function<diag::Outcome<std::unique_ptr<Engine>>()>;

/// Added in M9 R1.
struct JobManagerOptions {
  /// Empty: no engine host; every job fails with SIM-ENGINE-031.
  EngineFactory make_engine;
  /// The pool (worker_pool_size). A refused user setting leaves the
  /// formula's N and is reported by JobManager::pool_diagnostic().
  PoolInputs pool;
  /// The clock of the job ids (local time); empty: the system clock.
  std::function<std::chrono::system_clock::time_point()> now;
};

/// Added in M9 R1: what the manager keeps of one job.
struct JobRecord {
  Job job;
  JobOptions options;
  std::vector<std::string> log;                // output and error-stream lines, in order
  std::vector<std::string> errors;             // the error-stream lines (prefix removed), the last 50
  std::vector<diag::Diagnostic> diagnostics;   // set when the job failed
  std::vector<Vector> vectors;                 // the final (or partial) vectors
  std::uint64_t engine = 0;                    // which engine ran it (1 = the first engine started)
};

class JobManager {
 public:
  JobManager();
  explicit JobManager(JobManagerOptions options);
  ~JobManager();
  JobManager(JobManager&&) noexcept;
  JobManager& operator=(JobManager&&) noexcept;
  JobManager(const JobManager&) = delete;
  JobManager& operator=(const JobManager&) = delete;

  /// Queues `deck` and returns the job's id. The job starts on a later
  /// poll().
  [[nodiscard]] diag::Outcome<JobId> submit(PreparedDeck deck, JobOptions options);

  /// Added in M9 R1: applies the queued engine events and starts queued
  /// jobs on free workers. Returns the number of events applied plus jobs
  /// started (0: nothing happened).
  std::size_t poll();

  /// Added in M9 R1: a queued job is cancelled at once; a running or paused
  /// one is cancelled in its engine and becomes `cancelled` when the engine
  /// reports the end of the run.
  [[nodiscard]] diag::Status cancel(const JobId& id);
  [[nodiscard]] diag::Status pause(const JobId& id);
  [[nodiscard]] diag::Status resume(const JobId& id);

  /// Added in M9 R1: nullptr for an unknown id.
  [[nodiscard]] const JobRecord* record(const JobId& id) const;
  [[nodiscard]] std::optional<JobState> state(const JobId& id) const;

  /// Added in M9 R1: the pool size in use.
  [[nodiscard]] std::uint32_t workers() const noexcept;
  /// Added in M9 R1: SIM-ENGINE-033 when the user setting was refused.
  [[nodiscard]] const std::optional<diag::Diagnostic>& pool_diagnostic() const noexcept;
  /// Added in M9 R1: jobs in `loading`, `running` or `paused` now.
  [[nodiscard]] std::size_t active() const noexcept;
  /// Added in M9 R1: the largest active() seen so far.
  [[nodiscard]] std::size_t peak_active() const noexcept;
  /// Added in M9 R1: engines started by the factory so far.
  [[nodiscard]] std::uint64_t engines_started() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::sim
