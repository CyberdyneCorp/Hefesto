// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The in-process developer host (HEFESTO_DEVELOPER_BUILD=ON and
// HEFESTO_BUILD_NGSPICE=ON only; Q-SIM-22 (a)): it loads the same
// hefesto-ngspice-adapter plugin as hefesto-simd, behind sim::Engine, and
// gives the same vectors as the worker. Owner: SIMD.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "devhost/in_process_engine.hpp"
#include "worker_fixture.hpp"

using hefesto::test::Recorder;
using hefesto::test::TempDir;
namespace sim = hefesto::sim;

TEST_CASE("the developer host runs the plugin in-process with the worker's results",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  const std::vector<std::string> deck = {"* rc", "V1 in 0 PULSE(0 1 0 1n 1n 1u 2u)", "R1 in out 1k", "C1 out 0 1n",
                                         ".tran 10n 2u", ".end"};
  auto in_process = hefesto::simd::make_in_process_engine(HEFESTO_NGSPICE_ADAPTER);
  REQUIRE(in_process.ok());
  Recorder local;
  in_process.value()->set_callbacks(local.callbacks());
  const std::vector<sim::Vector> mine = hefesto::test::run_job(*in_process.value(), local, deck);
  // One engine per process: a second in-process engine is refused.
  CHECK(hefesto::simd::make_in_process_engine(HEFESTO_NGSPICE_ADAPTER).error().code == "SIM-ENGINE-001");

  const TempDir job{"devhost"};
  hefesto::cli::PosixWorkerHost host;
  hefesto::test::WorkerOptions options;
  options.job_dir = job.path().string();
  auto worker = hefesto::test::start_worker(host, options);
  REQUIRE(worker.ok());
  Recorder remote;
  worker.value()->set_callbacks(remote.callbacks());
  const std::vector<sim::Vector> theirs = hefesto::test::run_job(*worker.value(), remote, deck);
  REQUIRE(mine.size() == theirs.size());
  for (std::size_t v = 0; v < mine.size(); ++v) {
    CHECK(mine[v].name == theirs[v].name);
    CHECK(mine[v].real == theirs[v].real);
  }
}
