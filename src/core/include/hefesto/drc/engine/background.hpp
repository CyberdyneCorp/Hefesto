// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The background checker (drc-engine "Pending list and background
// scheduling", Q-DRC-12; openspec/changes/m3-drc/design.md D8).
//
// One worker thread per session checks one step square at a time
// (drc::process_square) while holding editor::EditorState::database_lock
// shared, polls db::DatabaseLock::writer_waiting() and its own cancel flag
// between tiles, and drops a partial square when a command wants the lock
// (Magic's DRC_BREAK_PENDING). A finished square waits, unapplied, for the
// host: commit_ready() applies it on the command thread between commands
// (taking the lock exclusively) only if neither the edit generation nor the
// commit epoch changed since the square started; otherwise the square is
// dropped and its check tiles stay for a retry. The worker starts the next
// square only after its last one was committed or dropped, so every square
// sees the check plane its predecessor left, as in Magic.
//
//   - The edit generation counts database edits: drc::check_this bumps it
//     (every marking site calls it); hosts call bump_generation() for any
//     other change a square may read (file reads, technology changes).
//   - The commit epoch counts applied squares (drc::commit advances it), so
//     a square computed before `drc catchup` or another commit is dropped.
//
// Hosts: the desktop app starts the worker while background DRC is on,
// calls wake() after every command and commit_ready() from its event loop
// whenever the ready callback fires; `hefesto-cli` never starts it while
// running a script or -c commands (Magic in batch mode); `drc catchup`
// runs the same square loop synchronously (drc::catchup, engine.hpp).
// Busy/idle are published through DrcSession::events on the command
// thread: busy by wake() when it finds work, idle by commit_ready() when
// the pending list becomes empty (and by stop() while busy).
//
// Owner: ENGINE (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::drc {

struct SquareResult;

/// Background state (DRCBackGround): off, on, or "not set" for the duration
/// of a command (the checker never runs inside a command).
enum class BackgroundMode : unsigned char { off, on, not_set };

class BackgroundChecker {
 public:
  BackgroundChecker();
  ~BackgroundChecker();  // stops and joins the worker
  BackgroundChecker(const BackgroundChecker&) = delete;
  BackgroundChecker& operator=(const BackgroundChecker&) = delete;

  /// Starts the worker over `editor` (no-op when running). The editor must
  /// outlive the worker or stop() must be called first.
  void start(editor::EditorState& editor);
  /// Asks the worker to stop and joins it; a square in progress or waiting
  /// for its commit is dropped. Publishes idle when the checker was busy
  /// (call it from the command thread).
  void stop();
  /// After a command (command thread): publishes busy when there is work
  /// and background checking is on, and wakes the worker.
  void wake();
  [[nodiscard]] bool running() const noexcept;

  /// Called on the worker thread whenever a finished square waits for
  /// commit_ready(); the host posts a call to its event loop. Set before
  /// start().
  void set_ready_callback(std::function<void()> callback);

  /// Test hook: runs on the worker thread after each step, between the
  /// check and the worker taking its mutex again.
  void set_after_step_hook(std::function<void()> hook);

  /// Applies the finished square, if any, when its generation and epoch
  /// still match, holding the database lock exclusively; returns the number
  /// committed (0 or 1). Publishes idle when the pending list becomes
  /// empty. Must be called on the command thread between commands.
  std::size_t commit_ready(editor::EditorState& editor);

  /// Blocks until the worker is idle (nothing to check, or background
  /// checking off) or `timeout_ms` passed, committing finished squares on
  /// the calling thread meanwhile; true when idle. For hosts without an
  /// event loop and for tests.
  bool run_until_idle(editor::EditorState& editor, int timeout_ms);

  /// The database edit generation.
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_.load(std::memory_order_acquire); }
  /// Records a database edit (called by drc::check_this).
  void bump_generation() noexcept { generation_.fetch_add(1, std::memory_order_acq_rel); }
  /// The commit epoch (squares applied so far).
  [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_.load(std::memory_order_acquire); }
  /// Records an applied square (called by drc::commit).
  void advance_epoch() noexcept { epoch_.fetch_add(1, std::memory_order_acq_rel); }

 private:
  struct Shared;
  void run(editor::EditorState& editor);
  [[nodiscard]] bool idle_locked() const;

  std::atomic<std::uint64_t> generation_{0};
  std::atomic<std::uint64_t> epoch_{0};
  std::unique_ptr<Shared> shared_;
  std::thread thread_;
  editor::EditorState* editor_ = nullptr;
};

}  // namespace hefesto::drc
