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
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
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

/// What the fake engines did, shared by every engine of one factory. Since
/// M9 R2 the factory, load and run run on the manager's loader threads, so
/// the counters are guarded; the tests read them after settle().
struct FakeHost {
  std::mutex mutex;
  std::uint64_t started = 0;
  std::uint64_t alive = 0;
  std::vector<std::uint64_t> loads;  // the engine number of each load, in order
  std::uint64_t cancels = 0;
  std::uint64_t runs = 0;
  class FakeEngine* last = nullptr;  // the engine started last
  sim::LoadOptions last_options;     // of the last load
  // `* slow-load` and `* slow-run` (and the factory when block_factory is
  // set before the first submit) wait until the gate opens.
  std::condition_variable gate_changed;
  bool gate_open = true;
  bool block_factory = false;

  void wait_gate() {
    std::unique_lock lock{mutex};
    gate_changed.wait(lock, [this] { return gate_open; });
  }
  void close_gate() {
    const std::lock_guard lock{mutex};
    gate_open = false;
  }
  void open_gate() {
    {
      const std::lock_guard lock{mutex};
      gate_open = true;
    }
    gate_changed.notify_all();
  }
};

class FakeEngine final : public sim::Engine {
 public:
  FakeEngine(FakeHost& host, std::uint64_t number) : host_(host), number_(number) {
    const std::lock_guard lock{host_.mutex};
    ++host_.alive;
    host_.last = this;
  }
  ~FakeEngine() override {
    const std::lock_guard lock{host_.mutex};
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

  diag::Status load(const std::vector<std::string>& deck, const sim::LoadOptions& options) override {
    {
      const std::lock_guard lock{host_.mutex};
      host_.loads.push_back(number_);
      host_.last_options = options;
    }
    script_ = deck.empty() ? std::string{} : deck.front();
    if (script_ == "* slow-load") {
      host_.wait_gate();  // a deck load of tens of seconds
    }
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
    {
      const std::lock_guard lock{host_.mutex};
      ++host_.runs;
    }
    callbacks_.on_state(sim::EngineState::running);
    running_ = script_ == "* hold" || script_ == "* slow-run" || script_ == "* progress" || script_ == "* stream";
    if (script_ == "* slow-run") {
      host_.wait_gate();  // the run started; the loader has not returned yet
    }
    if (script_ == "* ok" || script_ == "* slow-load") {
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
    } else if (script_ == "* flood" || script_ == "* flood-fail") {
      // A hostile worker: 200000 state frames in one run.
      for (int i = 0; i < 100000; ++i) {
        callbacks_.on_state(sim::EngineState::paused);
        callbacks_.on_state(sim::EngineState::running);
        if (i == 50000 && script_ == "* flood-fail") {
          callbacks_.on_state(sim::EngineState::failed);
        }
      }
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* stream") {
      // M9 R3: 500 points of time and v(out) (1000 values, 8000 bytes), then
      // the analysis keeps running until it is cancelled.
      for (int i = 0; i < 500; ++i) {
        callbacks_.on_data("time", 1e-12 * i);
        callbacks_.on_data("v(out)", 0.5);
      }
    } else if (script_ == "* big") {
      callbacks_.on_state(sim::EngineState::loaded);  // 2000 real points: 16000 bytes
    } else if (script_ == "* log") {
      for (int i = 0; i < 10; ++i) {
        callbacks_.on_output(std::string(100, static_cast<char>('a' + i)));
      }
      callbacks_.on_state(sim::EngineState::loaded);
    } else if (script_ == "* progress") {
      callbacks_.on_progress("op", -1.0);
      callbacks_.on_progress("tran", 50.0);
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

  /// One output line of the running job, as its worker sends it.
  void send_line(std::string_view line) { callbacks_.on_output(line); }

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
    if (script_ == "* big") {
      return std::vector<sim::Vector>{sim::Vector{"v(out)", "voltage", false, std::vector<double>(2000, 1.0), {}}};
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
    if (d.text.find(text) != std::string::npos) {
      ++n;
    }
  }
  return n;
}

sim::JobManagerOptions options(FakeHost& host, sim::PoolInputs pool = {}) {
  sim::JobManagerOptions o;
  o.make_engine = [&host]() -> diag::Outcome<std::unique_ptr<sim::Engine>> {
    if (host.block_factory) {
      host.wait_gate();  // a worker handshake of up to 10 s
    }
    std::uint64_t number = 0;
    {
      const std::lock_guard lock{host.mutex};
      number = ++host.started;
    }
    return std::unique_ptr<sim::Engine>(std::make_unique<FakeEngine>(host, number));
  };
  o.pool = pool;
  o.now = [] { return std::chrono::system_clock::time_point{} + std::chrono::hours(24 * 365 * 50); };
  return o;
}

/// Polls until no job is loading and nothing is left to apply (M9 R2: the
/// loads run on loader threads). Bounded by time, not by poll count: a
/// flooding engine thread keeps every poll busy, and under ThreadSanitizer
/// its 200 000 frames take longer than a fixed number of polls.
void settle(sim::JobManager& jobs) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (std::chrono::steady_clock::now() < deadline) {
    jobs.poll_for(std::chrono::milliseconds(5));
    if (jobs.loading() == 0 && jobs.poll() == 0) {
      return;
    }
  }
  FAIL("the job manager did not settle");
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
  CHECK(jobs.state(a) == sim::JobState::running);
  CHECK(jobs.state(b) == sim::JobState::queued);
  REQUIRE(jobs.cancel(a).ok());
  settle(jobs);
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
  settle(jobs);
  CHECK(host.started == 1);
  CHECK(host.loads == std::vector<std::uint64_t>(10, 1));
}

TEST_CASE("ngspice segmentation fault: the job fails and the next job runs in a new engine",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId crashed = submit(jobs, "* segv");
  const sim::JobId next = submit(jobs, "* ok");
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
  REQUIRE(jobs.state(id) == sim::JobState::running);
  host.last->end_run();  // its `loaded` event is not applied yet
  const diag::Status paused = jobs.pause(id);
  REQUIRE_FALSE(paused.ok());
  CHECK(paused.error().code == "SIM-ENGINE-010");
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::completed);
  CHECK(jobs.engines_started() == 1);
}

TEST_CASE("No engine host: the job fails as unavailable",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  sim::JobManager jobs;
  auto id = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", std::nullopt});
  REQUIRE(id.ok());
  settle(jobs);
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
  settle(jobs);
  REQUIRE(jobs.pause(running).ok());
  settle(jobs);
  CHECK(jobs.state(running) == sim::JobState::paused);
  CHECK(jobs.pause(running).error().code == "SIM-ENGINE-035");
  REQUIRE(jobs.resume(running).ok());
  settle(jobs);
  CHECK(jobs.state(running) == sim::JobState::running);
  REQUIRE(jobs.cancel(queued).ok());
  CHECK(jobs.state(queued) == sim::JobState::cancelled);
  REQUIRE(jobs.cancel(running).ok());
  settle(jobs);
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
  settle(jobs);
  REQUIRE(jobs.cancel(id).ok());
  settle(jobs);
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
  settle(jobs);
  REQUIRE(jobs.pause(id).ok());
  settle(jobs);
  REQUIRE(jobs.state(id) == sim::JobState::paused);
  REQUIRE(jobs.cancel(id).ok());
  CHECK(jobs.state(id) == sim::JobState::cancelled);
  CHECK(jobs.record(id)->vectors.size() == 1);
  settle(jobs);
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
  settle(jobs);
  CHECK(jobs.active() == 3);
  CHECK(jobs.state(ids[3]) == sim::JobState::queued);
  REQUIRE(jobs.cancel(ids[0]).ok());
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
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
  settle(jobs);
  CHECK(jobs.active() == 1);
}

TEST_CASE("Events posted from another thread are applied by poll",
          "[cap:simulation-engine-embedding][req:Engine threading model]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* thread");
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::completed);
  CHECK(jobs.record(id)->log == std::vector<sim::LogLine>{{false, "from the engine thread"}});
}

