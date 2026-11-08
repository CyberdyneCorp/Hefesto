// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform/storage.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform/process_runner.hpp"

namespace hefesto::platform {

namespace codes = diag::codes;

namespace {

constexpr std::string_view kAutosaveExtension = ".autosave";

bool is_safe_name_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
         c == '-';
}

bool is_safe_name(std::string_view name) {
  return !name.empty() && name != "." && name != ".." && std::all_of(name.begin(), name.end(), is_safe_name_char);
}

}  // namespace

// --- UnsupportedProcessRunner ---------------------------------------------

diag::Outcome<ProcessOutput> UnsupportedProcessRunner::run(const ProcessRequest& request) {
  return diag::make(codes::kPlatformUnsupported,
                    "external process execution is not available on this platform (requested '" +
                        request.program + "')");
}

// --- DirectoryPdkStore ----------------------------------------------------

DirectoryPdkStore::DirectoryPdkStore(FileSystem& fs, std::vector<Path> roots) : fs_(fs), roots_(std::move(roots)) {}

diag::Outcome<Path> DirectoryPdkStore::find_technology(std::string_view name) {
  const std::string file = std::string{name} + ".tech";
  if (is_safe_name(name)) {
    for (const auto& root : roots_) {
      for (const Path& candidate : {root / file, root / std::string{name} / "libs.tech" / "magic" / file}) {
        if (fs_.exists(candidate)) {
          return candidate;
        }
      }
    }
  }
  return diag::make(codes::kPlatformTechNotFound,
                    "technology '" + std::string{name} + "' not found in " + std::to_string(roots_.size()) +
                        " technology root(s)");
}

// --- FileAutosaveService --------------------------------------------------

FileAutosaveService::FileAutosaveService(FileSystem& fs, Path root) : fs_(fs), root_(std::move(root)) {}

diag::Outcome<Path> FileAutosaveService::snapshot_path(std::string_view name) const {
  if (!is_safe_name(name)) {
    return diag::make(codes::kPlatformWrite, "invalid autosave snapshot name '" + std::string{name} + "'");
  }
  return root_ / (std::string{name} + std::string{kAutosaveExtension});
}

diag::Status FileAutosaveService::write_snapshot(std::string_view name, std::string_view data) {
  auto path = snapshot_path(name);
  if (!path) {
    return path.error();
  }
  if (auto made = fs_.create_directories(root_); !made) {
    return made.error();
  }
  return fs_.write_atomic(path.value(), data);
}

diag::Outcome<std::string> FileAutosaveService::read_snapshot(std::string_view name) {
  auto path = snapshot_path(name);
  if (!path) {
    return path.error();
  }
  return fs_.read(path.value());
}

std::vector<std::string> FileAutosaveService::list_snapshots() {
  std::vector<std::string> names;
  auto entries = fs_.list(root_);
  if (!entries) {
    return names;  // no autosave directory yet: no snapshots
  }
  for (const auto& entry : entries.value()) {
    if (entry.extension() == kAutosaveExtension) {
      names.push_back(entry.stem().string());
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

diag::Status FileAutosaveService::discard_snapshot(std::string_view name) {
  auto path = snapshot_path(name);
  if (!path) {
    return path.error();
  }
  return fs_.remove(path.value());
}

}  // namespace hefesto::platform
