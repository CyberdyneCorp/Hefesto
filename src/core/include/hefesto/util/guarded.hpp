// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Single-writer / multi-reader guard (capability hefesto-architecture,
// requirement "Concurrency model"): one writer at a time (the command thread)
// and any number of concurrent readers (rendering, background DRC/extraction).
// Readers get const access only, so they cannot mutate shared state.

#pragma once

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace hefesto::util {

template <typename T>
class Guarded {
 public:
  template <typename... Args>
  explicit Guarded(Args&&... args) : value_(std::forward<Args>(args)...) {}

  Guarded(const Guarded&) = delete;
  Guarded& operator=(const Guarded&) = delete;

  /// Runs `fn(const T&)` under a shared lock; readers run concurrently.
  template <typename F>
  decltype(auto) read(F&& fn) const {
    const std::shared_lock lock{mutex_};
    return std::forward<F>(fn)(static_cast<const T&>(value_));
  }

  /// Runs `fn(T&)` under the exclusive lock; waits for readers to finish.
  template <typename F>
  decltype(auto) write(F&& fn) {
    const std::unique_lock lock{mutex_};
    return std::forward<F>(fn)(value_);
  }

 private:
  mutable std::shared_mutex mutex_;
  T value_;
};

}  // namespace hefesto::util
