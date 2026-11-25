// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Layout node naming and probes (openspec/changes/m9-lvs-simulation
// design.md D4, D13, "Area contracts" SIMPROBE; Q-SIM-24, approved
// 2026-11-22; capabilities irsim-interface "Hierarchical node names",
// simulation-ui-and-crossprobe "Probing a layout net", "Probe names without
// length limits").
//
// node_name brings the irsim-interface naming rule forward from M12 with no
// length limit in either magic-compat mode; the `getnode` command itself is
// not implemented. resolve_probe looks the name up in the net-name map's
// `layout_name` index and falls back to connectivity: the entry whose point
// lies on the selected node.
//
// Owner: SIMPROBE.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/job.hpp"
#include "hefesto/sim/probe/net_name_map.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::sim {

/// Selected paint in a layout window: the window's root cell and a point on
/// the node.
struct LayoutSelection {
  const db::CellDef* root = nullptr;
  LayoutPoint point;
};

/// True when `point` lies on the selected node, following the node's
/// connectivity through the extracted hierarchy.
using OnSelectedNode = std::function<bool(const LayoutPoint& point)>;

/// The hierarchical node name of the selection, relative to its root cell.
[[nodiscard]] diag::Outcome<std::string> node_name(const LayoutSelection& selection);

/// The map entry of the selected node: by layout_name first, then the entry
/// whose point satisfies `on_selected_node`; an error ("Net <name> is not in
/// the netlist of this run") when neither finds one.
[[nodiscard]] diag::Outcome<NetNameEntry> resolve_probe(const NetNameMap& map, const LayoutSelection& selection,
                                                        const OnSelectedNode& on_selected_node);

using TraceId = std::uint32_t;

struct ProbeTarget {
  enum class Kind : std::uint8_t { layout_net, schematic_net, device_current };
  Kind kind = Kind::layout_net;
  LayoutSelection layout;  // layout_net
  std::string name;        // schematic_net: the net; device_current: the device
};

/// Adds the trace of `target` in `job`'s plot.
[[nodiscard]] diag::Outcome<TraceId> probe(const Job& job, const ProbeTarget& target);

}  // namespace hefesto::sim
