// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The hierarchy path of a displayed schematic (capability schematic-editing
// "Hierarchy path"): `.` for the top, then one element per descent, e.g.
// `.x1.xinv[2].`. Used by `@path`, highlight propagation and simulation
// vector names.
//
// Owner: EDIT.

#pragma once

#include <string>
#include <vector>

namespace hefesto::schem {

struct HierPath {
  /// The chosen element of each instance name descended through, top first;
  /// empty at the top.
  std::vector<std::string> elements;

  friend bool operator==(const HierPath&, const HierPath&) = default;
};

/// The path text: `.` then each element followed by `.` (`.x1.xinv[2].`).
[[nodiscard]] std::string path_text(const HierPath& path);

/// The value of `@path`: path_text() without the leading `.` (`x1.x2.`;
/// empty at the top).
[[nodiscard]] std::string at_path(const HierPath& path);

}  // namespace hefesto::schem
