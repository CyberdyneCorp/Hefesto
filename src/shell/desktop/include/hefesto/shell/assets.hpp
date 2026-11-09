// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Locating the desktop app's data files: the bundled fonts (Inter,
// JetBrains Mono, OFL-1.1) and icons under assets/ (capability
// hefesto-design-system "Typography", "Icons"; hefesto-licensing).
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace hefesto::shell {

/// The folders searched for assets, in order: `override_dir` (when not
/// empty), `<exe dir>/assets` (build tree and Linux/portable layout),
/// `<exe dir>/../Resources/assets` (macOS bundle),
/// `<exe dir>/../share/hefesto/assets` (installed), and `source_dir`
/// (a development fallback compiled in; may be empty).
[[nodiscard]] std::vector<std::filesystem::path> asset_search_dirs(const std::filesystem::path& override_dir,
                                                                   const std::filesystem::path& exe_dir,
                                                                   const std::filesystem::path& source_dir);

/// The first search dir that holds the bundled UI font
/// (fonts/Inter-Regular.ttf); nullopt when none does.
[[nodiscard]] std::optional<std::filesystem::path> find_asset_dir(const std::vector<std::filesystem::path>& dirs);

}  // namespace hefesto::shell
