// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::JobManager with a fake Engine (task 5.5; design D9, D10, D-25 (a)):
// the queue and job states, the worker pool, engine reuse, and the fault
// policy (every fault replaces the engine). The fake runs the deck's first
// line as a script; the real worker faults (crash, controlled exit, missed
// heartbeat, oversize frame) reach the manager as on_state(failed), which is
// what the fake sends. Pause, resume and cancel behave like the adapter's
// (src/simd/adapter/ngspice_adapter.cpp): a halt reports `paused`, also for
// cancel; pause with nothing running is refused (SIM-ENGINE-010); cancel
// with nothing running succeeds and reports nothing.
//
// Owner: SIMENG.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "hefesto/sim/engine/job_manager.hpp"

namespace diag = hefesto::diag;
namespace sim = hefesto::sim;

namespace {

constexpr std::uint64_t kGiB = std::uint64_t{1} << 30;
constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;

/// What the fake engines did, shared by every engine of one factory.
struct FakeHost {
  std::uint64_t started = 0;
  std::uint64_t alive = 0;
  std::vector<std::uint64_t> loads;  // the engine number of each load, in order
  std::uint64_t cancels = 0;
  class FakeEngine* last = nullptr;  // the engine started last
};

class FakeEngine final : public sim::Engine {
 public:
  FakeEngine(FakeHost& host, std::uint64_t number) : host_(host), number_(number) {
    ++host_.alive;
    host_.last = this;
  }
  ~FakeEngine() override {
    --host_.alive;
    if (host_.last == this) {
      host_.last = nullptr;
    }
  }
  FakeEngine(const FakeEngine&) = delete;
  FakeEngine& operator=(const FakeEngine&) = delete;
  FakeEngine(FakeEngine&&) = delete;
  FakeEngine& operator=(FakeEngine&&) = delete;

  void set_callbacks(sim::EngineCallbacks callbacks) override { callbacks_ = std::move(callbacks); }

  diag::Status load(const std::vector<std::string>& deck) override {
    host_.loads.push_back(number_);
    script_ = deck.empty() ? std::string{} : deck.front();
    if (script_ == "* load-reject") {
      callbacks_.on_error("Error: unknown subckt: xinv");
      return diag::make("SIM-ENGINE-009", "the deck was not loaded\nError: unknown subckt: xinv");
    }
    if (script_ == "* load-crash") {
      callbacks_.on_error("Error: last words");
      fault_ = diag::make("SIM-ENGINE-008", "simulation engine terminated: the worker was killed by signal 11\n"
                                            "Error: last words");
      callbacks_.on_state(sim::EngineState::failed);
      return *fault_;
    }
    callbacks_.on_state(sim::EngineState::loaded);
    return diag::Done{};
  }

  diag::Status run() override {
    callbacks_.on_state(sim::EngineState::running);
    running_ = script_ == "* hold";
    if (script_ == "* ok") {
      callbacks_.on_output("Note: no compatibility mode selected");
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* warning") {
      callbacks_.on_error("Warning: singular matrix fixed");
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* timestep") {
      callbacks_.on_error("Error: Timestep too small");
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* error-then-warnings") {
      callbacks_.on_error("Error: Timestep too small");
      for (int i = 0; i < 60; ++i) {
        callbacks_.on_error("Warning: timestep reduced " + std::to_string(i));
      }
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* noplot") {
      callbacks_.on_output("Note: no analysis in the deck");
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* killed") {
      callbacks_.on_error("Error: last words");
      fault_ = diag::make("SIM-ENGINE-008", "simulation engine terminated: the worker was killed by signal 11\n"
                                            "Error: last words");
      callbacks_.on_state(sim::EngineState::failed);
    } else if (script_ == "* segv") {
      // The worker died: WorkerEngine reports it as failed. A late line of
      // the dead engine must not reach the next job.
      callbacks_.on_state(sim::EngineState::failed);
      callbacks_.on_error("Error: late line of a dead engine");
    } else if (script_ == "* exit") {
      callbacks_.on_error("Error: unknown subckt: xinv");
      callbacks_.on_error("Error: ngspice.dll cannot recover");
      callbacks_.on_state(sim::EngineState::failed);
    } else if (script_ == "* thread") {
      std::thread other([this] {
        callbacks_.on_output("from the engine thread");
        callbacks_.on_state(sim::EngineState::loaded);
      });
      other.join();
    }
    // "* hold": stays running until cancelled.
    return diag::Done{};
  }

