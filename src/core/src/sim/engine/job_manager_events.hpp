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
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/job_manager.hpp"

namespace hefesto::sim {

struct EngineEvent {
  /// started: the loader thread of the slot ended (M9 R2); progress: a
  /// status string (`line`) and its percentage (`point`, or -1); scale: the
  /// streamed `time` value (`point`).
  enum class Kind : std::uint8_t { output, error, state, dropped, started, progress, scale };
  std::size_t slot = 0;
  std::uint64_t generation = 0;  // the job's engine connection that posted it
  Kind kind = Kind::output;
  std::string line;              // output, error, progress; dropped: the first dropped `Error` line, or empty
  EngineState state = EngineState::idle;  // state
  std::uint64_t dropped = 0;     // dropped: how many lines were not queued
  double point = -1.0;           // progress, scale
};

/// The queue is bounded by count in every kind a worker can send:
///   - line events beyond kMaxQueuedLines between two take() calls are not
///     queued: they are counted per connection and posted as one `dropped`
///     event before the connection's next queued state event (or at
///     take()), so a dropped `Error` line still fails the job;
///   - state events beyond kMaxQueuedStateEvents (M9 R2) are coalesced per
///     connection: the connection's later states collapse into `running`
///     (when one of them was `running`) followed by the last state, or
///     `failed` when any was `failed`, posted at take(). A worker that
///     floods state frames therefore cannot grow the queue, and the
///     outcome of the job is the one the full sequence gives (the run's
///     end and a fault are never lost);
///   - progress and scale events are superseded: only the latest of each
///     connection is kept and posted at take();
///   - `started` comes from the manager's own loader threads, one per job.
class EventQueue {
 public:
  static constexpr std::size_t kMaxQueuedLines = std::size_t{1} << 16;
  static constexpr std::size_t kMaxQueuedStates = kMaxQueuedStateEvents;

  void post(EngineEvent event) {
    {
      const std::lock_guard lock(mutex_);
      route(std::move(event));
      peak_ = std::max(peak_, events_.size());
    }
    changed_.notify_all();
  }

  [[nodiscard]] std::vector<EngineEvent> take() {
    std::vector<EngineEvent> out;
    const std::lock_guard lock(mutex_);
    for (Pending& p : pending_) {
      flush(p);
    }
    pending_.clear();
    out.swap(events_);
    lines_ = 0;
    states_ = 0;
    return out;
  }

  /// Waits until an event is queued or `timeout` passes.
  void wait(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    changed_.wait_for(lock, timeout, [this] { return !events_.empty() || !pending_.empty(); });
  }

  /// The largest number of events queued at once so far.
  [[nodiscard]] std::size_t peak() {
    const std::lock_guard lock(mutex_);
    return peak_;
  }

 private:
  // What one connection has pending outside events_.
  struct Pending {
    std::size_t slot = 0;
    std::uint64_t generation = 0;
    std::uint64_t dropped = 0;
    std::string first_error;             // the first dropped `Error` line
    bool coalescing = false;             // later states of this connection are coalesced
    bool saw_running = false;
    bool failed = false;
    std::optional<EngineState> last;     // the last coalesced state
    std::optional<EngineEvent> progress;  // the latest progress event
    std::optional<EngineEvent> scale;     // the latest scale event
  };

  void route(EngineEvent event) {
    switch (event.kind) {
      case EngineEvent::Kind::state: post_state(std::move(event)); break;
      case EngineEvent::Kind::progress: pending(event).progress = std::move(event); break;
      case EngineEvent::Kind::scale: pending(event).scale = std::move(event); break;
      case EngineEvent::Kind::started: events_.push_back(std::move(event)); break;
      default: post_line(std::move(event)); break;
    }
  }

  void post_state(EngineEvent event) {
    Pending* p = find(event.slot, event.generation);
    if (states_ < kMaxQueuedStates && (p == nullptr || !p->coalescing)) {
      if (p != nullptr) {
        flush_dropped(*p);
      }
      ++states_;
      events_.push_back(std::move(event));
      return;
    }
    Pending& c = pending(event);
    c.coalescing = true;
    c.saw_running = c.saw_running || event.state == EngineState::running;
    c.failed = c.failed || event.state == EngineState::failed;
    c.last = event.state;
  }

  void post_line(EngineEvent event) {
    if (lines_ < kMaxQueuedLines) {
      ++lines_;
      events_.push_back(std::move(event));
      return;
    }
    Pending& p = pending(event);
    ++p.dropped;
    if (event.kind == EngineEvent::Kind::error && p.first_error.empty() && event.line.starts_with("Error")) {
      p.first_error = std::move(event.line);
    }
  }

  Pending* find(std::size_t slot, std::uint64_t generation) {
    const auto it = std::find_if(pending_.begin(), pending_.end(), [&](const Pending& p) {
      return p.slot == slot && p.generation == generation;
    });
    return it == pending_.end() ? nullptr : &*it;
  }

  Pending& pending(const EngineEvent& event) {
    if (Pending* p = find(event.slot, event.generation)) {
      return *p;
    }
    pending_.push_back(Pending{});
    pending_.back().slot = event.slot;
    pending_.back().generation = event.generation;
    return pending_.back();
  }

  void flush_dropped(Pending& p) {
    if (p.dropped == 0) {
      return;
    }
    events_.push_back(EngineEvent{p.slot, p.generation, EngineEvent::Kind::dropped, std::move(p.first_error),
                                  EngineState::idle, p.dropped});
    p.dropped = 0;
    p.first_error.clear();
  }

  void push_state(const Pending& p, EngineState state) {
    events_.push_back(EngineEvent{p.slot, p.generation, EngineEvent::Kind::state, {}, state});
  }

  void flush(Pending& p) {
    flush_dropped(p);
    if (p.progress) {
      events_.push_back(std::move(*p.progress));
    }
    if (p.scale) {
      events_.push_back(std::move(*p.scale));
    }
    if (!p.coalescing) {
      return;
    }
    if (p.saw_running) {
      push_state(p, EngineState::running);
    }
    if (p.failed) {
      push_state(p, EngineState::failed);
    } else if (p.last && *p.last != EngineState::running) {
      push_state(p, *p.last);
    }
  }

  std::mutex mutex_;
  std::condition_variable changed_;
  std::vector<EngineEvent> events_;
  std::size_t lines_ = 0;          // line events in events_
  std::size_t states_ = 0;         // state events in events_
  std::size_t peak_ = 0;
  std::vector<Pending> pending_;   // per connection: dropped lines, coalesced states, latest progress
};

}  // namespace hefesto::sim
