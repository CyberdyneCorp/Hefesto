// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// POSIX implementation of the cell FileLockService (cell-io-safety
// "Advisory locking between sessions", "Locking mechanism"): fcntl()
// whole-file write locks on a descriptor kept open while the lock is held,
// the holder reported as "pid <n>". It lives in a host because the core
// makes no POSIX calls. Because POSIX locks belong to the process, closing
// any descriptor of a locked file drops the lock: the core reads a cell
// file before locking it.

#pragma once

#include <map>

#include "hefesto/cell/io_safety.hpp"

namespace hefesto::cli {

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

}  // namespace hefesto::cli
