// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFFlatBuild (EF_FLATNODES | EF_NOFLATSUBCKT) and EFVisitNodes (see
// flatten.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "flatten.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

#include "hier_name.hpp"

namespace hefesto::extflat::detail {

namespace {

// One instance of a def below the root (HierContext).
struct HierContext {
  std::uint32_t def = 0;
  std::string prefix;  // instance path (hier_name.hpp); empty for the root
  Transform trans;
};

// efFlatGlobHash: the leaf component's hash.
struct LeafHash {
  std::int32_t operator()(const std::string& leaf) const noexcept {
    return static_cast<std::int32_t>(component_hash(leaf));
  }
};
using GlobalTable = util::MagicHashTable<std::uint32_t, util::MagicClientKey<std::string, LeafHash>>;

constexpr std::size_t kGlobalTableSize = 1024;  // INITFLATSIZE
constexpr std::size_t kGlobalNamesShown = 10;

class Flattener {
 public:
  explicit Flattener(ReadState& st) : st_(st), on_stack_(st.defs.size(), 0) {}

  void flat_nodes(const HierContext& hc, std::size_t depth);
  void flat_kills(const HierContext& hc, std::size_t depth);
  void flat_glob();
  [[nodiscard]] std::vector<FlatNode> visit();

 private:
  template <typename F>
  void for_each_use(const HierContext& hc, std::size_t depth, F&& visit_child);
  template <typename F>
  void for_each_element(const HierContext& hc, const Use& use, F&& visit_child);
  bool count_instance();
  void add_nodes(const HierContext& hc);
  void add_node_names(const HierContext& hc, const Def& def, std::uint32_t local, std::uint32_t flat);
  void add_conns(const HierContext& hc);
  void expand_conn(const std::string& prefix, const Conn& conn);
  void add_one_conn(const std::string& prefix, const std::string& name1, const std::string& name2);
  [[nodiscard]] std::uint32_t lookup(const std::string& prefix, const std::string& name, std::string_view what);
  void report_split_global(std::string_view leaf, std::uint32_t glob, std::uint32_t flat);
  void report_names(std::uint32_t node);
  void name_globals(const GlobalTable& global);
  [[nodiscard]] FlatNode output(std::uint32_t node) const;

