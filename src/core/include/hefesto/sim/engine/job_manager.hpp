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
// on running; on_state(idle or loaded) after `running` ends the job, and
// so does on_state(paused) after cancel (the adapter halts a cancelled run
// the way it pauses one). The job ends `cancelled` after cancel, `failed`
// when any error-stream line started with `Error` (SIM-ENGINE-032) or the
// run left no vectors (SIM-ENGINE-037), and `completed` otherwise; its
// vectors are copied with vectors(). Cancelling a job that is already
// paused ends it at once with the vectors of the pause.
// EngineCallbacks::on_error lines are the error-stream lines (the adapter
// has removed SendChar's `stderr ` prefix); on_output lines are the rest
// of the job log. Each engine's events carry the job they belong to, so a
// late event of an ended job never reaches the next one.
//
// A fault is on_state(failed) (WorkerEngine reports a crash, a controlled
// exit, a missed heartbeat, a malformed or oversize frame this way) or an
// error returned by an Engine call other than a refusal of a healthy
// engine. The job fails with SIM-ENGINE-030 `simulation engine terminated`,
// the cause (the engine's own diagnostic, first line), and its last 50
// error-stream lines (SIM-ENGINE-032); the engine is destroyed, and the
// next job on that worker starts in a new engine from the factory. The
// refusals are SIM-ENGINE-009 (deck rejected: the job fails with the
// refusal and its error lines, the engine is kept), and SIM-ENGINE-010 or
// 011 on pause or resume (returned to the caller; the job is unchanged).
//
// Threading (M9 R2): a job's start runs on a loader thread of its worker
// slot: the factory when the slot has no engine (the worker's handshake,
// up to 10 s), set_callbacks, Engine::load with the job's LoadOptions (the
// deck descriptor's initialisation commands and JobOptions::seed) and
// Engine::run. poll() never waits for it: it keeps applying events and
// starting jobs on other workers. The loader posts a `started` event when
// it ends; poll() then joins it, keeps the new engine, and applies the
// engine's state events of the load, which it held back meanwhile (a fault
// among them replaces the engine as above). While a job loads, pause and
// resume are refused (SIM-ENGINE-035) and cancel() returns at once: the
// loader skips the run when it has not started it, else the run is
// cancelled when the loader ends; either way the job ends `cancelled`.
// The factory may be called from several loader threads at once. The
// destructor waits for loads in progress.
//
// Bounds (M9 R2): the event queue holds at most 65536 line events and
// kMaxQueuedStateEvents state events between two polls; a worker that
// floods state frames has its later states coalesced (see
// job_manager_events.hpp), and progress events are superseded by later
// ones, so no worker can grow the queue without bound. Since the M9 R2 fix
// pass one poll() applies at most kMaxPollPasses batches, so no worker can
// hold the owning thread in poll() either; job ids must be one safe path
// component (SIM-ENGINE-038).
//
// Limits (M9 R3; Q-SIM-25 and Q-SIM-16 as approved 2026-11-27, design D18):
// JobManagerOptions::max_log_bytes (default kMaxJobLogBytes, 64 MiB) bounds
// each job's in-memory log; later lines are counted in log_dropped.
// JobManagerOptions::result_budget (default kDesktopResultBudget, 4 GiB until
// Q-SIM-25 decides) bounds each job's result: when the streamed estimate
// (values received through on_data x 8 bytes, a lower bound of the spec's
// `points x sum(8 x (2 if complex else 1))`) passes it, the run is cancelled
// in its engine; when the run ends, or when its final vectors cost more than
// the budget, the job ends `failed` with SIM-RESULT-011 `result memory budget
// exceeded (<estimate> > <B>)`, keeps its vectors and is marked `partial`.
// Each setting can be lowered; zero or a value above twice its default is
// refused with SIM-ENGINE-039 (limits_diagnostic()) and the default is used.
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
#include "hefesto/sim/result/result_store.hpp"