// --- M9 R2 ---------------------------------------------------------------------------

// Regression (security, M9 R1 follow-up): state events were queued without a
// count limit, so a worker flooding state frames grew the queue until the
// next poll without bound (200000 events here).
TEST_CASE("A worker flooding state frames cannot grow the event queue and the job completes",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* flood");
  const sim::JobId next = submit(jobs, "* ok");
  settle(jobs);
  CHECK(jobs.peak_queued_events() <= sim::kMaxQueuedStateEvents + 8);
  CHECK(jobs.state(id) == sim::JobState::completed);
  CHECK(jobs.record(id)->vectors.size() == 1);
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.started == 1);
}

TEST_CASE("A fault inside a flood of state frames still replaces the worker",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* flood-fail");
  const sim::JobId next = submit(jobs, "* ok");
  settle(jobs);
  CHECK(jobs.peak_queued_events() <= sim::kMaxQueuedStateEvents + 8);
  CHECK(jobs.state(id) == sim::JobState::failed);
  CHECK(has_code(*jobs.record(id), "SIM-ENGINE-030", "simulation engine terminated"));
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.started == 2);
}

// Regression (security, M9 R2 review): poll() applied event batches until
// one pass found nothing, so a worker that kept sending frames held the
// owning thread inside poll() for as long as it sent.
TEST_CASE("poll() returns while a worker is still sending",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  FakeHost host;
  sim::JobManagerOptions o = options(host);
  // A worker that always has sent more by the time a batch is taken: each
  // taken batch is followed by a new line from the running job's engine.
  // Bounded at 1000 lines, so that a poll() without its own bound ends.
  std::size_t taken = 0;
  bool sending = false;
  o.on_events_taken = [&] {
    ++taken;
    if (sending && taken < 1000) {
      host.last->send_line("Note: still sending");
    }
  };
  sim::JobManager jobs(std::move(o));
  const sim::JobId id = submit(jobs, "* hold");
  settle(jobs);
  REQUIRE(jobs.state(id) == sim::JobState::running);
  sending = true;
  taken = 0;
  host.last->send_line("Note: first");
  jobs.poll();
  CHECK(taken == sim::kMaxPollPasses);
  sending = false;
  settle(jobs);
  REQUIRE(jobs.cancel(id).ok());
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::cancelled);
}

