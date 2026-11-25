// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the hefesto-simd tests: a job directory, a WorkerEngine
// over the POSIX host, and a recorder of callbacks that can wait for a state.
// Owner: SIMD.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/sim/engine/engine.hpp"
#include "worker_host_posix.hpp"
// The private header of sim/engine (worker::make with limits and arguments).
#include "../../../src/core/src/sim/engine/worker_engine.hpp"

namespace hefesto::test {

using namespace std::chrono_literals;

/// A fresh directory under the temporary directory, removed at the end.
class TempDir {
 public:
  explicit TempDir(const std::string& name) {
    static std::atomic<int> counter{0};
    path_ = std::filesystem::temp_directory_path() /
            ("hefesto-" + name + "-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
    path_ = std::filesystem::canonical(path_);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  ~TempDir() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

/// Records callbacks; wait_for() blocks until a state was seen.
class Recorder {
 public:
  sim::EngineCallbacks callbacks() {
    sim::EngineCallbacks c;
    c.on_state = [this](sim::EngineState s) {
      std::lock_guard lock{mutex_};
      states_.push_back(s);
      changed_.notify_all();
    };
    c.on_output = [this](std::string_view line) {
      std::lock_guard lock{mutex_};
      output_.emplace_back(line);
    };
    return c;
  }
  bool wait_for(sim::EngineState state, std::chrono::milliseconds timeout = 30s) {
    std::unique_lock lock{mutex_};
    return changed_.wait_for(lock, timeout, [&] { return std::count(states_.begin(), states_.end(), state) > seen(state); });
  }
  /// Marks every state so far as seen, so the next wait_for needs a new one.
  void reset() {
    std::lock_guard lock{mutex_};
    baseline_ = states_;
  }
  std::vector<std::string> output() {
    std::lock_guard lock{mutex_};
    return output_;
  }
  std::vector<sim::EngineState> states() {
    std::lock_guard lock{mutex_};
    return states_;
  }
  bool said(std::string_view prefix) {
    const std::vector<std::string> lines = output();
    return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.starts_with(prefix); });
  }

 private:
  [[nodiscard]] long seen(sim::EngineState state) const {
    return std::count(baseline_.begin(), baseline_.end(), state);
  }
  std::mutex mutex_;
  std::condition_variable changed_;
  std::vector<sim::EngineState> states_;
  std::vector<sim::EngineState> baseline_;
  std::vector<std::string> output_;
};

struct WorkerOptions {
  std::string adapter;  // empty: the plugin next to hefesto-simd
  std::string job_dir;
  std::vector<std::string> readonly_roots;
  sim::worker::Limits limits;
  int heartbeat_ms = 0;  // 0: the default 2 s
};

inline diag::Outcome<std::unique_ptr<sim::Engine>> start_worker(cli::PosixWorkerHost& host,
                                                                 const WorkerOptions& options) {
  sim::WorkerEngineOptions engine_options;
  engine_options.program = HEFESTO_SIMD;
  engine_options.job_dir = options.job_dir;
  engine_options.readonly_roots = options.readonly_roots;
  std::vector<std::string> args;
  if (!options.adapter.empty()) {
    args.push_back("--adapter=" + options.adapter);
  }
  if (options.heartbeat_ms > 0) {
    args.push_back("--heartbeat-ms=" + std::to_string(options.heartbeat_ms));
  }
  return sim::worker::make(&host, engine_options, options.limits, args);
}

/// Loads, runs to the end and returns the vectors of one job.
inline std::vector<sim::Vector> run_job(sim::Engine& engine, Recorder& recorder, const std::vector<std::string>& deck) {
  recorder.reset();
  const diag::Status loaded = engine.load(deck);
  REQUIRE(loaded.ok());
  const diag::Status ran = engine.run();
  REQUIRE(ran.ok());
  REQUIRE(recorder.wait_for(sim::EngineState::loaded));
  auto vectors = engine.vectors();
  REQUIRE(vectors.ok());
  return std::move(vectors).value();
}

}  // namespace hefesto::test
