// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's open paths (openspec/changes/archive/2026-11-22-m6-lef-def-oasis design D16,
// tasks.md 14.1; capability oasis-command "Format recognition on open"):
// the File > Open picker and files dropped on the window both go through
// the generic open path (app::open_command), which reads each file's first
// bytes, after gzip, and gives the command that opens it whatever its
// extension: `oasis read` for OASIS, `gds read` for GDS, `load` for a
// Magic cell. The main window runs each line as a typed command, so it is
// echoed and reaches the M5 progress and cancel hosts. Toolkit-free.
// Owner: SHELL.

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::shell {

/// What opening some files runs.
struct OpenPlan {
  std::vector<std::string> lines;          // one command line per file that opens, in order
  std::vector<diag::Diagnostic> problems;  // the files that do not (unknown format, unreadable)
};

/// The plan of opening `paths` on `fs`.
[[nodiscard]] OpenPlan plan_open(platform::FileSystem& fs, std::span<const std::string> paths);

/// The picker's filter: every file, since the content decides the format.
inline constexpr std::string_view kOpenFilterName = "Layout files (Magic, GDS, OASIS)";
inline constexpr std::string_view kOpenFilterPatterns = "*";

}  // namespace hefesto::shell
