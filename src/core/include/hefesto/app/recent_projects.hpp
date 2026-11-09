// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The recent-projects list (hefesto-pdk-manager "Uninstall and cleanup":
// "the projects known to pin it (from the recent-projects list)"). A JSON
// file `{"projects": ["/abs/folder", ...]}`, most recent first, at most
// kMaxRecentProjects entries. Front ends choose where it lives and pass it
// to run_startup (StartupEnvironment::recent_projects), which records the
// opened project folder and publishes the file in the session setting
// kRecentProjectsFile so that the `pdk` command finds it.
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::app {

/// Setting naming the recent-projects file (empty or unset: no list).
inline constexpr std::string_view kRecentProjectsFile = "recent-projects-file";
inline constexpr std::size_t kMaxRecentProjects = 20;

/// The folders of the list, most recent first. A missing file is an empty
/// list; a malformed one fails with APP-RECENT-001.
[[nodiscard]] diag::Outcome<std::vector<platform::Path>> load_recent_projects(platform::FileSystem& fs,
                                                                             const platform::Path& file);

/// Moves (or adds) `folder` to the front of the list and saves it
/// atomically. A malformed list is replaced.
[[nodiscard]] diag::Status note_recent_project(platform::FileSystem& fs, const platform::Path& file,
                                               const platform::Path& folder);

/// The folders whose hefesto.project pins `family` at `version`; folders
/// without a readable project file are skipped.
[[nodiscard]] std::vector<platform::Path> projects_pinning(platform::FileSystem& fs,
                                                           const std::vector<platform::Path>& folders,
                                                           std::string_view family, std::string_view version);

}  // namespace hefesto::app
