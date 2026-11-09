// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// POSIX FileHost: the core's local host plus stat()/kill() based crash
// recovery tests. Follows Magic 8.3.694 database/DBio.c (DBFileRecovery:
// st_uid against getuid(), st_ctime) with kill(pid, 0) instead of SIGCONT
// (cell-io-safety "Import of Magic crash backups").

#include "hefesto/platform_posix/file_host.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <csignal>

namespace hefesto::platform_posix {

namespace {

class PosixFileHost final : public cell::FileHost {
 public:
  PosixFileHost() : local_(cell::make_local_file_host()) {}

  bool is_symlink(const platform::Path& path) override { return local_->is_symlink(path); }
  bool is_directory(const platform::Path& path) override { return local_->is_directory(path); }
  std::optional<bool> same_file(const platform::Path& a, const platform::Path& b) override {
    return local_->same_file(a, b);
  }
  platform::Path current_directory() override { return local_->current_directory(); }
  bool writable(const platform::Path& path) override { return local_->writable(path); }
  std::optional<std::uint32_t> permissions(const platform::Path& path) override { return local_->permissions(path); }
  bool set_permissions(const platform::Path& path, std::uint32_t mode) override {
    return local_->set_permissions(path, mode);
  }
  std::optional<std::string> git_head(const platform::Path& directory) override { return local_->git_head(directory); }

  bool owned_by_current_user(const platform::Path& path) override {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && st.st_uid == ::getuid();
  }

  std::optional<std::int64_t> change_time(const platform::Path& path) override {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
      return std::nullopt;
    }
#if defined(__APPLE__)
    const struct timespec& ts = st.st_ctimespec;
#else
    const struct timespec& ts = st.st_ctim;
#endif
    constexpr std::int64_t kNanos = 1000000000;
    const std::int64_t seconds = ts.tv_sec;
    const std::int64_t nanos = ts.tv_nsec;
    return seconds * kNanos + nanos;
  }

  std::optional<bool> process_running(std::int64_t pid) override {
    if (pid <= 0 || pid > INT_MAX) {
      return false;  // not a process id this system can have
    }
    if (::kill(static_cast<pid_t>(pid), 0) == 0) {
      return true;
    }
    return errno == EPERM;  // exists, owned by someone else
  }

 private:
  std::unique_ptr<cell::FileHost> local_;
};

}  // namespace

std::unique_ptr<cell::FileHost> make_file_host() {
  return std::make_unique<PosixFileHost>();
}

}  // namespace hefesto::platform_posix
