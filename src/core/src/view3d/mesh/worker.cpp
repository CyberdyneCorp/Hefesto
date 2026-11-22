// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// MeshWorker (worker.hpp; openspec/changes/m10-3d-view/design.md D4, D6).
// One thread waits for work: a queued full build first, else the recorded
// dirty areas once a model and its inputs exist. The cancel flag is reset
// under the mutex when a job is taken, and set under it by build() and
// cancel(), so a request never cancels the job that follows it. A job that
// finishes while the flag is set is dropped like a cancelled one.
//
// Owner: MESH (design D3).

#include "hefesto/view3d/mesh/worker.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "internal.hpp"

namespace hefesto::view3d {

struct MeshWorker::State {
  WorkerCallbacks callbacks;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::condition_variable idle;
  std::optional<BuildInputs> queued;  // a full build waiting
  std::optional<BuildInputs> inputs;  // those of the current model
  std::vector<db::Rect> dirty;        // recorded since the current model's build started
  std::vector<db::Rect> parked;       // areas of cancelled rebuilds, joined to the next changed()
  MeshHandle current;
  bool running = false;
  bool stopping = false;
  std::atomic<bool> cancel{false};
  std::thread thread;

  // One job taken from the queue.
  struct Job {
    BuildInputs inputs;
    MeshHandle previous;           // null for a full build
    std::vector<db::Rect> dirty;   // a rebuild's areas
  };

  [[nodiscard]] bool has_work() const { return queued.has_value() || (!dirty.empty() && current && inputs); }

  void run() {
    for (;;) {
      std::optional<Job> job = take();
      if (!job) {
        return;
      }
      std::optional<MeshModel> model = execute(*job);
      finish(*job, std::move(model));
    }
  }

  // Waits for work; nullopt when stopping.
  std::optional<Job> take() {
    std::unique_lock lock(mutex);
    wake.wait(lock, [this] { return stopping || has_work(); });
    if (stopping) {
      return std::nullopt;
    }
    cancel.store(false, std::memory_order_relaxed);
    running = true;
    Job job;
    if (queued) {
      job.inputs = std::move(*queued);
      queued.reset();
      dirty.clear();
      parked.clear();
    } else {
      job.inputs = *inputs;
      job.previous = current;
      job.dirty = std::exchange(dirty, {});
    }
    return job;
  }

  std::optional<MeshModel> execute(const Job& job) {
    const BuildInputs& in = job.inputs;
    if (!in.stack || in.cell == nullptr) {
      return std::nullopt;
    }
    BuildContext ctx;
    ctx.progress.cancel = &cancel;
    ctx.progress.report = callbacks.on_progress;
    ctx.on_chunk = callbacks.on_chunk;
    ctx.span_connected_layers = in.span_connected_layers;
    ctx.technology = in.technology;
    ctx.centre = in.centre;
    ctx.lock = in.lock;
    if (job.previous) {
      return detail::rebuild_layout(*job.previous, *in.stack, *in.cell, job.dirty, ctx, in.max_chunk_tiles);
    }
    return detail::build_layout(*in.stack, *in.cell, in.clip, ctx, in.max_chunk_tiles);
  }

  // `running` stays true until on_done has returned, so wait_idle() and
  // idle() never report idle while the callback is still running.
  void finish(Job& job, std::optional<MeshModel> model) {
    MeshHandle done;
    {
      const std::lock_guard lock(mutex);
      const bool dropped = !model || cancel.load(std::memory_order_relaxed);
      if (dropped && job.previous && !queued) {
        // The model still lacks these edits: keep them for the next change.
        parked.insert(parked.end(), job.dirty.begin(), job.dirty.end());
      }
      if (!dropped) {
        done = std::make_shared<const MeshModel>(std::move(*model));
        current = done;
        if (!job.previous) {
          inputs = std::move(job.inputs);
        }
      }
    }
    if (done && callbacks.on_done) {
      callbacks.on_done(done);
    }
    {
      const std::lock_guard lock(mutex);
      running = false;
    }
    idle.notify_all();
  }
};

MeshWorker::MeshWorker(WorkerCallbacks callbacks) : state_(std::make_unique<State>()) {
  state_->callbacks = std::move(callbacks);
  state_->thread = std::thread([s = state_.get()] { s->run(); });
}

MeshWorker::~MeshWorker() {
  {
    const std::lock_guard lock(state_->mutex);
    state_->stopping = true;
    state_->cancel.store(true, std::memory_order_relaxed);
  }
  state_->wake.notify_all();
  state_->thread.join();
}

void MeshWorker::build(BuildInputs inputs) {
  {
    const std::lock_guard lock(state_->mutex);
    state_->queued = std::move(inputs);
    state_->dirty.clear();
    state_->parked.clear();
    if (state_->running) {
      state_->cancel.store(true, std::memory_order_relaxed);
    }
  }
  state_->wake.notify_all();
}

void MeshWorker::changed(std::span<const db::Rect> dirty) {
  if (dirty.empty()) {
    return;
  }
  {
    const std::lock_guard lock(state_->mutex);
    State& s = *state_;
    s.dirty.insert(s.dirty.end(), s.parked.begin(), s.parked.end());
    s.parked.clear();
    s.dirty.insert(s.dirty.end(), dirty.begin(), dirty.end());
  }
  state_->wake.notify_all();
}

void MeshWorker::cancel() {
  const std::lock_guard lock(state_->mutex);
  state_->queued.reset();
  // The areas are parked, not dropped: the current model does not have
  // them, so the next changed() rebuilds them too.
  State& s = *state_;
  s.parked.insert(s.parked.end(), s.dirty.begin(), s.dirty.end());
  s.dirty.clear();
  if (state_->running) {
    state_->cancel.store(true, std::memory_order_relaxed);
  }
}

MeshHandle MeshWorker::current() const {
  const std::lock_guard lock(state_->mutex);
  return state_->current;
}

bool MeshWorker::idle() const {
  const std::lock_guard lock(state_->mutex);
  return !state_->running && !state_->has_work();
}

bool MeshWorker::wait_idle(int timeout_ms) {
  std::unique_lock lock(state_->mutex);
  return state_->idle.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                               [this] { return !state_->running && !state_->has_work(); });
}

}  // namespace hefesto::view3d
