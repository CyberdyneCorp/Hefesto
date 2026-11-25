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
// As landed (M9 R1, task 14.2 naming portion; additive):
//   - node_name follows the selected node through the hierarchy with
//     connect::search_hierarchical, names the node's region in each cell it
//     reaches with the extractor's node regions and labels (extract/node:
//     RegionFinder, attach_labels, node_name), prefixes the instance path,
//     keeps the best candidate by prefer_node_name (irsim-interface "Node
//     name preference order") and strips a global name's path and `!`.
//     Names are std::string throughout: Magic's 256-byte buffers
//     (SimGetNodeName, sim/SimExtract.c:734-767) overflow on long names, a
//     crash fixed in both magic-compat modes. The positional `@=` names of
//     "Fast node names" (getnode fast) are not produced; an `@=` candidate
//     (for instance a map's layoutName) still wins by preference rule 1.
//   - LayoutSelection::naming carries the technology and the extraction
//     style the naming needs (nullptr: node_name fails with SIM-PROBE-002).
//   - selected_node_predicate builds the OnSelectedNode of a selection from
//     the same hierarchical search.
//
// Owner: SIMPROBE.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/job.hpp"
#include "hefesto/sim/probe/net_name_map.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {
class ExtStyle;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::sim {

/// Added in M9 R1: what node_name needs besides the layout.
struct NodeNaming {
  const tech::Technology* technology = nullptr;
  const extract::ExtStyle* style = nullptr;  // the current extraction style
  /// `getnode globals` (default on): a best name ending in `!` loses its
  /// path and the `!`.
  bool globals = true;
  /// The magic-compat setting, passed to the connectivity search and the
  /// extractor's label order.
  bool magic_compat = true;
};

/// Selected paint in a layout window: the window's root cell and a point on
/// the node.
struct LayoutSelection {
  const db::CellDef* root = nullptr;
  LayoutPoint point;
  NodeNaming naming;  // added in M9 R1
};

/// True when `point` lies on the selected node, following the node's
/// connectivity through the extracted hierarchy.
using OnSelectedNode = std::function<bool(const LayoutPoint& point)>;

/// The hierarchical node name of the selection, relative to its root cell.
/// Fails with SIM-PROBE-002 without a technology or style, SIM-PROBE-003
/// when the use path does not exist and SIM-PROBE-004 when no paint lies
/// at the point.
[[nodiscard]] diag::Outcome<std::string> node_name(const LayoutSelection& selection);

/// Added in M9 R1: irsim-interface "Node name preference order"
/// (efPreferredName): true when `a` is preferred to `b`.
[[nodiscard]] bool prefer_node_name(std::string_view a, std::string_view b, bool globals) noexcept;

/// Added in M9 R1: the predicate of resolve_probe for `selection`: true
/// when a point (its use path from the selection's root and a point in the
/// innermost cell) lies on paint of the selected node. Fails as node_name.
[[nodiscard]] diag::Outcome<OnSelectedNode> selected_node_predicate(const LayoutSelection& selection);

/// The map entry of the selected node: by layout_name first, then the entry
/// whose point satisfies `on_selected_node` (net entries before `extresist`
/// rnode entries, in map order); an error SIM-PROBE-001 ("Net <name> is not
/// in the netlist of this run") when neither finds one. When node_name
/// fails, only the predicate is tried and its error is returned.
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