  diag::Status pause() override {
    if (!running_) {
      return diag::make("SIM-ENGINE-010", "no analysis is running");
    }
    halt();
    return diag::Done{};
  }
  diag::Status resume() override {
    if (running_) {
      return diag::make("SIM-ENGINE-011", "engine busy");
    }
    running_ = true;
    callbacks_.on_state(sim::EngineState::running);
    return diag::Done{};
  }
  diag::Status cancel() override {
    ++host_.cancels;
    if (running_) {
      halt();
    }
    return diag::Done{};
  }

  /// The held analysis ends by itself (its `loaded` event is queued).
  void end_run() {
    running_ = false;
    callbacks_.on_state(sim::EngineState::loaded);
  }

  diag::Outcome<std::vector<sim::Vector>> vectors() const override {
    if (fault_) {
      return *fault_;
    }
    if (script_ == "* noplot") {
      return std::vector<sim::Vector>{};
    }
    return std::vector<sim::Vector>{sim::Vector{"v(out)", "voltage", false, {1.0, 2.0}, {}}};
  }

 private:
  FakeHost& host_;
  std::uint64_t number_;
  void halt() {
    running_ = false;
    callbacks_.on_state(sim::EngineState::paused);
  }

  sim::EngineCallbacks callbacks_;
  std::string script_;
  bool running_ = false;
  std::optional<diag::Diagnostic> fault_;
};

std::size_t count_text(const sim::JobRecord& rec, std::string_view text) {
  std::size_t n = 0;
  for (const diag::Diagnostic& d : rec.diagnostics) {
    n += d.text.find(text) != std::string::npos ? 1 : 0;
  }
  return n;
}

sim::JobManagerOptions options(FakeHost& host, sim::PoolInputs pool = {}) {
  sim::JobManagerOptions o;
  o.make_engine = [&host]() -> diag::Outcome<std::unique_ptr<sim::Engine>> {
    return std::unique_ptr<sim::Engine>(std::make_unique<FakeEngine>(host, ++host.started));
  };
  o.pool = pool;
  o.now = [] { return std::chrono::system_clock::time_point{} + std::chrono::hours(24 * 365 * 50); };
  return o;
}

sim::PreparedDeck deck(std::string script) {
  sim::PreparedDeck d;
  d.lines = {std::move(script), ".tran 1n 10n", ".end"};
  return d;
}

sim::JobId submit(sim::JobManager& jobs, const std::string& script, const std::string& corner = "tt") {
  auto id = jobs.submit(deck(script), sim::JobOptions{"tb", corner, std::nullopt});
  REQUIRE(id.ok());
  return id.value();
}

bool has_code(const sim::JobRecord& rec, std::string_view code, std::string_view text) {
  for (const diag::Diagnostic& d : rec.diagnostics) {
    if (d.code == code && d.text == text) {
      return true;
    }
  }
  return false;
}

/// 8 physical cores and 32 GiB with the default result budget: N = 3.
const sim::PoolInputs kThreeWorkers{8, 32 * kGiB, 4 * kGiB + kMiB, std::nullopt};

}  // namespace

TEST_CASE("A job goes queued, loading, running, completed and keeps its vectors",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* ok");
  CHECK(jobs.state(id) == sim::JobState::queued);
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(id);
  REQUIRE(rec != nullptr);
  CHECK(rec->job.state == sim::JobState::completed);
  REQUIRE(rec->vectors.size() == 1);
  CHECK(rec->vectors[0].name == "v(out)");
  CHECK(rec->log == std::vector<sim::LogLine>{{false, "Note: no compatibility mode selected"}});
  CHECK(rec->diagnostics.empty());
}

TEST_CASE("Job ids carry the time, testbench and corner, with a suffix on collision",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId a = submit(jobs, "* ok");
  const sim::JobId b = submit(jobs, "* ok");
  const sim::JobId c = submit(jobs, "* ok");
  CHECK(a.size() == std::string{"YYYYMMDD-HHMMSS-tb-tt"}.size());
  CHECK(a.ends_with("-tb-tt"));
  CHECK(b == a + "-2");
  CHECK(c == a + "-3");
  auto point = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", sim::JobId{"rs1-p0"}});
  REQUIRE(point.ok());
  CHECK(point.value() == "rs1-p0");
  auto again = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", sim::JobId{"rs1-p0"}});
  REQUIRE_FALSE(again.ok());
  CHECK(again.error().code == "SIM-ENGINE-036");
}

