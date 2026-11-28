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
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::platform {

using Path = std::filesystem::path;

/// A file as C's fopen(path, "r") and a read loop see it: the text, or the
/// errno of the failed open in `error` (std::generic_category, so
/// error.message() is the text perror prints). Callers that copy Magic's
/// fopen/perror pairs (efSymAddFile) use it instead of reading the reason
/// back out of a diagnostic.
struct StreamRead {
  std::string text;
  std::error_code error;
};

class FileSystem {
 public:
  FileSystem() = default;
  FileSystem(const FileSystem&) = delete;
  FileSystem& operator=(const FileSystem&) = delete;
  virtual ~FileSystem() = default;

  /// Whole-file read, in binary mode. A missing file fails with
  /// PLATFORM-IO-004 (`no such file '<path>'`); any other failure with
  /// PLATFORM-IO-001, whose text ends in `: <strerror text>` when the OS
  /// gave a reason (`: Permission denied`, `: Is a directory`).
  [[nodiscard]] virtual diag::Outcome<std::string> read(const Path& path) = 0;
  /// The first `max_bytes` bytes of the file (all of it when it is
  /// shorter), with the errors of read(). Bounds the memory a read of a file
  /// that grows meanwhile can take, whatever size() said before. The
  /// default reads the whole file and cuts it; LocalFileSystem never reads
  /// more than `max_bytes`.
  [[nodiscard]] virtual diag::Outcome<std::string> read_at_most(const Path& path, std::size_t max_bytes);
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
  /// The time the file was last modified, to notice that it changed since
  /// it was read; nullopt when unknown (no such file, or a file system that
  /// keeps no times: the default).
  [[nodiscard]] virtual std::optional<std::filesystem::file_time_type> modified_time(const Path& path);
  /// The file as fopen(path, "r") reads it. Anything that exists but is not
  /// a regular file (a directory, a device, a FIFO) reads as empty with no
  /// error: fopen opens a directory and fgets reads nothing from it, and a
  /// FIFO or an endless device is not waited on. The default maps read():
  /// PLATFORM-IO-004 to ENOENT, any other failure to EIO.
  [[nodiscard]] virtual StreamRead read_stream(const Path& path);
};

/// The portable default: std::filesystem and <fstream>. A read error
/// carries the OS reason: errno of the failed open, `Is a directory` for a
/// directory (an ifstream would open it and read nothing). read_stream()
/// reports the failed fopen's errno itself (ENOTDIR, EACCES, ...).
class LocalFileSystem final : public FileSystem {
 public:
  diag::Outcome<std::string> read(const Path& path) override;
  diag::Outcome<std::string> read_at_most(const Path& path, std::size_t max_bytes) override;
  StreamRead read_stream(const Path& path) override;
  diag::Status write_atomic(const Path& path, std::string_view data) override;
  diag::Status append(const Path& path, std::string_view data) override;
  bool exists(const Path& path) override;
  diag::Outcome<std::uint64_t> size(const Path& path) override;
  diag::Outcome<std::vector<Path>> list(const Path& directory) override;
  diag::Status create_directories(const Path& directory) override;
  diag::Status rename(const Path& from, const Path& to) override;
  diag::Status remove(const Path& path) override;
  std::optional<std::filesystem::file_time_type> modified_time(const Path& path) override;
};

/// In-memory file system for tests and sandboxed hosts. Directories are
/// implicit: a directory exists when a file below it exists or it was created.
class MemoryFileSystem final : public FileSystem {
 public:
  diag::Outcome<std::string> read(const Path& path) override;
  StreamRead read_stream(const Path& path) override;
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
