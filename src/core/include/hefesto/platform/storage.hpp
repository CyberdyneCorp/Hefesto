// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// PDK/technology storage and autosave/recovery (capability
// hefesto-architecture, requirement "Platform services abstraction").

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/clock.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::platform {

class PdkStore {
 public:
  PdkStore() = default;
  PdkStore(const PdkStore&) = delete;
  PdkStore& operator=(const PdkStore&) = delete;
  virtual ~PdkStore() = default;

  /// Roots searched for technologies, in search order.
  [[nodiscard]] virtual std::vector<Path> roots() const = 0;
  /// Path of the `.tech` file for `name`; PLATFORM-PDK-001 when not found.
  [[nodiscard]] virtual diag::Outcome<Path> find_technology(std::string_view name) = 0;
};

/// Searches each root, in order, for `<root>/<name>.tech` and then
/// `<root>/<name>/libs.tech/magic/<name>.tech` (open_pdks layout).
class DirectoryPdkStore final : public PdkStore {
 public:
  DirectoryPdkStore(FileSystem& fs, std::vector<Path> roots);

  [[nodiscard]] std::vector<Path> roots() const override { return roots_; }
  [[nodiscard]] diag::Outcome<Path> find_technology(std::string_view name) override;

 private:
  FileSystem& fs_;
  std::vector<Path> roots_;
};

class AutosaveService {
 public:
  AutosaveService() = default;
  AutosaveService(const AutosaveService&) = delete;
  AutosaveService& operator=(const AutosaveService&) = delete;
  virtual ~AutosaveService() = default;

  /// Stores (or replaces) the recovery snapshot for `name`.
  [[nodiscard]] virtual diag::Status write_snapshot(std::string_view name, std::string_view data) = 0;
  [[nodiscard]] virtual diag::Outcome<std::string> read_snapshot(std::string_view name) = 0;
  /// Names of the stored snapshots, sorted.
  [[nodiscard]] virtual std::vector<std::string> list_snapshots() = 0;
  [[nodiscard]] virtual diag::Status discard_snapshot(std::string_view name) = 0;
};

/// Snapshots are files `<root>/<name>.autosave`, written atomically.
/// Names are restricted to [A-Za-z0-9._-] so they cannot escape the root.
class FileAutosaveService final : public AutosaveService {
 public:
  FileAutosaveService(FileSystem& fs, Path root);

  diag::Status write_snapshot(std::string_view name, std::string_view data) override;
  diag::Outcome<std::string> read_snapshot(std::string_view name) override;
  std::vector<std::string> list_snapshots() override;
  diag::Status discard_snapshot(std::string_view name) override;

 private:
  [[nodiscard]] diag::Outcome<Path> snapshot_path(std::string_view name) const;

  FileSystem& fs_;
  Path root_;
};

}  // namespace hefesto::platform
