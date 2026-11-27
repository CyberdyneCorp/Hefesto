// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Nodes and their name lists, shared by the per-cell model the reader
// builds and the flat netlist (Magic's EFNode / EFNodeName and efNodeMerge;
// capability ext-flattening-hierarchy "Node merge semantics").
//
// Magic keeps a circular node list with new nodes at its head and unlinks a
// node merged away, so a list walk visits the live nodes in reverse
// creation order: NodeGraph keeps the nodes in creation order and marks the
// merged ones. A merged node points to the node it went into (union-find),
// so every name of it resolves to the kept node without Magic's walk over
// the name list, and name lists are spliced in O(1); chains of merges stay
// near linear on hostile input (design D23 M6-S8). Device terminals that
// hold a node id reach the kept node through find() the same way, which is
// efBuildEquiv's terminal redirection.
//
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/geometry.hpp"

namespace hefesto::extflat::detail {

inline constexpr std::uint32_t kNil = std::numeric_limits<std::uint32_t>::max();

/// efnode_flags (extflat/EFtypes.h) that the reader uses.
enum NodeFlag : std::uint32_t {
  kFlagPort = 1U << 0U,         // EF_PORT
  kFlagTopPort = 1U << 1U,      // EF_TOP_PORT
  kFlagSubsPort = 1U << 2U,     // EF_SUBS_PORT
  kFlagSubsNode = 1U << 3U,     // EF_SUBS_NODE
  kFlagGlobSubs = 1U << 4U,     // EF_GLOB_SUBS_NODE
  kFlagDevTerm = 1U << 5U,      // EF_DEVTERM
  kFlagKilled = 1U << 6U,       // EF_KILLED
};

/// One saved location (LinkedRect of efnode_disjoint): the rectangle's
/// lower-left corner and the layer index.
struct SavedLocation {
  db::Point ll;
  std::uint32_t layer = 0;
};

/// EFPerimArea: the area and perimeter of one resistance class.
struct PerimArea {
  std::int32_t area = 0;
  std::int32_t perim = 0;
};

/// EFAttr: one node attribute (its text, layer index and rectangle).
struct NodeAttr {
  std::string text;
  std::uint32_t layer = 0;
  db::Rect rect;
};

/// Adds `add` to `sum` class by class (32-bit wraparound as Magic's int
/// adds, without the undefined behaviour), growing `sum` when needed.
void add_perim_area(std::vector<PerimArea>& sum, const std::vector<PerimArea>& add);

struct GraphNode {
  std::uint32_t flags = 0;
  std::uint32_t layer = 0;  // index into the layer-name table; 0 is "space"
  db::Point loc;            // efnode_loc's lower-left corner
  std::vector<SavedLocation> disjoint;
  std::uint32_t head = kNil;  // first name (the canonical one)
  std::uint32_t tail = kNil;
  std::uint64_t num = 1;      // efnode_num
  std::uint32_t parent = kNil;  // the node this one was merged into
  float cap = 0.0F;             // efnode_cap (aF; EFCapValue is float)
  std::vector<PerimArea> pa;    // efnode_pa, one per resistance class
  std::vector<NodeAttr> attrs;  // efnode_attrs in list order (newest first)
};

struct NameEntry {
  std::string key;         // hierarchical name (hier_name.hpp)
  std::uint32_t node = kNil;  // node at creation; NodeGraph::find resolves merges
  std::uint32_t prev = kNil;
  std::uint32_t next = kNil;
  std::int32_t port = -1;     // efnn_port
  std::uint32_t redirect = kNil;  // efBuildEquiv points the hash entries of a merged-away name here
  /// The name of a shorted-port device's own terminal-0 node (Q-NETLIST-31
  /// and Q-NETLIST-60, magic-compat OFF): printed, but kept out of every
  /// table a record's name is looked up in.
  bool anonymous = false;
};

class NodeGraph {
 public:
  [[nodiscard]] std::uint32_t add_node(GraphNode node);
  /// A name of `node`, not yet in its list.
  [[nodiscard]] std::uint32_t add_name(std::string key, std::uint32_t node);

  void link_head(std::uint32_t node, std::uint32_t name);
  void link_after_head(std::uint32_t node, std::uint32_t name);
  void unlink(std::uint32_t node, std::uint32_t name);

  /// The live node `node` was merged into (itself when live).
  [[nodiscard]] std::uint32_t find(std::uint32_t node);
  [[nodiscard]] bool live(std::uint32_t node) const { return nodes_[node].parent == kNil; }
  /// The entry a hash entry holding `name` designates (efBuildEquiv's
  /// redirection).
  [[nodiscard]] std::uint32_t resolve(std::uint32_t name);
  /// The live node of the entry a hash entry holding `name` designates.
  [[nodiscard]] std::uint32_t node_of(std::uint32_t name) { return find(names_[resolve(name)].node); }

  /// efNodeMerge(node1, node2): returns the kept node.
  std::uint32_t merge(std::uint32_t node1, std::uint32_t node2);

  /// efNodeMerge's attribute splice keeps only the last attribute of the
  /// removed node, in front of the kept node's list (the others are lost):
  /// true copies that (magic-compat ON), false keeps them all.
  void set_compat_attrs(bool compat) noexcept { compat_attrs_ = compat; }

  [[nodiscard]] GraphNode& node(std::uint32_t id) { return nodes_[id]; }
  [[nodiscard]] const GraphNode& node(std::uint32_t id) const { return nodes_[id]; }
  [[nodiscard]] NameEntry& name(std::uint32_t id) { return names_[id]; }
  [[nodiscard]] const NameEntry& name(std::uint32_t id) const { return names_[id]; }
  [[nodiscard]] std::uint32_t node_count() const noexcept { return static_cast<std::uint32_t>(nodes_.size()); }
  [[nodiscard]] std::size_t name_count() const noexcept { return names_.size(); }

  /// Calls `visit(name_id)` for each name of `node` in list order.
  template <typename F>
  void for_each_name(std::uint32_t node, F&& visit) const {
    for (std::uint32_t n = nodes_[node].head; n != kNil; n = names_[n].next) {
      visit(n);
    }
  }

 private:
  bool keep_removed_name(const GraphNode& keep, const GraphNode& removing) const;
  void splice_names(std::uint32_t keep, std::uint32_t removing, bool removing_first);
  void merge_attrs(GraphNode& keep, GraphNode& removing) const;

  std::vector<GraphNode> nodes_;
  std::vector<NameEntry> names_;
  bool compat_attrs_ = true;
};

}  // namespace hefesto::extflat::detail
