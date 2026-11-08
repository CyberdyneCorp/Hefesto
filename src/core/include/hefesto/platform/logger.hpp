// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Logging service and the rotating session log (capability
// hefesto-diagnostics, requirement "Diagnostic presentation and logging").

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform {

class Logger {
 public:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  virtual ~Logger() = default;

  /// Records one message. Thread-safe in every implementation.
  virtual void write(diag::Severity level, std::string_view message) = 0;

  /// Records a diagnostic in its text form.
  void log(const diag::Diagnostic& diagnostic);
};

class NullLogger final : public Logger {
 public:
  void write(diag::Severity /*level*/, std::string_view /*message*/) override {}
};

/// Keeps lines in memory ("<severity> <message>"), for tests.
class MemoryLogger final : public Logger {
 public:
  void write(diag::Severity level, std::string_view message) override;
  [[nodiscard]] std::vector<std::string> lines() const;

 private:
  mutable std::mutex mutex_;
  std::vector<std::string> lines_;
};

struct RotationPolicy {
  std::uint64_t max_bytes = 1024 * 1024;  // rotate before a file would exceed this
  std::uint32_t max_files = 5;            // current file plus rotated ones (>= 1)
};

/// Session log in `directory`: `<base>.log`, rotated to `<base>.1.log` …
/// `<base>.<max_files-1>.log` (oldest is dropped). Lines are
/// "<UTC timestamp> <severity> <message>". Write failures are swallowed after
/// the first one is remembered in last_error(), so logging never fails a command.
class RotatingFileLogger final : public Logger {
 public:
  RotatingFileLogger(FileSystem& fs, Clock& clock, Path directory, std::string base_name,
                     RotationPolicy policy = {});

  void write(diag::Severity level, std::string_view message) override;

  [[nodiscard]] Path current_path() const;
  [[nodiscard]] std::optional<diag::Diagnostic> last_error() const;

 private:
  [[nodiscard]] Path numbered_path(std::uint32_t index) const;
  void rotate_locked();
  void record_failure_locked(const diag::Status& status);

  FileSystem& fs_;
  Clock& clock_;
  Path directory_;
  std::string base_name_;
  RotationPolicy policy_;
  mutable std::mutex mutex_;
  std::optional<diag::Diagnostic> last_error_;
};

}  // namespace hefesto::platform
