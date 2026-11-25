// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Probe resolution (design D4 "Two namings, one node"; simulation-ui-and-
// crossprobe "Probing a layout net", step 3): the net-name map's
// `layout_name` index first, then the entry whose point lies on the selected
// node. Names are never compared any other way. node_name is in
// node_name.cpp; probe() (the trace) is still a stub until the rest of task
// 14.2 lands with SIMDECK's layout sources (13.4).
//
// Owner: SIMPROBE.

#include <string>

#include "hefesto/sim/not_implemented.hpp"
#include "hefesto/sim/probe/probe.hpp"

namespace hefesto::sim {

namespace {

/// The first entry whose point satisfies `on_node`: net entries first, then
/// the rnode entries of `extresist` networks, each in map order.
const NetNameEntry* by_connectivity(const NetNameMap& map, const OnSelectedNode& on_node) {
  if (!on_node) {
    return nullptr;
  }
  for (const bool rnodes : {false, true}) {
    for (const NetNameEntry& entry : map.entries()) {
      if (entry.rnode.empty() != rnodes && on_node(entry.point)) {
        return &entry;
      }
    }
  }
  return nullptr;
}

}  // namespace

diag::Outcome<NetNameEntry> resolve_probe(const NetNameMap& map, const LayoutSelection& selection,
                                          const OnSelectedNode& on_selected_node) {
  const diag::Outcome<std::string> name = node_name(selection);
  if (name.ok()) {
    if (const NetNameEntry* entry = map.find_layout_name(name.value())) {
      return *entry;
    }
  }
  if (const NetNameEntry* entry = by_connectivity(map, on_selected_node)) {
    return *entry;
  }
  if (!name.ok()) {
    return name.error();
  }
  return diag::make("SIM-PROBE-001", "Net " + name.value() + " is not in the netlist of this run");
}

diag::Outcome<TraceId> probe(const Job& /*job*/, const ProbeTarget& /*target*/) {
  return not_implemented("sim::probe");
}

}  // namespace hefesto::sim
