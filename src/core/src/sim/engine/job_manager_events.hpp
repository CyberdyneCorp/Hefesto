// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The engine events of sim::JobManager (task 5.5): engine callbacks run on
// the engine's reader thread, copy their data into an EngineEvent and post
// it here without blocking on the manager (Q-SIM-15); JobManager::poll()
// takes the batch on the owning thread.
//
// Owner: SIMENG.

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/sim/engine/engine.hpp"

namespace hefesto::sim {

struct EngineEvent {
  enum class Kind : std::uint8_t { output, state };
  std::size_t slot = 0;
  std::uint64_t generation = 0;  // the engine that posted it
  Kind kind = Kind::output;
  std::string line;              // output
  EngineState state = EngineState::idle;  // state
};

class EventQueue {
 public:
  void post(EngineEvent event) {
    const std::lock_guard lock(mutex_);
    events_.push_back(std::move(event));
  }

  [[nodiscard]] std::vector<EngineEvent> take() {
    std::vector<EngineEvent> out;
    const std::lock_guard lock(mutex_);
    out.swap(events_);
    return out;
  }

 private:
  std::mutex mutex_;
  std::vector<EngineEvent> events_;
};

}  // namespace hefesto::sim
