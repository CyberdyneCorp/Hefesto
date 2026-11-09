// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Host file facilities that cell resolution and I/O safety need beyond
// platform::FileSystem: symbolic-link detection, file identity, the working
// directory, writability and permissions, and the git HEAD test
// (cell-library-resolution "Host mechanisms for resolution", Q-CELL-17,
// Q-CELL-22, Q-CELL-23; cell-io-safety "Read-only cell detection").
// The core keeps no POSIX calls: the local implementation uses only
// std::filesystem and <cstdio>; hosts may install their own.
//
// Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "hefesto/platform/file_system.hpp"

namespace hefesto::cell {

class FileHost {
 public:
  FileHost() = default;
  FileHost(const FileHost&) = delete;
  FileHost& operator=(const FileHost&) = delete;
  virtual ~FileHost() = default;

  /// lstat(): the final component is a symbolic link.
  [[nodiscard]] virtual bool is_symlink(const platform::Path& path) = 0;
  /// stat(): an existing directory (links followed).
  [[nodiscard]] virtual bool is_directory(const platform::Path& path) = 0;
  /// Same platform file identity (device and inode, or the file resource
  /// identifier on Apple platforms, Q-CELL-22); nullopt when either path
  /// cannot be examined.
  [[nodiscard]] virtual std::optional<bool> same_file(const platform::Path& a, const platform::Path& b) = 0;
  /// getcwd(): absolute and free of links; empty when unknown.
  [[nodiscard]] virtual platform::Path current_directory() = 0;
  /// "Read-only cell detection" rules 1-4 for an existing file: false when
  /// the path is not a regular file (links not followed), cannot be opened
  /// for update, or has no write permission bit for the process.
  [[nodiscard]] virtual bool writable(const platform::Path& path) = 0;
  /// POSIX permission bits (low 12 bits); nullopt when unknown.
  [[nodiscard]] virtual std::optional<std::uint32_t> permissions(const platform::Path& path) = 0;
  virtual bool set_permissions(const platform::Path& path, std::uint32_t mode) = 0;
  /// HEAD commit of the git work tree containing `directory`; nullopt when
  /// git is unavailable or the directory is not in a work tree
  /// (inconclusive, Q-CELL-23).
  [[nodiscard]] virtual std::optional<std::string> git_head(const platform::Path& directory) = 0;

  // Crash recovery ("Crash recovery", "Import of Magic crash backups").
  // The defaults suit hosts without the facility; a POSIX host overrides
  // them (src/platform_posix).

  /// The file is owned by the current user. Default: true (the crash
  /// backup directory is private to the user).
  [[nodiscard]] virtual bool owned_by_current_user(const platform::Path& path);
  /// The file's status-change time (any monotonic file time when the host
  /// has no ctime), in nanoseconds since an arbitrary epoch; nullopt when
  /// unknown.
  [[nodiscard]] virtual std::optional<std::int64_t> change_time(const platform::Path& path);
  /// Whether process `pid` is running, tested without affecting it
  /// (kill(pid, 0)); nullopt when the host cannot tell, which disables the
  /// import of Magic crash backups.
  [[nodiscard]] virtual std::optional<bool> process_running(std::int64_t pid);
};

/// The local disk through std::filesystem. git_head is always inconclusive
/// (the core spawns no processes; a host with a ProcessRunner may override).
[[nodiscard]] std::unique_ptr<FileHost> make_local_file_host();

/// For in-memory sessions and tests: no links, no identities (only equal
/// paths are the same file), every existing file writable, a fixed working
/// directory. Directories are answered through `fs`.
[[nodiscard]] std::unique_ptr<FileHost> make_memory_file_host(platform::FileSystem& fs,
                                                              platform::Path current_directory = "/");

}  // namespace hefesto::cell
