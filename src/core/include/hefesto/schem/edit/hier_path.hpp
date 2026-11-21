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

}  // namespace hefesto::schem
