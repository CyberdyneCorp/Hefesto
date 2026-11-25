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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/sim/engine/engine.hpp"

namespace hefesto::sim {

struct EngineEvent {
  enum class Kind : std::uint8_t { output, error, state, dropped };
  std::size_t slot = 0;
  std::uint64_t generation = 0;  // the job's engine connection that posted it
  Kind kind = Kind::output;
  std::string line;              // output, error; dropped: the first dropped `Error` line, or empty
  EngineState state = EngineState::idle;  // state
  std::uint64_t dropped = 0;     // dropped: how many lines were not queued
};

/// Line events beyond kMaxQueuedLines between two take() calls are not
/// queued: they are counted per connection and posted as one `dropped`
/// event before the connection's next state event (or at take()), so the
/// queue stays bounded and a dropped `Error` line still fails the job.
class EventQueue {
 public:
  static constexpr std::size_t kMaxQueuedLines = std::size_t{1} << 16;

  void post(EngineEvent event) {
    const std::lock_guard lock(mutex_);
    if (event.kind == EngineEvent::Kind::state) {
      flush_dropped(event.slot, event.generation);
      events_.push_back(std::move(event));
      return;
    }
    if (lines_ < kMaxQueuedLines) {
      ++lines_;
      events_.push_back(std::move(event));
      return;
    }
    count_dropped(event);
  }

  [[nodiscard]] std::vector<EngineEvent> take() {
    std::vector<EngineEvent> out;
    const std::lock_guard lock(mutex_);
    while (!dropped_.empty()) {
      flush_dropped(dropped_.front().slot, dropped_.front().generation);
    }
    out.swap(events_);
    lines_ = 0;
    return out;
  }

 private:
  void count_dropped(const EngineEvent& event) {
    auto it = std::find_if(dropped_.begin(), dropped_.end(), [&](const EngineEvent& d) {
      return d.slot == event.slot && d.generation == event.generation;
    });
    if (it == dropped_.end()) {
      it = dropped_.insert(dropped_.end(), EngineEvent{event.slot, event.generation, EngineEvent::Kind::dropped, {},
                                                       EngineState::idle, 0});
    }
    ++it->dropped;
    if (event.kind == EngineEvent::Kind::error && it->line.empty() && event.line.starts_with("Error")) {
      it->line = event.line;
    }
  }

  void flush_dropped(std::size_t slot, std::uint64_t generation) {
    const auto it = std::find_if(dropped_.begin(), dropped_.end(), [&](const EngineEvent& d) {
      return d.slot == slot && d.generation == generation;
    });
    if (it != dropped_.end()) {
      events_.push_back(std::move(*it));
      dropped_.erase(it);
    }
  }

  std::mutex mutex_;
  std::vector<EngineEvent> events_;
  std::size_t lines_ = 0;               // line events in events_
  std::vector<EngineEvent> dropped_;    // one `dropped` event per connection with dropped lines
};

}  // namespace hefesto::sim
