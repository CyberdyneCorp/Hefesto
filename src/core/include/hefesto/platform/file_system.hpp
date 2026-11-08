// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// File access and document storage (capability hefesto-architecture,
// requirement "Platform services abstraction").

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::platform {

using Path = std::filesystem::path;

class FileSystem {
 public:
  FileSystem() = default;
  FileSystem(const FileSystem&) = delete;
  FileSystem& operator=(const FileSystem&) = delete;
  virtual ~FileSystem() = default;

  /// Whole-file read, in binary mode.
  [[nodiscard]] virtual diag::Outcome<std::string> read(const Path& path) = 0;
  /// Replaces the file atomically: readers see the old or the new content,
  /// never a partial file. Parent directories must exist.
  [[nodiscard]] virtual diag::Status write_atomic(const Path& path, std::string_view data) = 0;
  /// Appends to the file, creating it if needed (used by logs).
  [[nodiscard]] virtual diag::Status append(const Path& path, std::string_view data) = 0;
  [[nodiscard]] virtual bool exists(const Path& path) = 0;
  /// Size in bytes; PLATFORM-IO-004 when the file does not exist.
  [[nodiscard]] virtual diag::Outcome<std::uint64_t> size(const Path& path) = 0;
  /// Direct children of a directory, sorted by path so callers are deterministic.
  [[nodiscard]] virtual diag::Outcome<std::vector<Path>> list(const Path& directory) = 0;
  [[nodiscard]] virtual diag::Status create_directories(const Path& directory) = 0;
  /// Renames, replacing `to` if it exists.
  [[nodiscard]] virtual diag::Status rename(const Path& from, const Path& to) = 0;
  /// Removes a file; removing a missing file succeeds.
  [[nodiscard]] virtual diag::Status remove(const Path& path) = 0;
};

/// The portable default: std::filesystem and <fstream>.
class LocalFileSystem final : public FileSystem {
 public:
  diag::Outcome<std::string> read(const Path& path) override;
  diag::Status write_atomic(const Path& path, std::string_view data) override;
  diag::Status append(const Path& path, std::string_view data) override;
  bool exists(const Path& path) override;
  diag::Outcome<std::uint64_t> size(const Path& path) override;
  diag::Outcome<std::vector<Path>> list(const Path& directory) override;
  diag::Status create_directories(const Path& directory) override;
  diag::Status rename(const Path& from, const Path& to) override;
  diag::Status remove(const Path& path) override;
};

/// In-memory file system for tests and sandboxed hosts. Directories are
/// implicit: a directory exists when a file below it exists or it was created.
class MemoryFileSystem final : public FileSystem {
 public:
  diag::Outcome<std::string> read(const Path& path) override;
  diag::Status write_atomic(const Path& path, std::string_view data) override;
  diag::Status append(const Path& path, std::string_view data) override;
  bool exists(const Path& path) override;
  diag::Outcome<std::uint64_t> size(const Path& path) override;
  diag::Outcome<std::vector<Path>> list(const Path& directory) override;
  diag::Status create_directories(const Path& directory) override;
  diag::Status rename(const Path& from, const Path& to) override;
  diag::Status remove(const Path& path) override;

 private:
  [[nodiscard]] bool is_directory_locked(const Path& path) const;

  std::mutex mutex_;
  std::map<Path, std::string> files_;
  std::vector<Path> directories_;
};

}  // namespace hefesto::platform
