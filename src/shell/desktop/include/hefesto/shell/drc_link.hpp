// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop app's side of the background DRC checker (openspec/changes/
// m3-drc/design.md D8 "Hosts", D11; tasks.md 9.1, 9.2):
//
//   - DrcPump owns the session's drc::BackgroundHost. The worker's ready
//     callback (worker thread) only posts one task to the UI event loop
//     through the injected poster, coalescing repeats; the task runs
//     BackgroundHost::service() on the UI thread, which is the command
//     thread, between commands. `drc on` / `drc off` start and stop the
//     checking through DRCBackGround (the worker idles while it is off).
//   - DrcLink observes drc::DrcEvents: a committed square marks its area
//     for redisplay in every window showing it (view::WindowManager::
//     area_changed, so the error tiles under it are redrawn), busy and idle
//     drive the chip, and the time from busy to idle is the job length for
//     the completion notice.
//
// Raven-free: the poster is a function (ui::post_task in the app, a queue in
// tests).
//
// Owner: SHELL (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "hefesto/drc/engine/host.hpp"
#include "hefesto/drc/events.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::shell {

class DrcPump {
 public:
  /// Runs `task` on the UI thread soon; must be callable from any thread.
  using Post = std::function<void(std::function<void()> task)>;

  /// Starts the background checker over `editor` (after startup, design D8
  /// "Hosts" step 1). `after_service(committed)` runs on the UI thread after
  /// each service() the posted task ran.
  DrcPump(editor::EditorState& editor, Post post, std::function<void(std::size_t)> after_service);
  ~DrcPump();
  DrcPump(const DrcPump&) = delete;
  DrcPump& operator=(const DrcPump&) = delete;

  /// What the worker's ready callback does (any thread): posts one service
  /// task unless one is already waiting.
  void notify();
  /// Commits a finished square now (UI thread); the posted task calls it.
  std::size_t service();
  [[nodiscard]] bool busy() const noexcept { return host_.busy(); }

 private:
  struct State {
    std::atomic<bool> posted{false};
    std::function<std::size_t()> service;
  };
  Post post_;
  std::function<void(std::size_t)> after_service_;
  std::shared_ptr<State> state_;  // declared before host_: alive until the worker stopped
  drc::BackgroundHost host_;
};

class DrcLink final : public drc::DrcObserver {
 public:
  using Clock = std::chrono::steady_clock;

  /// Observes `editor.drc.events` until destroyed.
  explicit DrcLink(editor::EditorState& editor);
  ~DrcLink() override;
  DrcLink(const DrcLink&) = delete;
  DrcLink& operator=(const DrcLink&) = delete;

  void on_state(drc::CheckerState state) override;
  void on_square_committed(const db::CellDef& cell, const db::Rect& area) override;
  void on_style_changed(const std::string& style) override;

  /// The root shown changed: it counts as not checked until a square of it
  /// is committed or the checker goes idle after work.
  void set_root(const db::CellDef* root);
  /// The shown root was checked since set_root().
  [[nodiscard]] bool root_checked() const noexcept { return root_checked_; }
  [[nodiscard]] bool busy() const noexcept { return busy_; }
  /// Changes every event (state, square, style): the UI compares it.
  [[nodiscard]] std::uint64_t serial() const noexcept { return serial_; }
  /// Seconds of the last busy-to-idle run, once (the completion notice).
  [[nodiscard]] std::optional<double> take_finished_job();
  /// Squares committed since the last call.
  [[nodiscard]] std::size_t take_committed() noexcept { return std::exchange(committed_, 0); }

  /// Stamp of the database state a browser list was built from: the edit
  /// generation and the commit epoch of the worker (both 0 without one).
  struct Stamp {
    std::uint64_t generation = 0;
    std::uint64_t epoch = 0;
    std::uint64_t style_changes = 0;
    friend bool operator==(const Stamp&, const Stamp&) = default;
  };
  [[nodiscard]] Stamp stamp() const noexcept;

  /// Test hook: the clock used for job lengths.
  void set_clock(std::function<Clock::time_point()> clock) { clock_ = std::move(clock); }

 private:
  editor::EditorState& editor_;
  const db::CellDef* root_ = nullptr;  // compared, never dereferenced
  bool root_checked_ = false;
  bool busy_ = false;
  std::uint64_t serial_ = 0;
  std::uint64_t style_changes_ = 0;
  std::size_t committed_ = 0;
  Clock::time_point busy_since_{};
  std::optional<double> finished_;
  std::function<Clock::time_point()> clock_ = [] { return Clock::now(); };
};

}  // namespace hefesto::shell
