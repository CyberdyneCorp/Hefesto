// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// POSIX cell file locks. Follows Magic 8.3.694 utils/flock.c (flock_open):
// F_GETLK for a conflicting whole-file write lock, then F_SETLK on a
// descriptor kept open while the lock is held.

#include "posix_lock_service.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <string>

namespace hefesto::cli {

namespace {

struct flock whole_file_write_lock() {
  struct flock fl {};
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;
  return fl;
}

}  // namespace

PosixLockService::~PosixLockService() {
  for (const auto& entry : fds_) {
    (void)::close(entry.second);
  }
}

diag::Outcome<bool> PosixLockService::acquire(const platform::Path& file, std::string* owner) {
  if (fds_.contains(file)) {
    return true;
  }
  const int fd = ::open(file.c_str(), O_RDWR);  // NOLINT(cppcoreguidelines-pro-type-vararg)
  if (fd < 0) {
    return false;  // cannot open for update: the same result as a held lock
  }
  struct flock query = whole_file_write_lock();
  if (::fcntl(fd, F_GETLK, &query) == 0 && query.l_type != F_UNLCK) {  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (owner != nullptr) {
      *owner = query.l_pid > 0 ? "pid " + std::to_string(query.l_pid) : std::string{"another process"};
    }
    (void)::close(fd);
    return false;
  }
  struct flock set = whole_file_write_lock();
  if (::fcntl(fd, F_SETLK, &set) != 0) {  // NOLINT(cppcoreguidelines-pro-type-vararg)
    (void)::close(fd);  // Magic reports with perror and carries on unlocked
    return true;
  }
  fds_.emplace(file, fd);
  return true;
}

void PosixLockService::release(const platform::Path& file) {
  if (const auto it = fds_.find(file); it != fds_.end()) {
    (void)::close(it->second);
    fds_.erase(it);
  }
}

}  // namespace hefesto::cli
