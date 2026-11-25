// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::Engine: the simulation engine interface (openspec/changes/
// m9-lvs-simulation design.md D2, D9, "Area contracts" SIMENG; capability
// simulation-engine-embedding "Engine adapter boundary", "Callback
// contract", "Crash containment on desktop").
//
// The core never links ngspice. On desktop the engine runs in the
// `hefesto-simd` worker, which loads the `hefesto-ngspice-adapter` plugin
// (Q-SIM-20, Q-SIM-26); WorkerEngine talks to it over platform::WorkerHost
// (platform/worker_host.hpp, PLATFORM) with the frames of
// sim/engine/frames.hpp (SIMD). The in-process engine exists only in
// developer builds and loads the same plugin behind this interface
// (Q-SIM-22). scripts/check_engine_boundary.py enforces both rules.
//
// Owner: SIMENG.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::platform {
class WorkerHost;  // platform/worker_host.hpp (PLATFORM, task 5.3)
}

namespace hefesto::sim {

enum class EngineState : std::uint8_t { idle, loaded, running, paused, failed };

/// One result vector, copied into Hefesto-owned storage.
struct Vector {
  std::string name;
  std::string type;  // ngspice's vector type name, e.g. "voltage", "time"
  bool complex = false;
  std::vector<double> real;
  std::vector<double> imag;  // empty unless `complex`
};

/// Called on the engine's reader thread; each callback copies what it keeps
/// and never blocks (Q-SIM-15).
struct EngineCallbacks {
  std::function<void(std::string_view line)> on_output;
  /// Error-stream lines (ngspice's `stderr ` prefix already removed). When
  /// unset, they go to on_output like any other job-log line.
  std::function<void(std::string_view line)> on_error;
  std::function<void(std::string_view status, double point)> on_progress;
  std::function<void(EngineState state)> on_state;
};

class Engine {
 public:
  virtual ~Engine() = default;

  virtual void set_callbacks(EngineCallbacks callbacks) = 0;
  [[nodiscard]] virtual diag::Status load(const std::vector<std::string>& deck) = 0;
  [[nodiscard]] virtual diag::Status run() = 0;
  [[nodiscard]] virtual diag::Status pause() = 0;
  [[nodiscard]] virtual diag::Status resume() = 0;
  [[nodiscard]] virtual diag::Status cancel() = 0;
  /// The final vectors of the current plot.
  [[nodiscard]] virtual diag::Outcome<std::vector<Vector>> vectors() const = 0;
};

/// How to start the worker (passed to WorkerHost as a WorkerSpec).
struct WorkerEngineOptions {
  std::string program = "hefesto-simd";
  std::string job_dir;                      // the only writable directory (design D9)
  std::vector<std::string> readonly_roots;  // the PDK store and the installed libraries
};

/// A WorkerEngine over `host` (not owned; it outlives the engine); fails
/// when `host` is nullptr or cannot start a worker.
[[nodiscard]] diag::Outcome<std::unique_ptr<Engine>> make_worker_engine(platform::WorkerHost* host,
                                                                        const WorkerEngineOptions& options);

}  // namespace hefesto::sim
