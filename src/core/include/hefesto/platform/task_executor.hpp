// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Background task execution (capability hefesto-architecture, requirements
// "Platform services abstraction" and "Concurrency model").

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace hefesto::platform {

using Task = std::function<void()>;

class TaskExecutor {
 public:
  TaskExecutor() = default;
  TaskExecutor(const TaskExecutor&) = delete;
  TaskExecutor& operator=(const TaskExecutor&) = delete;
  virtual ~TaskExecutor() = default;

  /// Queues work off the command thread. Tasks must not throw.
  virtual void submit(Task task) = 0;
  /// Blocks until every task submitted so far has finished.
  virtual void wait_idle() = 0;
};

/// Runs each task immediately on the caller's thread: deterministic tests.
class InlineExecutor final : public TaskExecutor {
 public:
  void submit(Task task) override { task(); }
  void wait_idle() override {}
};

/// A fixed pool of std::threads. The destructor drains the queue and joins.
class ThreadPoolExecutor final : public TaskExecutor {
 public:
  /// `threads` == 0 means std::thread::hardware_concurrency() (at least 1).
  explicit ThreadPoolExecutor(std::size_t threads = 0);
  ~ThreadPoolExecutor() override;

  void submit(Task task) override;
  void wait_idle() override;

 private:
  void worker_loop();

  std::mutex mutex_;
  std::condition_variable work_ready_;
  std::condition_variable idle_;
  std::deque<Task> queue_;
  std::size_t running_ = 0;
  bool stopping_ = false;
  std::vector<std::thread> workers_;
};

}  // namespace hefesto::platform