TEST_CASE("Convergence failure: an Error line fails the job and keeps the partial result",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* timestep");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(id);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-ENGINE-032", "Error: Timestep too small"));
  CHECK(rec->vectors.size() == 1);
  // Not a fault: the engine is healthy and reused.
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.started == 1);
}

TEST_CASE("A Warning line is logged and does not fail the job",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* warning");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(id);
  CHECK(rec->job.state == sim::JobState::completed);
  CHECK(rec->errors == std::vector<std::string>{"Warning: singular matrix fixed"});
  // Regression: the log kept no stream, so log.txt could not write `E `.
  CHECK(rec->log == std::vector<sim::LogLine>{{true, "Warning: singular matrix fixed"}});
}

// Regression: only the last 50 error-stream lines were searched for `Error`,
// so 60 warnings after an Error line left the job `completed`.
TEST_CASE("An Error line followed by more than 50 warnings still fails the job",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* error-then-warnings");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(id);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(rec->errors.size() == 50);
  CHECK(has_code(*rec, "SIM-ENGINE-032", "Error: Timestep too small"));
  CHECK(rec->log.size() == 61);
}

// Regression: a run that produced no plot was `completed`.
TEST_CASE("A run that produces no plot fails the job",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* noplot");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(id);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-ENGINE-037", "the run produced no plot"));
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.started == 1);  // not a fault
}

TEST_CASE("Second job waits until the first reaches a final state",
          "[cap:simulation-engine-embedding][req:Single engine instance per address space]") {
  FakeHost host;
  sim::JobManager jobs(options(host));  // default inputs: one worker
  REQUIRE(jobs.workers() == 1);
  const sim::JobId a = submit(jobs, "* hold");
  const sim::JobId b = submit(jobs, "* ok");
  jobs.poll();
  CHECK(jobs.state(a) == sim::JobState::running);
  CHECK(jobs.state(b) == sim::JobState::queued);
  REQUIRE(jobs.cancel(a).ok());
  jobs.poll();
  CHECK(jobs.state(a) == sim::JobState::cancelled);
  CHECK(jobs.state(b) == sim::JobState::completed);
  CHECK(host.started == 1);  // a healthy engine is reused
}

TEST_CASE("Healthy engines are reused for later jobs",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  for (int i = 0; i < 10; ++i) {
    submit(jobs, "* ok");
  }
  jobs.poll();
  CHECK(host.started == 1);
  CHECK(host.loads == std::vector<std::uint64_t>(10, 1));
}

TEST_CASE("ngspice segmentation fault: the job fails and the next job runs in a new engine",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId crashed = submit(jobs, "* segv");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(crashed);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-ENGINE-030", "simulation engine terminated"));
  const sim::JobRecord* after = jobs.record(next);
  CHECK(after->job.state == sim::JobState::completed);
  CHECK(after->engine != rec->engine);
  CHECK(host.started == 2);
  CHECK(host.alive == 1);  // the crashed engine was discarded
  // The dead engine's late line reached neither job.
  CHECK(rec->errors.empty());
  CHECK(after->errors.empty());
}

TEST_CASE("Controlled exit replaces the worker and keeps the error lines",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId failed = submit(jobs, "* exit");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(failed);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-ENGINE-030", "simulation engine terminated"));
  CHECK(has_code(*rec, "SIM-ENGINE-032", "Error: unknown subckt: xinv"));
  CHECK(has_code(*rec, "SIM-ENGINE-032", "Error: ngspice.dll cannot recover"));
  CHECK(jobs.record(next)->engine == 2);
  CHECK(host.loads == std::vector<std::uint64_t>{1, 2});
}

TEST_CASE("Unrecoverable error: failed with the error lines, next job in a new worker, no reset",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId failed = submit(jobs, "* exit");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  CHECK(jobs.state(failed) == sim::JobState::failed);
  CHECK(has_code(*jobs.record(failed), "SIM-ENGINE-032", "Error: unknown subckt: xinv"));
  CHECK(jobs.state(next) == sim::JobState::completed);
  // The faulted engine was destroyed, not reset and reused.
  CHECK(host.started == 2);
  CHECK(host.alive == 1);
}

TEST_CASE("An engine call that fails with a fault replaces the engine and keeps its cause once",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId failed = submit(jobs, "* load-crash");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(failed);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-ENGINE-030", "simulation engine terminated"));
  CHECK(has_code(*rec, "SIM-ENGINE-008", "simulation engine terminated: the worker was killed by signal 11"));
  CHECK(count_text(*rec, "Error: last words") == 1);  // regression: listed twice
  CHECK(jobs.record(next)->engine == 2);
  CHECK(jobs.record(next)->errors.empty());
}

