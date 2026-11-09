// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's crash-backup clock (cell-io-safety "Crash backup timer"):
// the cell::BackupTimerHost the core arms with `crashbackups`. It keeps a
// deadline that the app's frame loop polls on the UI thread (the command
// thread), so `crash save` never runs concurrently with another command.
// Toolkit-free and unit-tested.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>

#include "hefesto/cell/io_safety.hpp"

namespace hefesto::shell {

class DeadlineTimer final : public cell::BackupTimerHost {
 public:
  using Clock = std::chrono::steady_clock;
  using Now = std::function<Clock::time_point()>;

  /// `now` reads the clock (tests pass a fake one).
  explicit DeadlineTimer(Now now = &Clock::now) : now_(std::move(now)) {}

  void schedule(std::int64_t delay_ms) override { deadline_ = now_() + std::chrono::milliseconds(delay_ms); }
  void cancel() override { deadline_.reset(); }

  [[nodiscard]] bool armed() const noexcept { return deadline_.has_value(); }
  /// True once when the deadline has passed (the timer is then disarmed,
  /// as a one-shot timer is after firing).
  bool take_due() {
    if (!deadline_ || now_() < *deadline_) return false;
    deadline_.reset();
    return true;
  }
  /// Seconds until the deadline (0 when due); nullopt when not armed. The
  /// frame loop waits at most this long for events.
  [[nodiscard]] std::optional<double> seconds_left() const {
    if (!deadline_) return std::nullopt;
    const std::chrono::duration<double> left = *deadline_ - now_();
    return left.count() > 0.0 ? left.count() : 0.0;
  }

 private:
  Now now_;
  std::optional<Clock::time_point> deadline_;
};

}  // namespace hefesto::shell