// Regression (M9 R2 review): job ids were not checked, and a job id names
// the job's folder in the result store.
TEST_CASE("A job id that is not one safe path component is refused",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  const auto bad_bench = jobs.submit(deck("* ok"), sim::JobOptions{"../../x", "tt", std::nullopt});
  REQUIRE_FALSE(bad_bench.ok());
  CHECK(bad_bench.error().code == "SIM-ENGINE-038");
  const auto bad_point = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", std::string{".."}});
  CHECK(bad_point.error().code == "SIM-ENGINE-038");
  const auto absolute = jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", std::string{"/tmp/x"}});
  CHECK(absolute.error().code == "SIM-ENGINE-038");
  CHECK(jobs.submit(deck("* ok"), sim::JobOptions{"tb", "tt", std::string{"rs1-p0"}}).ok());
}

// M9 R1 follow-up: poll() ran the factory (the worker handshake) and the
// load on the owning thread, so a slow start blocked every other job.
TEST_CASE("A slow worker handshake does not block poll()",
          "[cap:simulation-engine-embedding][req:Engine threading model]") {
  FakeHost host;
  host.block_factory = true;
  host.close_gate();
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* ok");
  const auto before = std::chrono::steady_clock::now();
  jobs.poll();
  jobs.poll_for(std::chrono::milliseconds(20));
  CHECK(std::chrono::steady_clock::now() - before < std::chrono::seconds(5));
  CHECK(jobs.state(id) == sim::JobState::loading);
  CHECK(jobs.loading() == 1);
  host.open_gate();
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::completed);
  CHECK(jobs.engines_started() == 1);
}

TEST_CASE("A slow deck load does not hold back a job on another worker",
          "[cap:simulation-engine-embedding][req:Engine threading model]") {
  FakeHost host;
  host.close_gate();
  sim::JobManager jobs(options(host, kThreeWorkers));
  const sim::JobId slow = submit(jobs, "* slow-load");
  const sim::JobId quick = submit(jobs, "* ok");
  for (int i = 0; i < 2000 && jobs.state(quick) != sim::JobState::completed; ++i) {
    jobs.poll_for(std::chrono::milliseconds(5));
  }
  CHECK(jobs.state(quick) == sim::JobState::completed);
  CHECK(jobs.state(slow) == sim::JobState::loading);
  host.open_gate();
  settle(jobs);
  CHECK(jobs.state(slow) == sim::JobState::completed);
}

TEST_CASE("Cancel during a load returns at once and the job ends cancelled without running",
          "[cap:simulation-analyses-and-results][req:Pause, resume and cancel]") {
  FakeHost host;
  host.close_gate();
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* slow-load");
  const sim::JobId next = submit(jobs, "* ok");
  jobs.poll();
  REQUIRE(jobs.state(id) == sim::JobState::loading);
  CHECK(jobs.pause(id).error().code == "SIM-ENGINE-035");
  REQUIRE(jobs.cancel(id).ok());  // the gate is closed: it cannot have waited for the load
  host.open_gate();
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::cancelled);
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(host.runs == 1);     // only the next job ran
  CHECK(host.started == 1);  // the healthy engine was kept
}

