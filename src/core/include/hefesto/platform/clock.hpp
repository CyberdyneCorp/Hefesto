// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Clock service (capability hefesto-architecture, "Platform services abstraction").

#pragma once

#include <atomic>
#include <chrono>
#include <string>

namespace hefesto::platform {

class Clock {
 public:
  Clock() = default;
  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;
  virtual ~Clock() = default;

  [[nodiscard]] virtual std::chrono::system_clock::time_point wall_now() = 0;
  [[nodiscard]] virtual std::chrono::steady_clock::time_point monotonic_now() = 0;
};

class SystemClock final : public Clock {
 public:
  std::chrono::system_clock::time_point wall_now() override { return std::chrono::system_clock::now(); }
  std::chrono::steady_clock::time_point monotonic_now() override { return std::chrono::steady_clock::now(); }
};

/// A clock that only moves when told to, for deterministic tests and outputs.
class FixedClock final : public Clock {
 public:
  explicit FixedClock(std::chrono::system_clock::time_point wall = {}) : wall_(wall.time_since_epoch().count()) {}

  std::chrono::system_clock::time_point wall_now() override {
    return std::chrono::system_clock::time_point{std::chrono::system_clock::duration{wall_.load()}};
  }
  std::chrono::steady_clock::time_point monotonic_now() override {
    return std::chrono::steady_clock::time_point{std::chrono::steady_clock::duration{mono_.load()}};
  }
  void advance(std::chrono::nanoseconds step) {
    wall_ += std::chrono::duration_cast<std::chrono::system_clock::duration>(step).count();
    mono_ += std::chrono::duration_cast<std::chrono::steady_clock::duration>(step).count();
  }

 private:
  std::atomic<std::chrono::system_clock::rep> wall_;
  std::atomic<std::chrono::steady_clock::rep> mono_{0};
};

/// UTC timestamp "YYYY-MM-DDTHH:MM:SS.mmmZ", independent of the host time zone.
[[nodiscard]] std::string format_utc_timestamp(std::chrono::system_clock::time_point time);

}  // namespace hefesto::platform
