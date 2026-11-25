// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-simd with the real hefesto-ngspice-adapter plugin (built with
// HEFESTO_BUILD_NGSPICE=ON), found next to the worker like an installed one
// (openspec/changes/m9-lvs-simulation design D2, D9; capability
// simulation-engine-embedding). Owner: SIMD.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "worker_fixture.hpp"

using hefesto::test::Recorder;
using hefesto::test::TempDir;
using hefesto::test::WorkerOptions;
namespace sim = hefesto::sim;
using namespace std::chrono_literals;

namespace {

WorkerOptions real(const TempDir& job) {
  WorkerOptions options;  // no --adapter: the plugin next to hefesto-simd
  options.job_dir = job.path().string();
  return options;
}

std::unique_ptr<sim::Engine> start(hefesto::cli::PosixWorkerHost& host, const WorkerOptions& options,
                                   Recorder& recorder) {
  auto engine = hefesto::test::start_worker(host, options);
  REQUIRE(engine.ok());
  engine.value()->set_callbacks(recorder.callbacks());
  return std::move(engine).value();
}

const sim::Vector* find(const std::vector<sim::Vector>& vectors, const std::string& name) {
  const auto it = std::find_if(vectors.begin(), vectors.end(), [&](const sim::Vector& v) { return v.name == name; });
  return it == vectors.end() ? nullptr : &*it;
}

std::vector<std::string> rc_deck(int ohms) {
  return {"* rc low-pass",
          "V1 in 0 PULSE(0 1 0 1n 1n 1u 2u)",
          "R1 in out " + std::to_string(ohms),
          "C1 out 0 1n",
          ".tran 10n 2u",
          ".end"};
}

// trnoise and an agauss parameter: the random draws of "Reproducible random sources".
std::vector<std::string> noise_deck(int index) {
  return {"* noise " + std::to_string(index),
          ".param p = 'agauss(1, 0.1, 1)'",
          "Vn n 0 dc 0 trnoise(1m 1n 0 0)",
          "Rn n 0 1k",
          "Vp pv 0 dc {p}",
          "Rp pv 0 " + std::to_string(1 + index) + "k",
          ".tran 1n 100n",
          ".end"};
}

}  // namespace

TEST_CASE("Engine adapter boundary: an RC transient runs in the worker and its vectors are copied out",
          "[cap:simulation-engine-embedding][req:Engine adapter boundary]") {
  const TempDir job{"ng-rc"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, real(job), recorder);
  const std::vector<sim::Vector> vectors = hefesto::test::run_job(*engine, recorder, rc_deck(1000));
  const sim::Vector* time = find(vectors, "time");
  const sim::Vector* out = find(vectors, "out");
  REQUIRE(time != nullptr);
  REQUIRE(out != nullptr);
  CHECK(time->type == "time");
  CHECK(out->type == "voltage");
  REQUIRE(out->real.size() == time->real.size());
  REQUIRE(out->real.size() > 10);
  CHECK(time->real.back() == 2e-6);
  // tau = 1 us: at 1 us the output has charged to about 1 - 1/e.
  const auto at_1us = std::lower_bound(time->real.begin(), time->real.end(), 1e-6) - time->real.begin();
  CHECK(std::abs(out->real[static_cast<std::size_t>(at_1us)] - 0.632) < 0.02);
}

TEST_CASE("Thread state: running, then out of running",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  const TempDir job{"ng-state"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, real(job), recorder);
  hefesto::test::run_job(*engine, recorder, rc_deck(1000));
  const auto states = recorder.states();
  const auto running = std::find(states.begin(), states.end(), sim::EngineState::running);
  REQUIRE(running != states.end());
  CHECK(std::find(running, states.end(), sim::EngineState::loaded) != states.end());
}

TEST_CASE("Error stream classification: the stderr prefix is removed",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  const TempDir job{"ng-stderr"};
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, real(job), recorder);
  (void)engine->load({"* missing subckt", "X1 a b xinv", ".end"});
  const auto lines = recorder.output();
  CHECK(std::any_of(lines.begin(), lines.end(),
                    [](const std::string& l) { return l.find("unknown subckt") != std::string::npos; }));
  CHECK(std::none_of(lines.begin(), lines.end(), [](const std::string& l) { return l.starts_with("stderr "); }));
}

TEST_CASE("Stray .spiceinit ignored: the worker reads no .spiceinit from its directory",
          "[cap:simulation-engine-embedding][req:Deterministic engine initialisation]") {
  const TempDir job{"ng-spiceinit"};
  std::ofstream{job.path() / ".spiceinit"} << "echo spiceinit-was-read\nset ngbehavior=ps\n";
  hefesto::cli::PosixWorkerHost host;
  Recorder recorder;
  const auto engine = start(host, real(job), recorder);
  hefesto::test::run_job(*engine, recorder, rc_deck(1000));
  CHECK_FALSE(recorder.said("spiceinit-was-read"));
}

TEST_CASE("Reproducible random sources: two fresh workers give bit-identical vectors",
          "[cap:simulation-engine-embedding][req:Deterministic engine initialisation]") {
  const TempDir job{"ng-seed"};
  hefesto::cli::PosixWorkerHost host;
  std::vector<std::vector<sim::Vector>> runs;
  for (int i = 0; i < 2; ++i) {
    Recorder recorder;
    const auto engine = start(host, real(job), recorder);
    runs.push_back(hefesto::test::run_job(*engine, recorder, noise_deck(0)));
  }
  REQUIRE(runs[0].size() == runs[1].size());
  for (std::size_t v = 0; v < runs[0].size(); ++v) {
    CHECK(runs[0][v].name == runs[1][v].name);
    CHECK(runs[0][v].real == runs[1][v].real);
  }
  const sim::Vector* noise = find(runs[0], "n");
  REQUIRE(noise != nullptr);
  CHECK(std::any_of(noise->real.begin(), noise->real.end(), [](double x) { return x != 0.0; }));
}

TEST_CASE("50 ngspice jobs in one worker equal the same jobs each in a fresh worker",
          "[cap:simulation-engine-embedding][req:Single engine instance per address space]") {
  const TempDir job{"ng-reuse"};
  hefesto::cli::PosixWorkerHost host;
  constexpr int kJobs = 50;
  std::vector<std::vector<sim::Vector>> reused;
  {
    Recorder recorder;
    const auto engine = start(host, real(job), recorder);
    for (int i = 0; i < kJobs; ++i) {
      reused.push_back(hefesto::test::run_job(*engine, recorder, noise_deck(i)));
    }
  }
  for (int i = 0; i < kJobs; ++i) {
    Recorder recorder;
    const auto engine = start(host, real(job), recorder);
    const auto fresh = hefesto::test::run_job(*engine, recorder, noise_deck(i));
    const auto& before = reused[static_cast<std::size_t>(i)];
    REQUIRE(fresh.size() == before.size());
    for (std::size_t v = 0; v < fresh.size(); ++v) {
      CHECK(fresh[v].name == before[v].name);
      CHECK(fresh[v].real == before[v].real);  // bit for bit
    }
  }
}
