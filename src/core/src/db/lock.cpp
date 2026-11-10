// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::DatabaseLock (openspec/changes/m3-drc/design.md D4/D8).
// Owner: INTEGRATION (shared).

#include "hefesto/db/lock.hpp"

namespace hefesto::db {

DatabaseLock::Exclusive::Exclusive(DatabaseLock& lock) : lock_(lock) {
  if (lock_.held_exclusively_by_this_thread()) {
    ++lock_.depth_;
    return;
  }
  lock_.waiting_writers_.fetch_add(1, std::memory_order_acq_rel);
  lock_.mutex_.lock();
  lock_.waiting_writers_.fetch_sub(1, std::memory_order_acq_rel);
  lock_.owner_.store(std::this_thread::get_id(), std::memory_order_release);
  lock_.depth_ = 1;
}

DatabaseLock::Exclusive::~Exclusive() {
  if (--lock_.depth_ > 0) {
    return;
  }
  lock_.owner_.store(std::thread::id{}, std::memory_order_release);
  lock_.mutex_.unlock();
}

DatabaseLock::Shared::Shared(DatabaseLock& lock) : lock_(lock), nested_(lock.held_exclusively_by_this_thread()) {
  if (!nested_) {
    lock_.mutex_.lock_shared();
  }
}

DatabaseLock::Shared::~Shared() {
  if (!nested_) {
    lock_.mutex_.unlock_shared();
  }
}

}  // namespace hefesto::db
