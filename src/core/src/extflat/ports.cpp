// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A cell's port entries in def_nodes iteration order (capability
// netlist-spice-options "Subcircuit header line"): topVisit walks the
// definition's node-name hash table and, for each name whose node is a
// port, reads the port index of the names from that one to the end of the
// node's list and of the node's canonical name. The netlisters number the
// ports from these (Magic ext2spice/ext2spice.c:1897-2060).
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13).

#include "ports.hpp"

#include "hier_name.hpp"

namespace hefesto::extflat::detail {

namespace {

// The entry a hash entry designates and its live node, without the path
// compression of NodeGraph::resolve/find (the model is shared and const).
std::uint32_t resolved(const NodeGraph& graph, std::uint32_t entry) {
  while (graph.name(entry).redirect != kNil) {
    entry = graph.name(entry).redirect;
  }
  return entry;
}

std::uint32_t live_node(const NodeGraph& graph, std::uint32_t entry) {
  std::uint32_t id = graph.name(entry).node;
  while (!graph.live(id)) {
    id = graph.node(id).parent;
  }
  return id;
}

PortName port_name(const NodeGraph& graph, std::uint32_t entry) {
  return PortName{name_to_string(graph.name(entry).key), graph.name(entry).port};
}

}  // namespace

std::vector<PortEntry> port_entries(const Def& def) {
  std::vector<PortEntry> out;
  def.name_order.for_each([&](const auto& hashed) {
    const auto it = def.names.find(hashed.key);
    if (it == def.names.end()) {
      return;
    }
    const std::uint32_t entry = resolved(def.graph, it->second);
    const std::uint32_t node = live_node(def.graph, entry);
    const GraphNode& n = def.graph.node(node);
    if ((n.flags & kFlagPort) == 0 || n.head == kNil) {
      return;
    }
    PortEntry port;
    for (std::uint32_t e = entry; e != kNil; e = def.graph.name(e).next) {
      port.chain.push_back(port_name(def.graph, e));
    }
    port.canonical = port_name(def.graph, n.head);
    out.push_back(std::move(port));
  });
  return out;
}

}  // namespace hefesto::extflat::detail
