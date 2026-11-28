// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A deterministic fake Engine for the run-set tests (M9 R3, tasks
// 15.1-15.3): its vectors depend only on the deck it loaded and the seed of
// the load, never on the jobs the engine ran before, like a worker that
// clears the circuit and repeats `setseed` between jobs (design D9). A job
// whose title holds an id of `crash_jobs` faults its engine; one in
// `slow_jobs` waits a few milliseconds before it ends.
//
// Owner: SIMADV.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "hefesto/sim/engine/job_manager.hpp"
#include "hefesto/sim/runset/runset.hpp"

namespace hefesto::test::simrunset {

namespace diag = hefesto::diag;
namespace sim = hefesto::sim;

inline constexpr std::uint64_t kGiB = std::uint64_t{1} << 30;

struct Host {
  std::mutex mutex;
  std::set<std::string> crash_jobs;
  std::set<std::string> slow_jobs;
  std::uint64_t engines = 0;
};

inline std::uint64_t fnv(std::uint64_t h, const std::string& text) {
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    h = (h ^ c) * 1099511628211ULL;
  }
  return h;
}

class Engine final : public sim::Engine {
 public:
  explicit Engine(Host& host) : host_(host) {}
  void set_callbacks(sim::EngineCallbacks callbacks) override { callbacks_ = std::move(callbacks); }
  diag::Status load(const std::vector<std::string>& deck, const sim::LoadOptions& options) override {
    hash_ = 1469598103934665603ULL ^ options.seed;  // the circuit and the seed of this load only
    for (const std::string& line : deck) {
      hash_ = fnv(hash_, line);
    }
    title_ = deck.empty() ? std::string{} : deck.front();
    return diag::Done{};
  }
  diag::Status run() override {
    callbacks_.on_state(sim::EngineState::running);
    if (matches(host_.slow_jobs)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    callbacks_.on_state(matches(host_.crash_jobs) ? sim::EngineState::failed : sim::EngineState::loaded);
    return diag::Done{};
  }
  diag::Status pause() override { return diag::make("SIM-ENGINE-010", "no analysis is running"); }
  diag::Status resume() override { return diag::make("SIM-ENGINE-011", "engine busy"); }
  diag::Status cancel() override { return diag::Done{}; }
  diag::Outcome<std::vector<sim::Vector>> vectors() const override {
    if (matches(host_.crash_jobs)) {
      return diag::make("SIM-ENGINE-008", "simulation engine terminated: the worker was killed by signal 11");
    }
    std::vector<double> out(4);
    for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = static_cast<double>((hash_ >> (8 * i)) & 0xFFFFU) / 65536.0;
    }
    return std::vector<sim::Vector>{sim::Vector{"time", "time", false, {0, 1e-9, 2e-9, 3e-9}, {}},
                                    sim::Vector{"v(out)", "voltage", false, out, {}}};
  }

 private:
  [[nodiscard]] bool matches(const std::set<std::string>& ids) const {
    const std::lock_guard lock{host_.mutex};
    for (const std::string& id : ids) {
      if (title_.find("[" + id + "]") != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  Host& host_;
  sim::EngineCallbacks callbacks_;
  std::uint64_t hash_ = 0;
  std::string title_;
};

/// A job manager of `workers` workers (32 GiB, small jobs).
inline sim::JobManager manager(Host& host, std::uint32_t workers) {
  sim::JobManagerOptions o;
  o.make_engine = [&host]() -> diag::Outcome<std::unique_ptr<sim::Engine>> {
    {
      const std::lock_guard lock{host.mutex};
      ++host.engines;
    }
    return std::unique_ptr<sim::Engine>(std::make_unique<Engine>(host));
  };
  o.pool = sim::PoolInputs{64, 64 * kGiB, kGiB, workers};
  o.now = [] { return std::chrono::system_clock::time_point{} + std::chrono::hours(24 * 365 * 50); };
  return sim::JobManager(std::move(o));
}

/// Runs `runner` to its end.
inline void run_to_end(sim::JobManager& jobs, sim::RunSetRunner& runner) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  while (std::chrono::steady_clock::now() < deadline) {
    jobs.poll_for(std::chrono::milliseconds(2));
    runner.update();
    if (runner.state() != sim::RunSetState::running && runner.state() != sim::RunSetState::paused) {
      return;
    }
    if (runner.state() == sim::RunSetState::paused && jobs.active() == 0 && jobs.loading() == 0) {
      return;
    }
  }
  FAIL("the run set did not end");
}

/// The determinism test's second worker count: min(4, physical cores - 1),
/// at least 2 (tasks.md 15.1).
inline std::uint32_t parallel_workers() {
  const unsigned cores = std::max(2U, std::thread::hardware_concurrency());
  return std::max(2U, std::min(4U, cores - 1));
}

/// A raw base deck with a top-level parameter, a temperature and a supply.
inline sim::PreparedDeck base_deck() {
  sim::PreparedDeck d;
  d.lines = {"* inverter tb",  ".param wn=1 ln=0.15", ".temp 27",       "Vdd vdd 0 DC 1.8",
             "Vin in 0 0.9",   ".subckt inv a y vdd", ".param wn=5",    "R1 a y {wn}",
             ".ends",          "Xdut in out vdd inv", ".tran 1n 10n",   ".end"};
  d.sha256 = "base-sha";
  return d;
}

/// The vectors of every point, in point order.
inline std::vector<std::vector<sim::Vector>> vectors_by_point(const sim::RunSet& rs, const sim::JobManager& jobs) {
  std::vector<std::vector<sim::Vector>> out;
  for (const sim::PointRecord& r : rs.records) {
    const sim::JobRecord* rec = jobs.record(r.job);
    out.push_back(rec != nullptr ? rec->vectors : std::vector<sim::Vector>{});
  }
  return out;
}

inline bool same_vectors(const std::vector<std::vector<sim::Vector>>& a, const std::vector<std::vector<sim::Vector>>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].size() != b[i].size()) {
      return false;
    }
    for (std::size_t k = 0; k < a[i].size(); ++k) {
      if (a[i][k].name != b[i][k].name || a[i][k].real != b[i][k].real) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace hefesto::test::simrunset
