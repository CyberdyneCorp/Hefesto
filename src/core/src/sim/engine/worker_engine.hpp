// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// WorkerEngine: sim::Engine over a hefesto-simd worker (openspec/changes/
// m9-lvs-simulation design D9, D10, D18; capability
// simulation-engine-embedding "Crash containment on desktop", "Worker channel
// limits"). Private to sim/engine: the public entry point is
// sim::make_worker_engine (engine.hpp), which uses the default limits.
//
// The engine talks to the worker over a platform::WorkerChannel with the
// frames of hefesto/sim/engine/frames.hpp. Requests (load, run, pause,
// resume, cancel) are sent one at a time and wait for the worker's reply.
// The channel's reader thread decodes every frame and calls the callbacks.
// A watchdog thread checks the heartbeat. Every fault (the worker exits or
// crashes, its engine faults, it sends a malformed, unknown or oversize frame,
// or misses its heartbeat) terminates the worker, reports
// EngineState::failed through on_state, and makes every later call fail with
// the fault's diagnostic ("simulation engine terminated: ..."); the owner
// replaces the engine (D-25 (a)).
//
// Between jobs the worker is reused: a load after a loaded job first sends
// `reset` (remcirc, destroy all), and every load sends `setseed 1` before
// the deck ("Deterministic engine initialisation").
//
// Owner: SIMD (the worker client of SIMENG's sim::Engine; task 5.4).

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/frames.hpp"

namespace hefesto::sim::worker {

/// The channel limits of "Worker channel limits"; each can be lowered,
/// never raised (a larger value is clamped to the default).
struct Limits {
  std::uint32_t max_frame_bytes = frames::kMaxFrameBytes;
  std::chrono::milliseconds handshake{10000};
  std::chrono::milliseconds heartbeat_timeout{10000};
  std::chrono::milliseconds watchdog_period{250};
  /// The result budget of one run (Q-SIM-25 default 4 GiB): the vectors a
  /// worker sends before VectorsDone; more is a fault (SIM-ENGINE-013).
  std::uint64_t max_result_bytes = std::uint64_t{4} << 30;
};

/// The limits with every value at most its default (and at least 1).
[[nodiscard]] Limits clamped(const Limits& limits);

/// make_worker_engine with explicit limits and extra worker arguments (the
/// tests pass hefesto-simd's --adapter and --heartbeat-ms).
[[nodiscard]] diag::Outcome<std::unique_ptr<Engine>> make(platform::WorkerHost* host,
                                                          const WorkerEngineOptions& options, const Limits& limits,
                                                          const std::vector<std::string>& args = {});

}  // namespace hefesto::sim::worker
