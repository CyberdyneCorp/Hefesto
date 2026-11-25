// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The worker pool size (simulation-engine-embedding "Multiple simulations
// and corner parallelism"; Q-SIMADV-13; task 5.5): the formula alone.
// job_manager_test.cpp checks that the manager runs at most N jobs.
//
// Owner: SIMENG.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "hefesto/sim/engine/pool.hpp"

namespace sim = hefesto::sim;

namespace {

constexpr std::uint64_t kGiB = std::uint64_t{1} << 30;
constexpr std::uint64_t kMiB = std::uint64_t{1} << 20;
constexpr std::uint64_t kResultBudget = 4 * kGiB;  // the desktop default (Q-SIM-16)

std::uint32_t pool(const sim::PoolInputs& inputs) {
  const auto n = sim::worker_pool_size(inputs);
  REQUIRE(n.ok());
  return n.value();
}

}  // namespace

TEST_CASE("Five corners on an 8-core Mac: three workers",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  // 32 GB, the default 4 GiB result budget and a small deck: 16 GiB / B_job rounds down to 3.
  CHECK(pool({8, 32 * kGiB, kResultBudget + kMiB, std::nullopt}) == 3);
  CHECK(pool({8, 32'000'000'000, kResultBudget + kMiB, std::nullopt}) == 3);
  CHECK(sim::max_worker_pool_size({8, 32 * kGiB, kResultBudget + kMiB, std::nullopt}) == 3);
}

TEST_CASE("Single-core host: one worker",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  CHECK(pool({1, 64 * kGiB, kResultBudget, std::nullopt}) == 1);
  // An unknown core count counts as one core.
  CHECK(pool({0, 64 * kGiB, kResultBudget, std::nullopt}) == 1);
}

TEST_CASE("Large post-layout deck limits the pool to one worker",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  // 10 cores, 16 GiB: 8 GiB / (4 GiB + 300 MiB) rounds down to 1.
  CHECK(pool({10, 16 * kGiB, kResultBudget + 300 * kMiB, std::nullopt}) == 1);
  // No memory at all still gives one worker.
  CHECK(pool({10, 0, kResultBudget, std::nullopt}) == 1);
}

TEST_CASE("Job with no memory estimate drops the memory term",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  CHECK(pool({8, 32 * kGiB, 0, std::nullopt}) == 4);
  CHECK(pool({64, 0, 0, std::nullopt}) == 4);  // never more than 4
}

TEST_CASE("Worker setting of zero counts as one",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  CHECK(pool({8, 32 * kGiB, kResultBudget + kMiB, 0U}) == 1);
}

TEST_CASE("A worker setting up to N lowers the pool",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  CHECK(pool({8, 32 * kGiB, kResultBudget + kMiB, 2U}) == 2);
  CHECK(pool({8, 32 * kGiB, kResultBudget + kMiB, 3U}) == 3);
}

TEST_CASE("Setting cannot raise the pool: a larger setting is refused with the maximum",
          "[cap:simulation-engine-embedding][req:Multiple simulations and corner parallelism]") {
  // Regression (round-0 follow-up): the setting was documented as "a larger
  // value has no effect", so a setting of 8 was silently ignored; the
  // scenario requires a refusal that gives the maximum 3.
  const sim::PoolInputs inputs{8, 32 * kGiB, kResultBudget + kMiB, 8U};
  const auto n = sim::worker_pool_size(inputs);
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "SIM-ENGINE-033");
  CHECK(n.error().text.find("maximum 3") != std::string::npos);
  CHECK(sim::max_worker_pool_size(inputs) == 3);
}