// Regression: on_state(failed) faulted the job without the engine's own
// diagnostic, so the reason (signal, heartbeat, bad frame) was lost.
TEST_CASE("A fault event keeps the engine's reason",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId failed = submit(jobs, "* killed");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(failed);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-ENGINE-008", "simulation engine terminated: the worker was killed by signal 11"));
  CHECK(has_code(*rec, "SIM-ENGINE-032", "Error: last words"));
  CHECK(count_text(*rec, "Error: last words") == 1);
}

// Regression: a deck the engine refused (SIM-ENGINE-009) was reported as a
// crash and the healthy worker was replaced.
TEST_CASE("A rejected deck fails the job with its error lines and keeps the engine",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId failed = submit(jobs, "* load-reject");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(failed);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK_FALSE(has_code(*rec, "SIM-ENGINE-030", "simulation engine terminated"));
  CHECK(has_code(*rec, "SIM-ENGINE-009", "the deck was not loaded"));
  CHECK(has_code(*rec, "SIM-ENGINE-032", "Error: unknown subckt: xinv"));
  CHECK(count_text(*rec, "Error: unknown subckt: xinv") == 1);
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(jobs.record(next)->errors.empty());  // the rejected deck's line stayed with its job
  CHECK(host.started == 1);
}

// Regression: a pause refused because the run had just ended was a fault, so
// a finished job became `failed` and its worker was replaced.
TEST_CASE("Pause after the run ended is refused and the job completes",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* hold");
  jobs.poll();
  REQUIRE(jobs.state(id) == sim::JobState::running);
  host.last->end_run();  // its `loaded` event is not applied yet
  const diag::Status paused = jobs.pause(id);
  REQUIRE_FALSE(paused.ok());
  CHECK(paused.error().code == "SIM-ENGINE-010");
  jobs.poll();
  CHECK(jobs.state(id) == sim::JobState::completed);
  CHECK(jobs.engines_started() == 1);
}

TEST_CASE("No engine host: the job fails as unavailable",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  sim::JobManager jobs;
  auto id = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", std::nullopt});
  REQUIRE(id.ok());
  jobs.poll();
  const sim::JobRecord* rec = jobs.record(id.value());
  CHECK(rec->job.state == sim::JobState::failed);
  REQUIRE_FALSE(rec->diagnostics.empty());
  CHECK(rec->diagnostics.front().code == "SIM-ENGINE-031");
  CHECK(rec->diagnostics.front().text == "simulation engine unavailable: no engine host");
}

TEST_CASE("Cancel, pause and resume follow the job states",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId running = submit(jobs, "* hold");
  const sim::JobId queued = submit(jobs, "* ok");
  jobs.poll();
  REQUIRE(jobs.pause(running).ok());
  jobs.poll();
  CHECK(jobs.state(running) == sim::JobState::paused);
  CHECK(jobs.pause(running).error().code == "SIM-ENGINE-035");
  REQUIRE(jobs.resume(running).ok());
  jobs.poll();
  CHECK(jobs.state(running) == sim::JobState::running);
  REQUIRE(jobs.cancel(queued).ok());
  CHECK(jobs.state(queued) == sim::JobState::cancelled);
  REQUIRE(jobs.cancel(running).ok());
  jobs.poll();
  CHECK(jobs.state(running) == sim::JobState::cancelled);
  CHECK(jobs.record(running)->vectors.size() == 1);  // the partial result
  CHECK(jobs.cancel(running).error().code == "SIM-ENGINE-035");
  CHECK(jobs.cancel("nope").error().code == "SIM-ENGINE-034");
  CHECK(host.loads.size() == 1);  // the cancelled queued job never reached the engine
  CHECK(jobs.active() == 0);
}

// Regression: the adapter halts a cancelled run and reports `paused`; the job
// stayed `paused` for ever and kept its worker.
TEST_CASE("Cancel a transient run: the halt ends the job as cancelled and frees the worker",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* hold");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  REQUIRE(jobs.cancel(id).ok());
  jobs.poll();
  CHECK(jobs.state(id) == sim::JobState::cancelled);
  CHECK(jobs.record(id)->vectors.size() == 1);
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.started == 1);
}

