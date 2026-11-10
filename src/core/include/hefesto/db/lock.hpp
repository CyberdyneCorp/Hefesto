// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The database reader/writer lock (capability hefesto-architecture,
// "Concurrency model": one writer, the command thread; concurrent readers
// such as scene building and the background DRC worker). One lock guards
// the whole cell library of a session (editor::EditorState::database_lock).
//
//   - cmd::Dispatcher::execute holds it exclusively for every command it
//     runs (including the post-command update), so a reader never sees a
//     half-done edit. Commands run nested commands (macros, `source`) on the
//     same thread: the exclusive lock is re-entrant for its owner thread.
//   - Readers hold it shared: the DRC worker while it checks one square
//     (openspec/changes/m3-drc/design.md D8), the shell while it builds a
//     scene. A shared request from the thread that already holds the lock
//     exclusively (a scene built from inside a command) is admitted at once.
//   - A writer announces itself before it blocks: writer_waiting() turns true
//     until the writer owns the lock. Long readers (the DRC worker) poll it
//     between tiles and release the lock, dropping their partial result
//     (Magic's DRC_BREAK_PENDING), so a command waits well under 1 ms.
//
// Only the RAII guards lock and unlock. Owner: INTEGRATION (shared;
// openspec/changes/m3-drc/design.md D2/D4).

#pragma once

#include <atomic>
#include <shared_mutex>
#include <thread>

namespace hefesto::db {

class DatabaseLock {
 public:
  DatabaseLock() = default;
  DatabaseLock(const DatabaseLock&) = delete;
  DatabaseLock& operator=(const DatabaseLock&) = delete;

  /// Exclusive ownership for the guard's lifetime (re-entrant for the owner thread).
  class Exclusive {
   public:
    explicit Exclusive(DatabaseLock& lock);
    ~Exclusive();
    Exclusive(const Exclusive&) = delete;
    Exclusive& operator=(const Exclusive&) = delete;

   private:
    DatabaseLock& lock_;
  };

  /// Shared ownership for the guard's lifetime. Blocks while a writer holds
  /// the lock; admitted at once on the thread that holds it exclusively.
  class Shared {
   public:
    explicit Shared(DatabaseLock& lock);
    ~Shared();
    Shared(const Shared&) = delete;
    Shared& operator=(const Shared&) = delete;

   private:
    DatabaseLock& lock_;
    bool nested_ = false;  // taken inside the owner's exclusive section
  };

  /// True while a writer waits for (or is about to take) the lock. Readers
  /// that hold the lock for long poll this and release it.
  [[nodiscard]] bool writer_waiting() const noexcept { return waiting_writers_.load(std::memory_order_acquire) > 0; }

  /// True when the calling thread holds the lock exclusively.
  [[nodiscard]] bool held_exclusively_by_this_thread() const noexcept {
    return owner_.load(std::memory_order_acquire) == std::this_thread::get_id();
  }

 private:
  std::shared_mutex mutex_;
  std::atomic<std::thread::id> owner_{};
  int depth_ = 0;  // exclusive nesting depth; touched by the owner thread only
  std::atomic<int> waiting_writers_{0};
};

}  // namespace hefesto::db
