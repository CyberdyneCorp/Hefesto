// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// platform::run_on_stack (see hefesto/platform/stack_thread.hpp): POSIX
// threads with an explicit stack size; std::thread (its default stack)
// where pthreads are not available.
// Owner: INTEGRATION.

#include "hefesto/platform/stack_thread.hpp"

#include <climits>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#define HEFESTO_HAVE_PTHREAD 1
#endif

namespace hefesto::platform {

namespace {

struct Job {
  const std::function<void()>* work = nullptr;
  std::exception_ptr error;
};

void run_job(Job& job) noexcept {
  try {
    (*job.work)();
  } catch (...) {
    job.error = std::current_exception();
  }
}

#ifdef HEFESTO_HAVE_PTHREAD
void* stack_thread_entry(void* arg) {
  run_job(*static_cast<Job*>(arg));
  return nullptr;
}

// The stack size raised to the system minimum and rounded up to 64 KiB, a
// multiple of every page size (macOS requires whole pages).
std::size_t usable_stack(std::size_t bytes) {
  constexpr std::size_t kUnit = std::size_t{64} << 10U;
  const auto minimum = static_cast<std::size_t>(PTHREAD_STACK_MIN);
  std::size_t size = bytes < minimum ? minimum : bytes;
  if (size % kUnit != 0) {
    size += kUnit - size % kUnit;
  }
  return size;
}

bool run_pthread(std::size_t stack_bytes, Job& job) {
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    return false;
  }
  pthread_t thread{};
  const bool started = pthread_attr_setstacksize(&attr, usable_stack(stack_bytes)) == 0 &&
                       pthread_create(&thread, &attr, stack_thread_entry, &job) == 0;
  (void)pthread_attr_destroy(&attr);
  if (started) {
    (void)pthread_join(thread, nullptr);
  }
  return started;
}
#endif

}  // namespace

void run_on_stack(std::size_t stack_bytes, const std::function<void()>& work) {
  Job job;
  job.work = &work;
#ifdef HEFESTO_HAVE_PTHREAD
  if (!run_pthread(stack_bytes, job)) {
    throw std::runtime_error("cannot start a thread with a " + std::to_string(stack_bytes) + "-byte stack");
  }
#else
  (void)stack_bytes;
  std::thread worker([&job] { run_job(job); });
  worker.join();
#endif
  if (job.error) {
    std::rethrow_exception(job.error);
  }
}

}  // namespace hefesto::platform
