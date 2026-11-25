// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The selected node's connectivity, shared by node_name and
// selected_node_predicate (node_name.cpp). Internal to sim/probe.
//
// Owner: SIMPROBE.

#pragma once

#include <string>
#include <vector>

#include "hefesto/connect/search.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/probe/probe.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::sim {

/// A cell instance reached by a use path, with its transform to the root.
struct PlacedCell {
  const db::CellDef* def = nullptr;
  db::Transform to_root;
};

/// The paint of the selected node in every cell it reaches, in root
/// coordinates with the instance path of each tile.
struct SelectedNet {
  std::vector<connect::NetTile> tiles;
};

/// Follows `use_path` (array elements as db::array_element_name prints
/// them) from `root`; SIM-PROBE-003 when an element is not found.
[[nodiscard]] diag::Outcome<PlacedCell> place(const db::CellDef& root, const std::vector<std::string>& use_path);

/// The hierarchical search from the paint at the selection's point (the
/// first paint plane with a non-space tile there).
[[nodiscard]] diag::Outcome<SelectedNet> find_selected_net(const LayoutSelection& selection);

}  // namespace hefesto::sim