TEST_CASE("Cancel after the run started but before the loader returned cancels the run",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host;
  host.close_gate();
  sim::JobManager jobs(options(host));
  const sim::JobId id = submit(jobs, "* slow-run");
  jobs.poll();
  for (int i = 0; i < 2000; ++i) {  // until the loader has called run()
    {
      const std::lock_guard lock{host.mutex};
      if (host.runs == 1) {
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(jobs.cancel(id).ok());
  host.open_gate();
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::cancelled);
  CHECK(jobs.record(id)->vectors.size() == 1);  // the partial result
  CHECK(host.cancels == 1);
  CHECK(jobs.active() == 0);
}

TEST_CASE("The job's seed and the descriptor's initialisation commands reach Engine::load",
          "[cap:simulation-engine-embedding][req:Deterministic engine initialisation]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  sim::PreparedDeck d = deck("* ok");
  d.descriptor.init_commands = {"set ngbehavior=hsa"};
  auto id = jobs.submit(std::move(d), sim::JobOptions{"tb", "tt", std::nullopt, 42});
  REQUIRE(id.ok());
  settle(jobs);
  CHECK(jobs.state(id.value()) == sim::JobState::completed);
  CHECK(host.last_options.seed == 42);
  CHECK(host.last_options.init_commands == std::vector<std::string>{"set ngbehavior=hsa"});
}

TEST_CASE("Two analyses: op completed and tran at 50 percent is 0.75",
          "[cap:simulation-analyses-and-results][req:Progress reporting]") {
  FakeHost host;
  sim::JobManager jobs(options(host));
  sim::PreparedDeck d;
  d.lines = {"* progress", ".op", ".tran 1n 10n", ".end"};
  auto id = jobs.submit(std::move(d), sim::JobOptions{"tb", "tt", std::nullopt});
  REQUIRE(id.ok());
  settle(jobs);
  const sim::JobRecord* rec = jobs.record(id.value());
  CHECK(rec->job.state == sim::JobState::running);
  CHECK(rec->progress == 0.75);
  CHECK(rec->status == "tran");
  REQUIRE(jobs.cancel(id.value()).ok());
  settle(jobs);
}

// --- M9 R3: the job log cap and the result budget (Q-SIM-25, Q-SIM-16; approved 2026-11-27) ---

TEST_CASE("The job log keeps lines up to its cap and counts the dropped ones",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]") {
  for (const auto& [cap, kept] : {std::pair<std::size_t, std::size_t>{300, 3}, {299, 2}}) {
    FakeHost host;
    sim::JobManagerOptions o = options(host);
    o.max_log_bytes = cap;  // at the cap (3 lines of 100 bytes) and one byte under it
    sim::JobManager jobs(std::move(o));
    CHECK_FALSE(jobs.limits_diagnostic().has_value());
    const sim::JobId id = submit(jobs, "* log");
    settle(jobs);
    const sim::JobRecord* rec = jobs.record(id);
    REQUIRE(rec != nullptr);
    CHECK(rec->job.state == sim::JobState::completed);
    CHECK(rec->log.size() == kept);
    CHECK(rec->log_bytes == kept * 100);
    CHECK(rec->log_dropped == 10 - kept);
  }
}

TEST_CASE("Job limit settings can be lowered but not raised past twice the default",
          "[cap:simulation-analyses-and-results][req:Job lifecycle]"
          "[cap:simulation-analyses-and-results][req:Saved vectors and memory budget]") {
  FakeHost host;
  SECTION("twice the defaults are accepted") {
    sim::JobManagerOptions o = options(host);
    o.max_log_bytes = 2 * sim::kMaxJobLogBytes;
    o.result_budget = 2 * sim::kDesktopResultBudget;
    sim::JobManager jobs(std::move(o));
    CHECK_FALSE(jobs.limits_diagnostic().has_value());
    CHECK(jobs.max_log_bytes() == 2 * sim::kMaxJobLogBytes);
    CHECK(jobs.result_budget() == 2 * sim::kDesktopResultBudget);
  }
  SECTION("above twice the default or zero is refused and the default is used") {
    sim::JobManagerOptions o = options(host);
    o.max_log_bytes = 2 * sim::kMaxJobLogBytes + 1;
    sim::JobManager raised(std::move(o));
    REQUIRE(raised.limits_diagnostic().has_value());
    CHECK(raised.limits_diagnostic()->code == "SIM-ENGINE-039");
    CHECK(raised.max_log_bytes() == sim::kMaxJobLogBytes);
    sim::JobManagerOptions z = options(host);
    z.result_budget = 0;
    sim::JobManager zero(std::move(z));
    REQUIRE(zero.limits_diagnostic().has_value());
    CHECK(zero.limits_diagnostic()->text.find("result budget 0 is refused") != std::string::npos);
    CHECK(zero.result_budget() == sim::kDesktopResultBudget);
  }
}

TEST_CASE("A new job manager has the default result budget of 4 GiB",
          "[cap:simulation-analyses-and-results][req:Saved vectors and memory budget]") {
  // Scenario "Default result budget" (R3-6): Linux and macOS, no setting.
  FakeHost host;
  sim::JobManager jobs(options(host));
  CHECK_FALSE(jobs.limits_diagnostic().has_value());
  CHECK(jobs.result_budget() == std::uint64_t{4294967296});
  CHECK(sim::kDesktopResultBudget == std::uint64_t{4294967296});
}

TEST_CASE("The iPadOS default result budget is 512 MiB",
          "[cap:simulation-analyses-and-results][req:Saved vectors and memory budget]") {
  // Scenario "Default result budget", iPadOS half (M9 R3 fix pass): the
  // default and the 2x ceiling follow the platform.
  CHECK(sim::default_result_budget(true) == std::uint64_t{536870912});
  CHECK(sim::default_result_budget(false) == sim::kDesktopResultBudget);
  // This test binary is a desktop build: the job manager uses the desktop
  // default and refuses more than twice it.
  CHECK(sim::kPlatformResultBudget == sim::kDesktopResultBudget);
  FakeHost host;
  sim::JobManagerOptions o = options(host);
  o.result_budget = 2 * sim::kPlatformResultBudget + 1;
  sim::JobManager jobs(std::move(o));
  REQUIRE(jobs.limits_diagnostic().has_value());
  CHECK(jobs.result_budget() == sim::kPlatformResultBudget);
}

TEST_CASE("A run whose streamed data passes the result budget fails and keeps its partial result",
          "[cap:simulation-analyses-and-results][req:Saved vectors and memory budget]") {
  // Regression (task 13.3 open item): the budget was only enforced by the
  // worker client as a fault after the final vectors; a streaming run went on.
  FakeHost host;
  sim::JobManagerOptions o = options(host);
  o.result_budget = 4000;  // the run streams 1000 values: 8000 bytes
  sim::JobManager jobs(std::move(o));
  const sim::JobId id = submit(jobs, "* stream");
  settle(jobs);
  const sim::JobRecord* rec = jobs.record(id);
  REQUIRE(rec != nullptr);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(has_code(*rec, "SIM-RESULT-011", "result memory budget exceeded (8000 > 4000)"));
  CHECK(rec->budget_exceeded == 8000);
  CHECK(rec->partial);
  CHECK_FALSE(rec->vectors.empty());
  CHECK(host.cancels == 1);
  CHECK(jobs.engines_started() == 1);
  // The engine is healthy: the next job reuses it.
  const sim::JobId next = submit(jobs, "* ok");
  settle(jobs);
  CHECK(jobs.state(next) == sim::JobState::completed);
  CHECK(jobs.engines_started() == 1);
}

TEST_CASE("A streaming run within the result budget keeps running",
          "[cap:simulation-analyses-and-results][req:Saved vectors and memory budget]") {
  FakeHost host;
  sim::JobManagerOptions o = options(host);
  o.result_budget = 8000;  // exactly the streamed estimate: not exceeded
  sim::JobManager jobs(std::move(o));
  const sim::JobId id = submit(jobs, "* stream");
  settle(jobs);
  CHECK(jobs.state(id) == sim::JobState::running);
  REQUIRE(jobs.cancel(id).ok());
  settle(jobs);
  const sim::JobRecord* rec = jobs.record(id);
  REQUIRE(rec != nullptr);
  CHECK(rec->job.state == sim::JobState::cancelled);
  CHECK(rec->partial);
  CHECK(rec->budget_exceeded == 0);
}

TEST_CASE("Final vectors above the result budget fail the job with the budget diagnostic",
          "[cap:simulation-analyses-and-results][req:Saved vectors and memory budget]") {
  for (const auto& [budget, state] : {std::pair<std::uint64_t, sim::JobState>{16000, sim::JobState::completed},
                                      {15999, sim::JobState::failed}}) {
    FakeHost host;
    sim::JobManagerOptions o = options(host);
    o.result_budget = budget;
    sim::JobManager jobs(std::move(o));
    const sim::JobId id = submit(jobs, "* big");
    settle(jobs);
    const sim::JobRecord* rec = jobs.record(id);
    REQUIRE(rec != nullptr);
    CHECK(rec->job.state == state);
    CHECK(rec->partial == (state == sim::JobState::failed));
    CHECK(has_code(*rec, "SIM-RESULT-011", "result memory budget exceeded (16000 > 15999)") ==
          (state == sim::JobState::failed));
    CHECK(rec->vectors.size() == 1);
  }
}
