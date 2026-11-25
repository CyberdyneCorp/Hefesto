// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-simd through the POSIX worker host and WorkerEngine, with the
// stand-in adapter (tests/unit/platform/fake_adapter.cpp): heartbeats during
// a long load, faults, confinement, and the reuse of one worker for many
// jobs (openspec/changes/m9-lvs-simulation design D9; capability
// simulation-engine-embedding). The same tests with the real ngspice adapter
// are in worker_host_ngspice_test.cpp. Owner: SIMD.

#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <thread>
#include <string>
#include <vector>

#include "hefesto/sim/engine/job_manager.hpp"
#include "worker_fixture.hpp"

using hefesto::test::Recorder;
using hefesto::test::TempDir;
using hefesto::test::WorkerOptions;
namespace sim = hefesto::sim;
using namespace std::chrono_literals;

namespace {

WorkerOptions fake(const TempDir& job) {
  WorkerOptions options;
  options.adapter = HEFESTO_FAKE_ADAPTER;
  options.job_dir = job.path().string();
  options.heartbeat_ms = 200;
  options.limits.heartbeat_timeout = 1500ms;
  options.limits.watchdog_period = 20ms;
  return options;
}

std::unique_ptr<sim::Engine> start(hefesto::cli::PosixWorkerHost& host, const WorkerOptions& options,
                                   Recorder& recorder) {
  auto engine = hefesto::test::start_worker(host, options);
  REQUIRE(engine.ok());
  engine.value()->set_callbacks(recorder.callbacks());
  return std::move(engine).value();
}

std::vector<std::string> job_deck(int index) {
  return {"* job " + std::to_string(index), "* fake: points 5", "R1 a 0 " + std::to_string(index + 1) + "k"};
}

// A listening TCP socket on 127.0.0.1; pending() tells whether anyone connected.
class Listener {
 public:
  Listener() : fd_(::socket(AF_INET, SOCK_STREAM, 0)) {
    REQUIRE(fd_ >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::bind(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof address) == 0);  // NOLINT
    REQUIRE(::listen(fd_, 4) == 0);
    socklen_t size = sizeof address;
    REQUIRE(::getsockname(fd_, reinterpret_cast<sockaddr*>(&address), &size) == 0);  // NOLINT
    port_ = ntohs(address.sin_port);
    ::fcntl(fd_, F_SETFL, O_NONBLOCK);
  }
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  ~Listener() { ::close(fd_); }
  [[nodiscard]] int port() const noexcept { return port_; }
  [[nodiscard]] bool pending() const {
    const int client = ::accept(fd_, nullptr, nullptr);
    if (client >= 0) {
      ::close(client);
    }
    return client >= 0;
  }

 private:
  int fd_;
  int port_ = 0;
};

}  // namespace

TEST_CASE("hefesto-simd says hello and serves a job through the worker host",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const TempDir job{"simd-job"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, fake(job), recorder);
  const std::vector<sim::Vector> vectors = hefesto::test::run_job(*engine, recorder, job_deck(0));
  REQUIRE(vectors.size() == 2);
  CHECK(vectors[0].name == "time");
  CHECK(vectors[1].name == "v(out)");
  CHECK(vectors[1].type == "voltage");
  CHECK(vectors[1].real.size() == 5);
  const auto states = recorder.states();
  CHECK(std::find(states.begin(), states.end(), sim::EngineState::running) != states.end());
}

TEST_CASE("Large deck load: heartbeats continue while the engine loads",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  // Scaled from the scenario's 25 s load and 10 s timeout: a 3 s load, a 200 ms
  // heartbeat and a 1.5 s timeout. The load blocks the engine thread only.
  const TempDir job{"simd-large"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, fake(job), recorder);
  const auto started = std::chrono::steady_clock::now();
  const auto loaded = engine->load({"* big deck", "* fake: sleep-load 3000"});
  REQUIRE(loaded.ok());
  CHECK(std::chrono::steady_clock::now() - started >= 3s);
  REQUIRE(engine->run().ok());
  REQUIRE(recorder.wait_for(sim::EngineState::loaded));
  const auto states = recorder.states();
  CHECK(std::find(states.begin(), states.end(), sim::EngineState::failed) == states.end());
}

