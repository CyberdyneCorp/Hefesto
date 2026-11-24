// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::NetNameMap: the simulation net-name map of a layout DUT
// (openspec/changes/m9-lvs-simulation design.md D4 "Two namings, one node",
// D13; Q-SIM-24, approved 2026-11-22; capability
// simulation-netlist-preparation "Net name map").
//
// One entry per SPICE node of the DUT netlist with four fields: the
// `layoutName` computed by the irsim-interface naming rule at the entry's
// point, the netlist's `spiceName`, the ngspice `simName`, and the point.
// The netlister's name and the hierarchical node name can differ (a
// case-collision rename, a transistor-terminal name); they are reconciled by
// connectivity, never by comparing strings: SIMDECK fills `layout_name` by
// calling sim::node_name at `point`, and a probe that finds no entry by
// `layout_name` takes the entry whose point lies on the selected node
// (sim::resolve_probe).
//
// The map is immutable once built, ordered by `spice_name` for deterministic
// iteration, with a second sorted index over `layout_name`.
//
// Owner: SIMPROBE (SIMDECK builds the map).

#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::sim {

/// A point on a node: the use path from the DUT cell and a point in the
/// coordinates of the innermost cell.
struct LayoutPoint {
  std::vector<std::string> use_path;
  db::Point point;

  friend bool operator==(const LayoutPoint&, const LayoutPoint&) = default;
};

struct NetNameEntry {
  std::string layout_name;  // `layoutName` (irsim-interface "Hierarchical node names")
  std::string spice_name;   // `spiceName`, as written in the netlist
  std::string sim_name;     // `simName`, e.g. "xdut.xinv_0.mid"
  LayoutPoint point;        // `point`, from the netlister's name map
  /// The rnode name of an `extresist` network node in mode `rc` (the entry
  /// then carries the parent net's `layout_name`); empty for a net.
  std::string rnode;
};

class NetNameMap {
 public:
  NetNameMap() = default;
  explicit NetNameMap(std::vector<NetNameEntry> entries);

  /// The entries sorted by (spice_name, rnode).
  [[nodiscard]] std::span<const NetNameEntry> entries() const noexcept { return entries_; }
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

  /// The net entry (empty rnode first) whose layout_name is `name`; nullptr
  /// when there is none.
  [[nodiscard]] const NetNameEntry* find_layout_name(std::string_view name) const;
  /// The net entry (empty rnode first) whose spice_name is `name`; nullptr
  /// when there is none.
  [[nodiscard]] const NetNameEntry* find_spice_name(std::string_view name) const;

 private:
  std::vector<NetNameEntry> entries_;
  std::vector<std::size_t> by_layout_name_;  // indices into entries_, sorted by (layout_name, rnode)
};

}  // namespace hefesto::sim
