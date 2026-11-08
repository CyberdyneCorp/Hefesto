// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform/task_executor.hpp"

#include <algorithm>
#include <utility>

namespace hefesto::platform {

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t threads) {
  if (threads == 0) {
    threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
  }
  workers_.reserve(threads);
  for (std::size_t i = 0; i < threads; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
  {
    const std::scoped_lock lock{mutex_};
    stopping_ = true;
  }
  work_ready_.notify_all();
  for (auto& worker : workers_) {
    worker.join();
  }
}

void ThreadPoolExecutor::submit(Task task) {
  {
    const std::scoped_lock lock{mutex_};
    queue_.push_back(std::move(task));
  }
  work_ready_.notify_one();
}

void ThreadPoolExecutor::wait_idle() {
  std::unique_lock lock{mutex_};
  idle_.wait(lock, [this] { return queue_.empty() && running_ == 0; });
}

void ThreadPoolExecutor::worker_loop() {
  std::unique_lock lock{mutex_};
  for (;;) {
    work_ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
    if (queue_.empty()) {
      return;  // stopping and drained
    }
    Task task = std::move(queue_.front());
    queue_.pop_front();
    ++running_;
    lock.unlock();
    task();
    lock.lock();
    --running_;
    if (queue_.empty() && running_ == 0) {
      idle_.notify_all();
    }
  }
}

}  // namespace hefesto::platform