TEST_CASE("Stuck worker: a stopped worker misses its heartbeat and the next job runs in a new one",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  const TempDir job{"simd-stuck"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  auto engine = start(host, fake(job), recorder);
  REQUIRE(engine->load({"* fake: stop"}).ok());
  (void)engine->run();  // the worker stops itself (SIGSTOP) while running
  REQUIRE(recorder.wait_for(sim::EngineState::failed, 10s));
  CHECK(engine->vectors().error().code == "SIM-ENGINE-007");
  CHECK(engine->vectors().error().text.starts_with("simulation engine terminated"));
  engine.reset();
  Recorder next;
  const auto replacement = start(host, fake(job), next);
  CHECK(hefesto::test::run_job(*replacement, next, job_deck(1)).size() == 2);
}

TEST_CASE("ngspice segmentation fault: a crashing worker fails the job and is replaced",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const TempDir job{"simd-crash"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  auto engine = start(host, fake(job), recorder);
  REQUIRE(engine->load({"* fake: crash"}).ok());
  (void)engine->run();
  REQUIRE(recorder.wait_for(sim::EngineState::failed, 10s));
  const auto error = engine->vectors().error();
  CHECK(error.code == "SIM-ENGINE-008");
  CHECK(error.text.starts_with("simulation engine terminated"));
  CHECK(engine->load({"* again"}).error().code == "SIM-ENGINE-008");
  engine.reset();
  Recorder next;
  const auto replacement = start(host, fake(job), next);
  CHECK(hefesto::test::run_job(*replacement, next, job_deck(2)).size() == 2);
}

TEST_CASE("Controlled exit replaces the worker: the job fails with the error lines",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const TempDir job{"simd-exit"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, fake(job), recorder);
  REQUIRE(engine->load({"* fake: exit"}).ok());
  (void)engine->run();
  REQUIRE(recorder.wait_for(sim::EngineState::failed, 10s));
  const auto error = engine->vectors().error();
  CHECK(error.code == "SIM-ENGINE-008");
  CHECK(error.text.find("Error: controlled exit requested by the deck") != std::string::npos);
}

TEST_CASE("Error stream classification: error-stream lines reach the job log as they are",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  const TempDir job{"simd-stderr"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, fake(job), recorder);
  REQUIRE(engine->load({"* fake: stderr Error: unknown subckt: xinv"}).ok());
  const auto lines = recorder.output();
  CHECK(std::find(lines.begin(), lines.end(), "Error: unknown subckt: xinv") != lines.end());
}

TEST_CASE("Error stream classification: error-stream lines reach on_error, the rest on_output",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  const TempDir job{"simd-onerror"};
  hefesto::cli::PosixWorkerHost host;
  std::mutex mutex;  // declared before the engine, whose reader thread calls the callbacks
  std::vector<std::string> output;
  std::vector<std::string> errors;
  auto engine = hefesto::test::start_worker(host, fake(job));
  REQUIRE(engine.ok());
  sim::EngineCallbacks callbacks;
  callbacks.on_output = [&](std::string_view line) {
    const std::lock_guard lock{mutex};
    output.emplace_back(line);
  };
  callbacks.on_error = [&](std::string_view line) {
    const std::lock_guard lock{mutex};
    errors.emplace_back(line);
  };
  engine.value()->set_callbacks(std::move(callbacks));
  REQUIRE(engine.value()->load({"* fake: stderr Error: unknown subckt: xinv"}).ok());
  const std::lock_guard lock{mutex};
  CHECK(errors == std::vector<std::string>{"Error: unknown subckt: xinv"});
  CHECK(std::find(output.begin(), output.end(), "Error: unknown subckt: xinv") == output.end());
}

// Regression (M9 R1 integration): JobManager classified error-stream lines by
// a `stderr ` prefix that WorkerEngine never sends, so an `Error` line of a
// real worker left the job `completed`.
TEST_CASE("Error stream classification: an Error line of a worker run fails the job in the job manager",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  const TempDir job{"simd-jobmanager"};
  hefesto::cli::PosixWorkerHost host;
  sim::JobManagerOptions options;
  options.make_engine = [&host, &job]() { return hefesto::test::start_worker(host, fake(job)); };
  sim::JobManager jobs(std::move(options));
  sim::PreparedDeck deck;
  deck.lines = {"* fake: stderr Error: unknown subckt: xinv", "* fake: points 5", "R1 a 0 1k"};
  const auto id = jobs.submit(std::move(deck), sim::JobOptions{"tb", "tt", std::nullopt});
  REQUIRE(id.ok());
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (std::chrono::steady_clock::now() < deadline) {
    jobs.poll();
    const auto state = jobs.state(id.value());
    if (state == sim::JobState::completed || state == sim::JobState::failed) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  const sim::JobRecord* rec = jobs.record(id.value());
  REQUIRE(rec != nullptr);
  CHECK(rec->job.state == sim::JobState::failed);
  CHECK(rec->errors == std::vector<std::string>{"Error: unknown subckt: xinv"});
}

TEST_CASE("Deck tries to reach the network: the confined worker cannot connect, spawn or write outside the job",
          "[cap:simulation-engine-embedding][req:Worker confinement]") {
  const TempDir job{"simd-confined-job"};
  const TempDir home{"simd-confined-home"};
  const Listener listener;
  const std::filesystem::path outside = home.path() / "escaped.txt";
  const std::filesystem::path inside = job.path() / "allowed.txt";
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  WorkerOptions options = fake(job);
  options.readonly_roots = {home.path().string()};
  const auto engine = start(host, options, recorder);
  REQUIRE(engine->load({"* deck that misbehaves", "* fake: net " + std::to_string(listener.port()),
                        "* fake: write " + outside.string(), "* fake: write " + inside.string(), "* fake: spawn"})
              .ok());
  CHECK(recorder.said("confinement: "));
  CHECK(recorder.said("net: denied"));
  CHECK_FALSE(listener.pending());
  CHECK(recorder.said("spawn: denied"));
  CHECK(recorder.said("write: ok"));  // the job directory stays writable
  CHECK(std::filesystem::exists(inside));
  if (recorder.said("confinement: Landlock unavailable")) {
    WARN("Landlock is unavailable on this kernel: file writes are not confined (noted in the job log)");
  } else {
    CHECK(recorder.said("write: denied"));
    CHECK_FALSE(std::filesystem::exists(outside));
  }
  // The job completes despite the denied attempts.
  REQUIRE(engine->run().ok());
  CHECK(recorder.wait_for(sim::EngineState::loaded));
}

TEST_CASE("Reproducible random sources: setseed precedes every load in a fresh or reused worker",
          "[cap:simulation-engine-embedding][req:Deterministic engine initialisation]") {
  const TempDir job{"simd-seed"};
  hefesto::cli::PosixWorkerHost host;
  std::vector<std::vector<sim::Vector>> runs;
  for (int i = 0; i < 2; ++i) {
    Recorder recorder;
    const auto engine = start(host, fake(job), recorder);
    runs.push_back(hefesto::test::run_job(*engine, recorder, job_deck(7)));
    const auto lines = recorder.output();
    const auto seed = std::find(lines.begin(), lines.end(), "cmd: setseed 1");
    const auto loaded = std::find_if(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("loaded "); });
    REQUIRE(seed != lines.end());
    CHECK(seed < loaded);
  }
  REQUIRE(runs[0].size() == runs[1].size());
  for (std::size_t v = 0; v < runs[0].size(); ++v) {
    CHECK(runs[0][v].real == runs[1][v].real);
  }
}

TEST_CASE("Memory released between jobs: a reused worker clears the circuit and plots before the next load",
          "[cap:simulation-engine-embedding][req:Single engine instance per address space]") {
  const TempDir job{"simd-clear"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, fake(job), recorder);
  hefesto::test::run_job(*engine, recorder, job_deck(1));
  hefesto::test::run_job(*engine, recorder, job_deck(2));
  const auto lines = recorder.output();
  const auto remcirc = std::find(lines.begin(), lines.end(), "cmd: remcirc");
  const auto destroy = std::find(lines.begin(), lines.end(), "cmd: destroy all");
  REQUIRE(remcirc != lines.end());
  REQUIRE(destroy != lines.end());
  CHECK(remcirc < destroy);
  // ...and the second job's setseed comes after the clear.
  const auto second_seed = std::find(destroy, lines.end(), "cmd: setseed 1");
  CHECK(second_seed != lines.end());
}

TEST_CASE("50 jobs in one worker equal the same jobs each in a fresh worker",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const TempDir job{"simd-reuse"};
  hefesto::cli::PosixWorkerHost host;
  constexpr int kJobs = 50;
  std::vector<std::vector<sim::Vector>> reused;
  {
    Recorder recorder;
    const auto engine = start(host, fake(job), recorder);
    for (int i = 0; i < kJobs; ++i) {
      reused.push_back(hefesto::test::run_job(*engine, recorder, job_deck(i)));
    }
  }
  for (int i = 0; i < kJobs; ++i) {
    Recorder recorder;
    const auto engine = start(host, fake(job), recorder);
    const std::vector<sim::Vector> fresh = hefesto::test::run_job(*engine, recorder, job_deck(i));
    REQUIRE(fresh.size() == reused[static_cast<std::size_t>(i)].size());
    for (std::size_t v = 0; v < fresh.size(); ++v) {
      CHECK(fresh[v].real == reused[static_cast<std::size_t>(i)][v].real);  // bit for bit
    }
  }
}

TEST_CASE("a vector longer than one frame arrives in chunks",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  const TempDir job{"simd-chunks"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, fake(job), recorder);
  const auto vectors = hefesto::test::run_job(*engine, recorder, {"* fake: points 2500000"});
  REQUIRE(vectors.size() == 2);
  REQUIRE(vectors[0].real.size() == 2500000);
  CHECK(vectors[0].real[2499999] == 2499999 * 1e-9);
}