  ReadState& st_;
  NodeGraph flat_;
  std::unordered_map<std::string, std::uint32_t> table_;  // efNodeHashTable
  std::vector<char> on_stack_;
  std::uint64_t instances_ = 0;
  std::uint64_t connections_ = 0;
};

bool Flattener::count_instance() {
  if (++instances_ > st_.options.limits.max_instances) {
    st_.fail(codes::kLimit, "the flattened .ext hierarchy has more than " +
                                std::to_string(st_.options.limits.max_instances) + " instances");
    return false;
  }
  return true;
}

// The elements of an array use, x outer and y inner, each from its lower
// index (efHierSrUses).
template <typename F>
void Flattener::for_each_element(const HierContext& hc, const Use& use, F&& visit_child) {
  const bool x_up = use.xlo <= use.xhi;
  const bool y_up = use.ylo <= use.yhi;
  const std::int64_t xlo = x_up ? use.xlo : use.xhi;
  const std::int64_t xhi = x_up ? use.xhi : use.xlo;
  const std::int64_t xsep = x_up ? std::int64_t{use.xsep} : -std::int64_t{use.xsep};
  const std::int64_t ylo = y_up ? use.ylo : use.yhi;
  const std::int64_t yhi = y_up ? use.yhi : use.ylo;
  const std::int64_t ysep = y_up ? std::int64_t{use.ysep} : -std::int64_t{use.ysep};
  const Transform t = compose(use.trans, hc.trans);
  for (std::int64_t x = xlo; x <= xhi; ++x) {
    for (std::int64_t y = ylo; y <= yhi; ++y) {
      if (!count_instance()) {
        return;
      }
      const std::string component = use_component(use.id, use.xlo != use.xhi, use.ylo != use.yhi,
                                                  static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
      HierContext child{use.def, concat_names(hc.prefix, component),
                        translate_then(xsep * (x - use.xlo), ysep * (y - use.ylo), t)};
      visit_child(child);
      if (st_.fatal) {
        return;
      }
    }
  }
}

// efHierSrUses: the uses in use-table order. A use cycle and the depth cap
// stop the flattening (Magic recurses until the stack overflows).
template <typename F>
void Flattener::for_each_use(const HierContext& hc, std::size_t depth, F&& visit_child) {
  const Def& def = st_.defs.at(hc.def);
  on_stack_[hc.def] = 1;
  def.use_table.for_each([&](const auto& entry) {
    if (st_.fatal || !entry.value) {
      return;
    }
    const Use& use = def.uses[*entry.value];
    if (on_stack_[use.def] != 0) {
      st_.fail(codes::kUseCycle, "the .ext uses of cell " + def.name + " reach it again through " + use.id);
      return;
    }
    if (depth + 1 > st_.options.limits.max_depth) {
      st_.fail(codes::kLimit, "the .ext hierarchy is deeper than " + std::to_string(st_.options.limits.max_depth));
      return;
    }
    if (use.is_array()) {
      for_each_element(hc, use, visit_child);
      return;
    }
    if (count_instance()) {
      HierContext child{use.def, concat_names(hc.prefix, use.id), compose(use.trans, hc.trans)};
      visit_child(child);
    }
  });
  on_stack_[hc.def] = 0;
}

// efFlatNodesStdCell: children first, except below a subcircuit, then the
// def's own nodes and (outside subcircuits) its connections.
void Flattener::flat_nodes(const HierContext& hc, std::size_t depth) {
  const bool subcircuit = st_.defs.at(hc.def).subcircuit;
  if (!subcircuit) {
    for_each_use(hc, depth, [&](const HierContext& child) { flat_nodes(child, depth + 1); });
  }
  if (st_.fatal) {
    return;
  }
  add_nodes(hc);
  if (!subcircuit && !st_.fatal) {
    add_conns(hc);
  }
}

// efAddNodes with FLATNODE_STDCELL: the def's live nodes in list order
// (reverse creation); a subcircuit gives its ports only.
void Flattener::add_nodes(const HierContext& hc) {
  const Def& def = st_.defs.at(hc.def);
  for (std::uint32_t i = def.graph.node_count(); i-- > 0 && !st_.fatal;) {
    const GraphNode& local = def.graph.node(i);
    if (!def.graph.live(i) || (def.subcircuit && (local.flags & kFlagPort) == 0)) {
      continue;
    }
    if (local.head == kNil) {
      continue;  // every name went to another node (Magic dereferences a NULL name later)
    }
    GraphNode node;
    node.flags = local.flags;
    node.layer = local.layer;
    node.loc = transform_unit_rect(hc.trans, local.loc);
    node.disjoint.assign(local.disjoint.rbegin(), local.disjoint.rend());
    const std::uint32_t id = flat_.add_node(std::move(node));
    add_node_names(hc, def, i, id);
  }
}

// The names of one new flat node: the instance path joined with each local
// name (the bare name for an implicit substrate). A name already flat
// merges the two nodes.
void Flattener::add_node_names(const HierContext& hc, const Def& def, std::uint32_t local, std::uint32_t flat) {
  const bool bare = (def.graph.node(local).flags & kFlagDevTerm) != 0;
  std::uint32_t current = flat;
  def.graph.for_each_name(local, [&](std::uint32_t n) {
    if (st_.fatal) {
      return;
    }
    const std::string& local_key = def.graph.name(n).key;
    std::string key = bare ? local_key : concat_names(hc.prefix, local_key);
    if (const auto it = table_.find(key); it != table_.end()) {
      const std::uint32_t old = flat_.node_of(it->second);
      current = flat_.find(current);
      current = old != current ? flat_.merge(old, current) : old;
      return;
    }
    if (flat_.name_count() >= st_.options.limits.max_flat_names) {
      st_.fail(codes::kLimit, "the flattened .ext netlist has more than " +
                                  std::to_string(st_.options.limits.max_flat_names) + " names");
      return;
    }
    current = flat_.find(current);
    const std::uint32_t entry = flat_.add_name(key, current);
    table_.emplace(std::move(key), entry);
    flat_.link_after_head(current, entry);
  });
}

// efAddConns: the merge records, newest first.
void Flattener::add_conns(const HierContext& hc) {
  const Def& def = st_.defs.at(hc.def);
  for (auto it = def.conns.rbegin(); it != def.conns.rend() && !st_.fatal; ++it) {
    expand_conn(hc.prefix, *it);
  }
}

// efHierSrArray: one connection per subscript (pair).
void Flattener::expand_conn(const std::string& prefix, const Conn& conn) {
  const ConnName& c1 = conn.name1;
  const ConnName& c2 = conn.name2;
  if (c1.nsubs == 0) {
    add_one_conn(prefix, c1.before, c2.before);
    return;
  }
  const bool two = c1.nsubs == 2;
  const std::int64_t jlo = two ? c1.subs[1].lo : 0;
  const std::int64_t jhi = two ? c1.subs[1].hi : 0;
  for (std::int64_t i = c1.subs[0].lo; i <= c1.subs[0].hi; ++i) {
    for (std::int64_t j = jlo; j <= jhi; ++j) {
      if (++connections_ > st_.options.limits.max_connections) {
        st_.fail(codes::kLimit, "the .ext merge records expand to more than " +
                                    std::to_string(st_.options.limits.max_connections) + " connections");
        return;
      }
      const std::int64_t i2 = i - c1.subs[0].lo + c2.subs[0].lo;
      const std::int64_t j2 = two ? j - jlo + c2.subs[1].lo : 0;
      add_one_conn(prefix, c1.element(i, j), c2.element(i2, j2));
    }
  }
}

// EFHNLook with a message when the name is not flat.
std::uint32_t Flattener::lookup(const std::string& prefix, const std::string& name, std::string_view what) {
  const std::string key = concat_names(prefix, name_from_string(name));
  const auto it = table_.find(key);
  if (it == table_.end()) {
    st_.message(codes::kNoSuchNode, std::string{what} + ": no such node " + name_to_string(key));
    return kNil;
  }
  return flat_.node_of(it->second);
}

// efAddOneConn (FLATNODE_DOWARN).
void Flattener::add_one_conn(const std::string& prefix, const std::string& name1, const std::string& name2) {
  const std::uint32_t node1 = lookup(prefix, name1, "connect(1)");
  if (node1 == kNil) {
    return;
  }
  const std::uint32_t node2 = lookup(prefix, name2, "connect(2)");
  if (node2 != kNil && node1 != node2) {
    flat_.merge(node1, node2);
  }
}

// efFlatKills: children first (every use, subcircuits too), then the
// def's kills, newest first.
void Flattener::flat_kills(const HierContext& hc, std::size_t depth) {
  for_each_use(hc, depth, [&](const HierContext& child) { flat_kills(child, depth + 1); });
  const Def& def = st_.defs.at(hc.def);
  for (auto it = def.kills.rbegin(); it != def.kills.rend() && !st_.fatal; ++it) {
    const std::uint32_t node = lookup(hc.prefix, *it, "kill");
    if (node != kNil) {
      flat_.node(node).flags |= kFlagKilled;
    }
  }
}

void Flattener::report_names(std::uint32_t node) {
  std::size_t count = 0;
  for (std::uint32_t n = flat_.node(node).head; n != kNil; n = flat_.name(n).next) {
    if (count == kGlobalNamesShown) {
      st_.message(codes::kGlobalSplit, "    .... (no more names will be printed)");
      return;
    }
    st_.message(codes::kGlobalSplit, "    " + name_to_string(flat_.name(n).key));
    ++count;
  }
}

// efFlatGlobError.
void Flattener::report_split_global(std::string_view leaf, std::uint32_t glob, std::uint32_t flat) {
  st_.message(codes::kGlobalSplit, "*** Global name " + std::string{leaf} + " not fully connected:");
  st_.message(codes::kGlobalSplit, "One portion contains the names:");
  report_names(glob);
  st_.message(codes::kGlobalSplit, "The other portion contains the names:");
  report_names(flat);
  st_.message(codes::kGlobalSplit, "I'm merging the two pieces into a single node, but you");
  st_.message(codes::kGlobalSplit, "should be sure eventually to connect them in the layout.");
  st_.message(codes::kGlobalSplit, "");
}

// efFlatGlob. Magic resumes its list walk at the kept node after a merge,
// which may revisit nodes already seen; a revisit changes nothing (each
// seen node's global leaf already maps to it), so the walk goes on from
// where it was.
void Flattener::flat_glob() {
  GlobalTable global{kGlobalTableSize};
  for (std::uint32_t i = flat_.node_count(); i-- > 0;) {
    if (!flat_.live(i) || flat_.node(i).head == kNil) {
      continue;
    }
    const std::string leaf{name_leaf(flat_.name(flat_.node(i).head).key)};
    if (!st_.is_global(leaf)) {
      continue;
    }
    auto& entry = global.find_or_insert(leaf);
    if (!entry.value) {
      entry.value = i;
      continue;
    }
    const std::uint32_t glob = flat_.find(*entry.value);
    if (glob == i) {
      continue;
    }
    if ((flat_.node(glob).flags & kFlagDevTerm) == 0 && (flat_.node(i).flags & kFlagDevTerm) == 0) {
      report_split_global(leaf, glob, i);
    }
    entry.value = flat_.merge(i, glob);
  }
  name_globals(global);
}

// efFlatGlob's last pass: every global leaf not yet a flat name becomes a
// name of its node.
void Flattener::name_globals(const GlobalTable& global) {
  global.for_each([&](const auto& entry) {
    if (!entry.value || table_.count(entry.key) != 0) {
      return;
    }
    const std::uint32_t node = flat_.find(*entry.value);
    const std::uint32_t name = flat_.add_name(entry.key, node);
    table_.emplace(entry.key, name);
    flat_.link_head(node, name);
  });
}

FlatNode Flattener::output(std::uint32_t node) const {
  const GraphNode& n = flat_.node(node);
  FlatNode out;
  out.name = name_to_string(flat_.name(n.head).key);
  out.layer = st_.layers.at(n.layer);
  out.location = n.loc;
  out.substrate = (n.flags & kFlagGlobSubs) != 0;
  out.port = (n.flags & kFlagPort) != 0;
  out.top_port = (n.flags & kFlagTopPort) != 0;
  flat_.for_each_name(node, [&](std::uint32_t e) { out.names.push_back(name_components(flat_.name(e).key)); });
  for (const SavedLocation& loc : n.disjoint) {
    out.locations.push_back(NodeLocation{loc.ll, st_.layers.at(loc.layer)});
  }
  return out;
}

// EFVisitNodes: the flat node list, killed nodes skipped.
std::vector<FlatNode> Flattener::visit() {
  std::vector<FlatNode> out;
  for (std::uint32_t i = flat_.node_count(); i-- > 0;) {
    const GraphNode& n = flat_.node(i);
    if (flat_.live(i) && n.head != kNil && (n.flags & kFlagKilled) == 0) {
      out.push_back(output(i));
    }
  }
  return out;
}

}  // namespace

std::vector<FlatNode> flatten_nodes(ReadState& st, std::uint32_t root) {
  st.defs.at(root).subcircuit = false;  // the top cell is never a subcircuit
  Flattener f{st};
  const HierContext top{root, {}, Transform{}};
  f.flat_nodes(top, 0);
  if (!st.fatal) {
    f.flat_kills(top, 0);
  }
  if (!st.fatal) {
    f.flat_glob();
  }
  if (st.fatal) {
    return {};
  }
  return f.visit();
}

}  // namespace hefesto::extflat::detail