// Regression: cancelling a paused job sent no event, so it stayed `paused`.
TEST_CASE("Cancel a paused job: it ends at once as cancelled with the vectors of the pause",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* hold");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  REQUIRE(jobs.pause(id).ok());
  jobs.poll();
  REQUIRE(jobs.state(id) == sim::JobState::paused);
  REQUIRE(jobs.cancel(id).ok());
  CHECK(jobs.state(id) == sim::JobState::cancelled);
  CHECK(jobs.record(id)->vectors.size() == 1);
  jobs.poll();
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.started == 1);
}

TEST_CASE("Five corners on an 8-core Mac: three jobs at once, the next when one finishes",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  FakeHost host;
  sim::JobManager jobs(options(host, kThreeWorkers));
  REQUIRE(jobs.workers() == 3);
  std::vector<sim::JobId> ids;
  for (const char* corner : {"tt", "ff", "ss", "fs", "sf"}) {
    ids.push_back(submit(jobs, "* hold", corner));
  }
  jobs.poll();
  CHECK(jobs.active() == 3);
  CHECK(jobs.state(ids[3]) == sim::JobState::queued);
  REQUIRE(jobs.cancel(ids[0]).ok());
  jobs.poll();
  CHECK(jobs.active() == 3);
  CHECK(jobs.state(ids[3]) == sim::JobState::running);
  CHECK(jobs.state(ids[4]) == sim::JobState::queued);
  CHECK(jobs.peak_active() == 3);
}

TEST_CASE("Setting cannot raise the pool: refused with the maximum, at most 3 jobs at once",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  // Regression (round-0 follow-up): a setting above N must be refused with a
  // diagnostic giving the maximum, not silently ignored.
  FakeHost host;
  sim::PoolInputs pool = kThreeWorkers;
  pool.user_limit = 8;
  sim::JobManager jobs(options(host, pool));
  CHECK(jobs.workers() == 3);
  REQUIRE(jobs.pool_diagnostic().has_value());
  CHECK(jobs.pool_diagnostic()->code == "SIM-ENGINE-033");
  CHECK(jobs.pool_diagnostic()->text.find("maximum 3") != std::string::npos);
  for (int i = 0; i < 8; ++i) {
    submit(jobs, "* hold");
  }
  jobs.poll();
  CHECK(jobs.active() == 3);
  CHECK(jobs.peak_active() == 3);
  CHECK(host.started == 3);
}

TEST_CASE("Worker setting of zero: one job at a time, all complete",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  FakeHost host;
  sim::PoolInputs pool = kThreeWorkers;
  pool.user_limit = 0;
  sim::JobManager jobs(options(host, pool));
  CHECK(jobs.workers() == 1);
  CHECK_FALSE(jobs.pool_diagnostic().has_value());
  std::vector<sim::JobId> ids;
  for (int i = 0; i < 5; ++i) {
    ids.push_back(submit(jobs, "* ok"));
  }
  jobs.poll();
  for (const sim::JobId& id : ids) {
    CHECK(jobs.state(id) == sim::JobState::completed);
  }
  CHECK(jobs.peak_active() == 1);
}

TEST_CASE("Single-core host: a 10-point sweep runs on one worker and completes",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  FakeHost host;
  sim::JobManager jobs(options(host, sim::PoolInputs{1, 16 * kGiB, 4 * kGiB, std::nullopt}));
  CHECK(jobs.workers() == 1);
  std::vector<sim::JobId> ids;
  for (int i = 0; i < 10; ++i) {
    auto id = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", "sw-p" + std::to_string(i)});
    REQUIRE(id.ok());
    ids.push_back(id.value());
  }
  jobs.poll();
  for (const sim::JobId& id : ids) {
    CHECK(jobs.state(id) == sim::JobState::completed);
  }
  CHECK(jobs.peak_active() == 1);
}

TEST_CASE("Large post-layout deck limits the pool: the points run one at a time",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  FakeHost host;
  sim::JobManager jobs(options(host, sim::PoolInputs{10, 16 * kGiB, 4 * kGiB + 300 * kMiB, std::nullopt}));
  CHECK(jobs.workers() == 1);
  for (int i = 0; i < 4; ++i) {
    submit(jobs, "* hold");
  }
  jobs.poll();
  CHECK(jobs.active() == 1);
}

TEST_CASE("Events posted from another thread are applied by poll",
          "[cap:simulation-engine-embedding][req:Engine threading model]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* thread");
  jobs.poll();
  CHECK(jobs.state(id) == sim::JobState::completed);
  CHECK(jobs.record(id)->log == std::vector<sim::LogLine>{{false, "from the engine thread"}});
}
