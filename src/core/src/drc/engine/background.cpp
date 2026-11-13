// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The background checker (design D8, Q-DRC-12): Magic's DRCContinuous
// loop moved to a worker thread. Magic runs it from the idle loop and
// breaks it on pending input (drc/DRCcontin.c DRCContinuous, DRCBreak;
// textio/txCommands.c); here the worker checks one square under the shared
// database lock, a waiting writer cancels it between tiles, and the host
// commits finished squares on the command thread.
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/engine/background.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "hefesto/drc/engine/engine.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::drc {

struct BackgroundChecker::Shared {
  std::mutex mutex;
  std::condition_variable cv;
  bool stop = false;
  bool woken = false;  // there may be work; cleared when the worker finds none
  std::uint64_t wakes = 0;  // wake() calls so far: a wake during a step keeps `woken`
  std::optional<SquareResult> ready;  // a finished square awaiting commit_ready()
  std::atomic<bool> cancel{false};
  std::function<void()> on_ready;
  std::function<void()> after_step;  // test hook (set_after_step_hook)
};

namespace {

enum class Step : std::uint8_t { result, nothing, cancelled };

// One square under the shared lock.
Step check_one(editor::EditorState& editor, const std::atomic<bool>& cancel, std::optional<SquareResult>& out) {
  if (editor.database_lock.writer_waiting()) {
    return Step::cancelled;
  }
  const db::DatabaseLock::Shared guard(editor.database_lock);
  if (editor.drc.background != BackgroundMode::on || editor.drc.pending.empty()) {
    return Step::nothing;
  }
  out = process_square(editor, CancelToken{&cancel, &editor.database_lock});
  return out ? Step::result : Step::cancelled;
}

}  // namespace

BackgroundChecker::BackgroundChecker() : shared_(std::make_unique<Shared>()) {}

BackgroundChecker::~BackgroundChecker() {
  stop();
}

void BackgroundChecker::set_ready_callback(std::function<void()> callback) {
  const std::lock_guard lock(shared_->mutex);
  shared_->on_ready = std::move(callback);
}

void BackgroundChecker::set_after_step_hook(std::function<void()> hook) {
  const std::lock_guard lock(shared_->mutex);
  shared_->after_step = std::move(hook);
}

void BackgroundChecker::start(editor::EditorState& editor) {
  if (thread_.joinable()) {
    return;
  }
  {
    const std::lock_guard lock(shared_->mutex);
    shared_->stop = false;
    shared_->woken = true;
    shared_->cancel = false;
  }
  editor_ = &editor;
  thread_ = std::thread([this, &editor] { run(editor); });
}

void BackgroundChecker::stop() {
  if (!thread_.joinable()) {
    return;
  }
  {
    const std::lock_guard lock(shared_->mutex);
    shared_->stop = true;
    shared_->cancel = true;
  }
  shared_->cv.notify_all();
  thread_.join();
  {
    const std::lock_guard lock(shared_->mutex);
    shared_->ready.reset();
  }
  if (editor_ != nullptr && editor_->drc.events.state() == CheckerState::busy) {
    editor_->drc.events.publish_state(CheckerState::idle);
  }
}

void BackgroundChecker::wake() {
  if (!running()) {
    return;
  }
  if (editor_->drc.background == BackgroundMode::on && !editor_->drc.pending.empty()) {
    editor_->drc.events.publish_state(CheckerState::busy);
  }
  {
    const std::lock_guard lock(shared_->mutex);
    shared_->woken = true;
    ++shared_->wakes;
  }
  shared_->cv.notify_all();
}

bool BackgroundChecker::running() const noexcept {
  return thread_.joinable();
}

void BackgroundChecker::run(editor::EditorState& editor) {
  std::unique_lock lock(shared_->mutex);
  while (!shared_->stop) {
    if (shared_->ready || !shared_->woken) {
      shared_->cv.wait(lock);
      continue;
    }
    const std::uint64_t wakes_seen = shared_->wakes;
    const std::function<void()> after_step = shared_->after_step;
    lock.unlock();
    std::optional<SquareResult> result;
    const Step step = check_one(editor, shared_->cancel, result);
    if (after_step) {
      after_step();
    }
    if (step == Step::cancelled) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));  // let the writer in
    }
    std::function<void()> notify;
    lock.lock();
    if (step == Step::result && !shared_->stop) {
      shared_->ready = std::move(result);
      notify = shared_->on_ready;
    } else if (step == Step::nothing && shared_->wakes == wakes_seen) {
      // A wake() since this step began (drc on, a new edit) may have work
      // the step did not see: check again instead of sleeping on it.
      shared_->woken = false;
    }
    shared_->cv.notify_all();
    if (notify) {
      lock.unlock();
      notify();
      lock.lock();
    }
  }
}

std::size_t BackgroundChecker::commit_ready(editor::EditorState& editor) {
  std::optional<SquareResult> result;
  {
    const std::lock_guard lock(shared_->mutex);
    result = std::move(shared_->ready);
    shared_->ready.reset();
  }
  if (!result) {
    return 0;
  }
  std::size_t committed = 0;
  {
    const db::DatabaseLock::Exclusive guard(editor.database_lock);
    // The square is still current: no edit, no other commit, and its cell
    // still heads the list (a deleted cell leaves it, DRCRemovePending).
    if (result->generation == generation() && result->epoch == epoch() &&
        editor.drc.pending.front() == result->cell) {
      commit(editor, *result);
      committed = 1;
    }
    if (editor.drc.pending.empty()) {
      editor.drc.events.publish_state(CheckerState::idle);
    }
  }
  shared_->cv.notify_all();
  return committed;
}

bool BackgroundChecker::idle_locked() const {
  return !shared_->ready && !shared_->woken;
}

bool BackgroundChecker::run_until_idle(editor::EditorState& editor, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (true) {
    (void)commit_ready(editor);
    std::unique_lock lock(shared_->mutex);
    if (!running() || idle_locked()) {
      return true;
    }
    if (shared_->cv.wait_until(lock, deadline, [this] { return shared_->ready.has_value() || idle_locked(); })) {
      continue;
    }
    return idle_locked();
  }
}

}  // namespace hefesto::drc
