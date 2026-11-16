// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop host of long GDS and CIF reads and writes (see mask_job_host.hpp).

#include "hefesto/shell/mask_job_host.hpp"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

#include "hefesto/platform/stack_thread.hpp"
#include "hefesto/shell/drc_link.hpp"

namespace hefesto::shell {

namespace {

using Clock = std::chrono::steady_clock;

std::string file_name(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return std::string{slash == std::string_view::npos ? path : path.substr(slash + 1)};
}

/// The end of a worker: set once on the worker, waited for on the caller.
class WorkerDone {
 public:
  void set() {
    {
      const std::lock_guard lock(mutex_);
      done_ = true;
    }
    cv_.notify_one();
  }
  /// Waits up to `interval`; true once set.
  bool wait_for(std::chrono::milliseconds interval) {
    std::unique_lock lock(mutex_);
    return cv_.wait_for(lock, interval, [this] { return done_; });
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool done_ = false;
};

}  // namespace

std::string completed_text(double fraction) {
  const double clamped = std::clamp(fraction, 0.0, 1.0);
  return "Completed " + std::to_string(static_cast<int>(std::floor(clamped * 100.0))) + "%";
}

std::optional<InterruptCardView> mask_job_card(const MaskJobState& state) {
  if (state.elapsed < kLongRunning) return std::nullopt;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(state.elapsed).count();
  InterruptCardView card;
  card.title = std::string{state.kind == MaskJobKind::read ? "Reading " : "Writing "} + file_name(state.label) +
               " · " + std::to_string(seconds) + " s";
  if (state.cancel_requested) {
    card.detail = state.kind == MaskJobKind::write ? "Stopping: no file is written" : "Stopping";
  } else if (state.fraction) {
    card.detail = completed_text(*state.fraction);
  }
  return card;
}

/// The state the worker, the calling thread and state() share.
struct MaskJobHost::Shared {
  mutable std::mutex mutex;
  MaskJobKind kind = MaskJobKind::write;
  std::string label;
  Clock::time_point started{};
  std::thread::id owner;  // the job's calling thread, which pumps
  std::atomic<bool> cancel{false};
  std::atomic<bool> reported{false};
  std::atomic<double> fraction{0.0};
  mask::Progress progress;
};

/// The outermost run_* call of a job: publishes and clears its state.
class MaskJobHost::Job {
 public:
  Job(MaskJobHost& host, MaskJobKind kind, std::string_view label) : host_(host) {
    Shared& s = *host_.shared_;
    {
      const std::lock_guard lock(s.mutex);
      s.kind = kind;
      s.label = std::string{label};
      s.started = Clock::now();
      s.owner = std::this_thread::get_id();
    }
    s.cancel.store(false, std::memory_order_relaxed);
    s.reported.store(false, std::memory_order_relaxed);
    host_.last_pump_ = Clock::now();
    host_.depth_.fetch_add(1, std::memory_order_acq_rel);
  }
  ~Job() { host_.depth_.fetch_sub(1, std::memory_order_acq_rel); }
  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

 private:
  MaskJobHost& host_;
};

MaskJobHost::MaskJobHost(db::DatabaseLock& lock) : lock_(lock), shared_(std::make_shared<Shared>()) {
  shared_->progress.cancel = &shared_->cancel;
  // Called by the reader on the calling thread, or by the generators on the
  // worker: the fraction is published; only the calling thread pumps.
  shared_->progress.report = [this](double fraction) {
    shared_->fraction.store(fraction, std::memory_order_relaxed);
    shared_->reported.store(true, std::memory_order_release);
    pump_if_due();
  };
  shared_->progress.tick = [this] { pump_if_due(); };
}

MaskJobHost::~MaskJobHost() = default;

void MaskJobHost::set_pump(std::function<void()> pump, std::chrono::milliseconds interval) {
  pump_ = std::move(pump);
  interval_ = interval;
}

void MaskJobHost::pump_if_due() {
  if (!pump_ || !busy()) return;
  {
    const std::lock_guard lock(shared_->mutex);
    if (shared_->owner != std::this_thread::get_id()) return;
  }
  const auto now = Clock::now();
  if (now - last_pump_ < interval_) return;
  last_pump_ = now;
  pump_();
}

cmd::CommandResult MaskJobHost::run_job(MaskJobKind kind, std::string_view label, const MaskWork& work) {
  if (busy()) return work(&shared_->progress);
  cmd::CommandResult result;
  {
    const Job job(*this, kind, label);
    result = work(&shared_->progress);
  }
  job_ended();
  return result;
}

cmd::CommandResult MaskJobHost::run_emit(std::string_view label, const MaskWork& emit) {
  const bool outermost = !busy();  // false inside run_job, which owns the job
  std::optional<Job> job;
  if (outermost) job.emplace(*this, MaskJobKind::write, label);
  // The calling thread's exclusive hold covers the worker (it only pumps).
  const bool covered = lock_.held_exclusively_by_this_thread();
  cmd::CommandResult result;
  std::exception_ptr error;
  WorkerDone done;
  std::thread worker([&] {
    try {
      std::optional<db::DatabaseLock::Shared> reading;
      if (!covered) reading.emplace(lock_);
      platform::run_on_stack(platform::kDeepRecursionStackBytes, [&] { result = emit(&shared_->progress); });
    } catch (...) {
      error = std::current_exception();
    }
    done.set();
  });
  while (!done.wait_for(interval_)) {
    if (pump_) {
      last_pump_ = Clock::now();
      pump_();
    }
  }
  worker.join();
  job.reset();
  if (outermost) job_ended();
  if (error) std::rethrow_exception(error);
  return result;
}

void MaskJobHost::job_ended() {
  if (busy()) return;
  if (on_end_) on_end_();
}

bool MaskJobHost::request_cancel() noexcept {
  if (!busy()) return false;
  shared_->cancel.store(true, std::memory_order_relaxed);
  return true;
}

std::optional<MaskJobState> MaskJobHost::state() const {
  if (!busy()) return std::nullopt;
  MaskJobState out;
  {
    const std::lock_guard lock(shared_->mutex);
    out.kind = shared_->kind;
    out.label = shared_->label;
    out.elapsed = Clock::now() - shared_->started;
  }
  if (shared_->reported.load(std::memory_order_acquire)) {
    out.fraction = shared_->fraction.load(std::memory_order_relaxed);
  }
  out.cancel_requested = shared_->cancel.load(std::memory_order_relaxed);
  return out;
}

MaskJobDrcLink::MaskJobDrcLink(MaskJobHost& jobs, DrcPump& pump) : jobs_(jobs), pump_(pump) {
  pump_.set_hold([&jobs] { return jobs.busy(); });
  jobs_.set_on_end([&pump] { pump.resume(); });
}

MaskJobDrcLink::~MaskJobDrcLink() {
  jobs_.set_on_end({});
  pump_.set_hold({});
}

}  // namespace hefesto::shell
