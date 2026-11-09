// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/shell/assets.hpp"

#include <system_error>

#include "hefesto/shell/design_tokens.hpp"

namespace hefesto::shell {

std::vector<std::filesystem::path> asset_search_dirs(const std::filesystem::path& override_dir,
                                                     const std::filesystem::path& exe_dir,
                                                     const std::filesystem::path& source_dir) {
  std::vector<std::filesystem::path> dirs;
  if (!override_dir.empty()) {
    dirs.push_back(override_dir);
  }
  if (!exe_dir.empty()) {
    dirs.push_back(exe_dir / "assets");
    dirs.push_back(exe_dir / ".." / "Resources" / "assets");
    dirs.push_back(exe_dir / ".." / "share" / "hefesto" / "assets");
  }
  if (!source_dir.empty()) {
    dirs.push_back(source_dir);
  }
  return dirs;
}

std::optional<std::filesystem::path> find_asset_dir(const std::vector<std::filesystem::path>& dirs) {
  for (const std::filesystem::path& dir : dirs) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(dir / kUiFontFile, ec)) {
      return dir;
    }
  }
  return std::nullopt;
}

}  // namespace hefesto::shell
