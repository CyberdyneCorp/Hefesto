// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop's open paths (see hefesto/shell/open_files.hpp).
// Owner: SHELL (openspec/changes/m6-lef-def-oasis design D16).

#include "hefesto/shell/open_files.hpp"

#include "hefesto/app/open_file.hpp"

namespace hefesto::shell {

OpenPlan plan_open(platform::FileSystem& fs, std::span<const std::string> paths) {
  OpenPlan plan;
  for (const std::string& path : paths) {
    auto open = app::open_command(fs, platform::Path{path});
    if (open.ok()) {
      plan.lines.push_back(open.value().line());
    } else {
      plan.problems.push_back(open.error());
    }
  }
  return plan;
}

}  // namespace hefesto::shell
