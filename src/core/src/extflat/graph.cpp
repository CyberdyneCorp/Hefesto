// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Nodes and name lists (see graph.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "graph.hpp"

#include <iterator>

#include "hier_name.hpp"

namespace hefesto::extflat::detail {

std::uint32_t NodeGraph::add_node(GraphNode node) {
  nodes_.push_back(std::move(node));
  return static_cast<std::uint32_t>(nodes_.size() - 1);
}

std::uint32_t NodeGraph::add_name(std::string key, std::uint32_t node) {
  NameEntry entry;
  entry.key = std::move(key);
  entry.node = node;
  names_.push_back(std::move(entry));
  return static_cast<std::uint32_t>(names_.size() - 1);
}

void NodeGraph::link_head(std::uint32_t node, std::uint32_t name) {
  GraphNode& n = nodes_[node];
  NameEntry& e = names_[name];
  e.prev = kNil;
  e.next = n.head;
  if (n.head != kNil) {
    names_[n.head].prev = name;
  } else {
    n.tail = name;
  }
  n.head = name;
}

void NodeGraph::link_after_head(std::uint32_t node, std::uint32_t name) {
  GraphNode& n = nodes_[node];
  if (n.head == kNil) {
    link_head(node, name);
    return;
  }
  NameEntry& head = names_[n.head];
  NameEntry& e = names_[name];
  e.prev = n.head;
  e.next = head.next;
  if (head.next != kNil) {
    names_[head.next].prev = name;
  } else {
    n.tail = name;
  }
  head.next = name;
}

void NodeGraph::unlink(std::uint32_t node, std::uint32_t name) {
  GraphNode& n = nodes_[node];
  NameEntry& e = names_[name];
  if (e.prev != kNil) {
    names_[e.prev].next = e.next;
  } else {
    n.head = e.next;
  }
  if (e.next != kNil) {
    names_[e.next].prev = e.prev;
  } else {
    n.tail = e.prev;
  }
  e.prev = kNil;
  e.next = kNil;
}

std::uint32_t NodeGraph::find(std::uint32_t node) {
  std::uint32_t root = node;
  while (nodes_[root].parent != kNil) {
    root = nodes_[root].parent;
  }
  while (nodes_[node].parent != kNil) {
    const std::uint32_t next = nodes_[node].parent;
    nodes_[node].parent = root;
    node = next;
  }
  return root;
}

std::uint32_t NodeGraph::resolve(std::uint32_t name) {
  std::uint32_t target = name;
  while (names_[target].redirect != kNil) {
    target = names_[target].redirect;
  }
  while (names_[name].redirect != kNil) {
    const std::uint32_t next = names_[name].redirect;
    names_[name].redirect = target;
    name = next;
  }
  return target;
}

// efNodeMerge's choice of canonical name: no name on the kept node, then
// the top-level port, then EFHNBest.
bool NodeGraph::keep_removed_name(const GraphNode& keep, const GraphNode& removing) const {
  if (keep.head == kNil) {
    return true;
  }
  const bool top_keep = (keep.flags & kFlagTopPort) != 0;
  const bool top_removing = (removing.flags & kFlagTopPort) != 0;
  if (top_keep != top_removing) {
    return top_removing;
  }
  return prefer_name(names_[removing.head].key, names_[keep.head].key);
}

// removing_first: removing's names, then keep's; otherwise keep's first
// name, removing's names, keep's other names.
void NodeGraph::splice_names(std::uint32_t keep, std::uint32_t removing, bool removing_first) {
  GraphNode& k = nodes_[keep];
  GraphNode& r = nodes_[removing];
  if (removing_first || k.head == kNil) {
    names_[r.tail].next = k.head;
    if (k.head != kNil) {
      names_[k.head].prev = r.tail;
    } else {
      k.tail = r.tail;
    }
    k.head = r.head;
  } else {
    NameEntry& head = names_[k.head];
    names_[r.tail].next = head.next;
    if (head.next != kNil) {
      names_[head.next].prev = r.tail;
    } else {
      k.tail = r.tail;
    }
    head.next = r.head;
    names_[r.head].prev = k.head;
  }
  r.head = kNil;
  r.tail = kNil;
}

void add_perim_area(std::vector<PerimArea>& sum, const std::vector<PerimArea>& add) {
  if (sum.size() < add.size()) {
    sum.resize(add.size());
  }
  for (std::size_t n = 0; n < add.size(); ++n) {
    sum[n].area = static_cast<std::int32_t>(static_cast<std::uint32_t>(sum[n].area) +
                                            static_cast<std::uint32_t>(add[n].area));
    sum[n].perim = static_cast<std::int32_t>(static_cast<std::uint32_t>(sum[n].perim) +
                                             static_cast<std::uint32_t>(add[n].perim));
  }
}

// efNodeMerge's attribute splice: Magic walks to the last attribute of
// `removing`, links the kept list after it and makes that last attribute
// the kept list's head, so the removed node's other attributes are lost.
void NodeGraph::merge_attrs(GraphNode& keep, GraphNode& removing) const {
  if (removing.attrs.empty()) {
    return;
  }
  if (compat_attrs_) {
    keep.attrs.insert(keep.attrs.begin(), std::move(removing.attrs.back()));
  } else {
    keep.attrs.insert(keep.attrs.begin(), std::make_move_iterator(removing.attrs.begin()),
                      std::make_move_iterator(removing.attrs.end()));
  }
  removing.attrs.clear();
}

std::uint32_t NodeGraph::merge(std::uint32_t node1, std::uint32_t node2) {
  if (node1 == node2) {
    return node1;
  }
  const bool first_kept = nodes_[node1].num >= nodes_[node2].num;
  const std::uint32_t keep = first_kept ? node1 : node2;
  const std::uint32_t removing = first_kept ? node2 : node1;
  GraphNode& k = nodes_[keep];
  GraphNode& r = nodes_[removing];
  k.cap += r.cap;
  add_perim_area(k.pa, r.pa);
  if (r.head != kNil) {
    const bool swap = keep_removed_name(k, r);
    if (swap && r.layer > 0) {
      k.loc = r.loc;
      k.layer = r.layer;
    }
    splice_names(keep, removing, swap);
  }
  k.num += r.num;
  merge_attrs(k, r);
  if ((r.flags & kFlagDevTerm) == 0) {
    k.flags &= ~kFlagDevTerm;
  }
  k.flags |= r.flags & (kFlagPort | kFlagTopPort | kFlagSubsPort | kFlagSubsNode | kFlagGlobSubs);
  k.disjoint.insert(k.disjoint.end(), r.disjoint.begin(), r.disjoint.end());
  r.disjoint.clear();
  r.flags = 0;
  r.parent = keep;
  return keep;
}

}  // namespace hefesto::extflat::detail
