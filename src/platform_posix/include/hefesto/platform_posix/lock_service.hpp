// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// POSIX implementation of the cell FileLockService (cell-io-safety
// "Advisory locking between sessions", "Locking mechanism", "Crash backup
// location and session lock"): fcntl() whole-file write locks on a
// descriptor kept open while the lock is held, the holder reported as
// "pid <n>". Because POSIX locks belong to the process, closing any
// descriptor of a locked file drops the lock: the core reads a cell file
// before locking it, and two sessions in one process do not exclude each
// other (use the in-process service's shared registry for that).
//
// Install it before any command runs (the per-session CELL state lives in
// the service): `editor.locks = hefesto::platform_posix::make_lock_service();`
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <map>
#include <memory>
#include <string>

#include "hefesto/cell/io_safety.hpp"

namespace hefesto::platform_posix {

class PosixLockService final : public cell::FileLockService {
 public:
  PosixLockService() = default;
  ~PosixLockService() override;
  PosixLockService(const PosixLockService&) = delete;
  PosixLockService& operator=(const PosixLockService&) = delete;

  [[nodiscard]] diag::Outcome<bool> acquire(const platform::Path& file, std::string* owner) override;
  void release(const platform::Path& file) override;
  [[nodiscard]] bool holds(const platform::Path& file) const override { return fds_.contains(file); }

 private:
  std::map<platform::Path, int> fds_;
};

/// The factory front ends install into editor::EditorState::locks.
[[nodiscard]] std::unique_ptr<cell::FileLockService> make_lock_service();

}  // namespace hefesto::platform_posix