namespace hefesto::sim {

struct JobOptions {
  std::string testbench;
  std::string corner;
  /// The run-set point id `<run set id>-p<index>`; nullopt for a single job.
  std::optional<JobId> run_set_point;
  /// Added in M9 R2: the seed the engine gets before the deck (`setseed`).
  std::uint32_t seed = 1;
};

/// Added in M9 R1: starts one engine (a worker on desktop). Called when a
/// job needs a worker that has none: the first job, and the first job after
/// a fault.
using EngineFactory = std::function<diag::Outcome<std::unique_ptr<Engine>>()>;

/// Added in M9 R1: the job log kept in memory per job; later lines are
/// counted in JobRecord::log_dropped instead.
inline constexpr std::size_t kMaxJobLogBytes = std::size_t{64} << 20;

/// Added in M9 R1.
struct JobManagerOptions {
  /// Empty: no engine host; every job fails with SIM-ENGINE-031.
  EngineFactory make_engine;
  /// The pool (worker_pool_size). A refused user setting leaves the
  /// formula's N and is reported by JobManager::pool_diagnostic().
  PoolInputs pool;
  /// The clock of the job ids (local time); empty: the system clock.
  std::function<std::chrono::system_clock::time_point()> now;
  /// Added in the M9 R2 fix pass, for tests: called on the owning thread
  /// after poll() takes each batch of events (a stand-in for a worker that
  /// sends while the batch is applied).
  std::function<void()> on_events_taken;
  /// Added in M9 R3: the in-memory job log per job, in bytes (Q-SIM-25,
  /// approved 2026-11-27); lowerable, at most twice kMaxJobLogBytes.
  std::size_t max_log_bytes = kMaxJobLogBytes;
  /// Added in M9 R3: the result memory budget per job, in bytes (Q-SIM-16;
  /// 4 GiB on desktop and 512 MiB on iPadOS until Q-SIM-25 decides);
  /// lowerable, at most twice the platform's default.
  std::uint64_t result_budget = kPlatformResultBudget;
};

/// Added in M9 R1: one job-log line and its stream (log.txt writes error
/// lines with `E ` and the others with `O `).
struct LogLine {
  bool error = false;
  std::string text;
  friend bool operator==(const LogLine&, const LogLine&) = default;
};

/// Added in M9 R2: state events queued between two polls before a
/// connection's later states are coalesced.
inline constexpr std::size_t kMaxQueuedStateEvents = 4096;

/// Added in M9 R2 (fix pass): event batches one poll() applies at most.
/// A worker that keeps sending frames cannot hold the owning thread in
/// poll(): what arrives after the last batch waits for the next poll.
inline constexpr std::size_t kMaxPollPasses = 4;

/// Added in M9 R1: what the manager keeps of one job.
struct JobRecord {
  Job job;
  JobOptions options;
  std::vector<LogLine> log;                    // output and error-stream lines, in order
  std::size_t log_bytes = 0;                   // text bytes in `log`
  std::uint64_t log_dropped = 0;               // lines not kept (kMaxJobLogBytes, or a flooded event queue)
  std::vector<std::string> errors;             // the error-stream lines (prefix removed), the last 50
  std::optional<std::string> first_error;      // the first error-stream line that starts with `Error`
  std::vector<diag::Diagnostic> diagnostics;   // set when the job failed
  std::vector<Vector> vectors;                 // the final (or partial) vectors
  std::uint64_t engine = 0;                    // which engine ran it (1 = the first engine started)
  double progress = 0.0;                       // M9 R2: "Progress reporting", 0..1
  std::string status;                          // M9 R2: the last status string of the engine
  /// M9 R3: the vectors are those of a run that did not complete (job.json
  /// `partial`): a cancelled job, or a failed one that kept vectors.
  bool partial = false;
  /// M9 R3: the result-size estimate when the budget was exceeded, else 0.
  std::uint64_t budget_exceeded = 0;
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
  /// poll(). M9 R3: a run-set point id whose job ended `failed` or
  /// `cancelled` may be submitted again (resume, re-run point); the record
  /// of the id is then the new job's.
  [[nodiscard]] diag::Outcome<JobId> submit(PreparedDeck deck, JobOptions options);

  /// Added in M9 R1: applies the queued engine events and starts queued
  /// jobs on free workers. Returns the number of events applied plus jobs
  /// started (0: nothing happened). Since the M9 R2 fix pass it takes at
  /// most kMaxPollPasses batches of events, so it returns while a worker
  /// is still sending.
  std::size_t poll();

  /// Added in M9 R2: waits up to `timeout` for an engine event or the end
  /// of a load, then polls.
  std::size_t poll_for(std::chrono::milliseconds timeout);

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
  /// Added in M9 R2: jobs whose loader thread has not ended yet.
  [[nodiscard]] std::size_t loading() const noexcept;
  /// Added in M9 R2: the most events the queue held at once so far.
  [[nodiscard]] std::size_t peak_queued_events() const;
  /// Added in M9 R3: SIM-ENGINE-039 when a limit setting of the options was
  /// refused (its default is used instead).
  [[nodiscard]] const std::optional<diag::Diagnostic>& limits_diagnostic() const noexcept;
  /// Added in M9 R3: the limits in use.
  [[nodiscard]] std::size_t max_log_bytes() const noexcept;
  [[nodiscard]] std::uint64_t result_budget() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hefesto::sim
