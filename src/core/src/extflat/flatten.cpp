// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EFFlatBuild, EFFlatBuildOneLevel and the visits (see flatten.hpp).
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

#include "flatten.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

#include "build.hpp"
#include "hier_name.hpp"
#include "text.hpp"

namespace hefesto::extflat::detail {

void FlatContext::message(std::string_view code, std::string text) const {
  sink.emit(diag::make(code, std::move(text)));
}

void FlatContext::fail(std::string_view code, std::string text) {
  if (!fatal) {
    fatal = diag::make_native(code, std::move(text));
    sink.emit(*fatal);
  }
}

namespace {

// One instance of a def below the root (HierContext).
struct HierContext {
  std::uint32_t def = 0;
  std::string prefix;  // instance path (hier_name.hpp); empty for the root
  Transform trans;
};

// efFlatNode's flags.
enum NodePass : unsigned {
  kStdCell = 1U << 0U,     // FLATNODE_STDCELL
  kDoWarn = 1U << 1U,      // FLATNODE_DOWARN
  kNoAbstract = 1U << 2U,  // FLATNODE_NOABSTRACT
  kHier = 1U << 3U,        // FLATNODE_HIER
  kChild = 1U << 4U,       // FLATNODE_CHILD
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

// The number of indices from `lo` to `hi` inclusive, in either order.
std::uint64_t span(std::int64_t lo, std::int64_t hi) {
  return static_cast<std::uint64_t>(lo <= hi ? hi - lo : lo - hi) + 1U;
}

// a * b, saturated at the largest value (two 32-bit spans can reach 2^64).
std::uint64_t saturating_product(std::uint64_t a, std::uint64_t b) {
  constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
  return b != 0 && a > kMax / b ? kMax : a * b;
}

// The number of elements of an array use.
std::uint64_t element_count(const Use& use) {
  return saturating_product(span(use.xlo, use.xhi), span(use.ylo, use.yhi));
}

// The number of element pairs an arrayed connection expands to (both
// names have the same ranges; efConnInitSubs checked them).
std::uint64_t pair_count(const ConnName& name) {
  const std::uint64_t first = span(name.subs[0].lo, name.subs[0].hi);
  return name.nsubs == 2 ? saturating_product(first, span(name.subs[1].lo, name.subs[1].hi)) : first;
}

// What is left of `limit` once `used` of it is spent.
std::uint64_t remaining(std::uint64_t used, std::uint64_t limit) {
  return used >= limit ? 0 : limit - used;
}

// HashGetNumEntries(&def->def_uses).
std::int64_t use_count(const Def& def) {
  std::int64_t n = 0;
  def.use_table.for_each([&](const auto& entry) { n += entry.value ? 1 : 0; });
  return n;
}

// The canonical name entry of a definition's own node (kNil when it has
// none), following the merges.
std::uint32_t local_head(const NodeGraph& g, std::uint32_t local) {
  std::uint32_t id = local;
  while (!g.live(id)) {
    id = g.node(id).parent;
  }
  return g.node(id).head;
}

std::vector<std::string> split_attrs(const std::string& text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const std::size_t comma = text.find(',', start);
    out.push_back(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
    if (comma == std::string::npos) {
      return out;
    }
    start = comma + 1;
  }
}

class Flattener {
 public:
  Flattener(FlatContext& ctx, const FlatPlan& plan, std::uint32_t root, bool clear_root)
      : ctx_(ctx), plan_(plan), root_(root), clear_root_(clear_root), on_stack_(ctx.defs.size(), 0),
        nodevices_(ctx.defs.size(), 0) {
    flat_.set_compat_attrs(ctx.options.magic_compat);
  }

  void stdcell_nodes(const HierContext& hc, std::size_t depth);
  void full_nodes(const HierContext& hc, std::size_t depth, unsigned pass);
  void deviceless_nodes(const HierContext& hc, std::size_t depth, std::int64_t& usecount);
  void deviceless_uses(const HierContext& root, std::int64_t& usecount);
  void flat_kills(const HierContext& hc, std::size_t depth);
  void flat_glob();
  void flat_caps(const HierContext& hc, std::size_t depth, bool deviceless);

  void visit_devices(const HierContext& hc, std::size_t depth, std::vector<FlatDevice>& out);
  void visit_resists(const HierContext& hc, std::size_t depth, std::vector<FlatResist>& out);
  void hier_devices(const HierContext& hc, std::vector<FlatDevice>& out);
  void hier_resists(const HierContext& hc, std::vector<FlatResist>& out);
  void visit_subcircuits(const HierContext& top, std::vector<FlatSubcircuit>& out);
  [[nodiscard]] std::vector<FlatNode> visit_nodes(bool hier);
  [[nodiscard]] std::vector<FlatCap> visit_caps() const;

  void mark_nodevices(std::uint32_t def) { nodevices_[def] = 1; }
  void set_nodevices(const std::vector<char>& marks) {
    for (std::size_t i = 0; i < marks.size() && i < nodevices_.size(); ++i) {
      nodevices_[i] = marks[i];
    }
  }
  [[nodiscard]] const std::vector<char>& nodevices_marks() const { return nodevices_; }
  [[nodiscard]] bool nodevices(std::uint32_t def) const { return nodevices_[def] != 0; }
  [[nodiscard]] bool failed() const { return ctx_.fatal.has_value(); }
  // Each traversal of the hierarchy counts its instances and expanded
  // connections against the limits afresh: the limits bound one
  // flattening of the design, not the number of visits made over it.
  void begin_pass() {
    instances_ = 0;
    connections_ = 0;
  }

 private:
  template <typename F>
  void for_each_use(const HierContext& hc, std::size_t depth, F&& visit_child);
  template <typename F>
  void for_each_element(const HierContext& hc, const Use& use, F&& visit_child);
  template <typename F>
  void for_each_conn(const HierContext& hc, const std::vector<Conn>& conns, F&& visit);
  template <typename F>
  void expand_conn(const Conn& conn, F& visit);
  [[nodiscard]] bool subckt(std::uint32_t def) const;
  bool count_instance();
  bool fits_instances(std::uint64_t count);
  bool fits_connections(std::uint64_t count);
  void add_nodes(const HierContext& hc, unsigned pass);
  void add_node_names(const HierContext& hc, const Def& def, std::uint32_t local, std::uint32_t flat);
  void add_conns(const HierContext& hc, unsigned pass);
  void add_one_conn(const HierContext& hc, const Conn& conn, const std::string& name1, const std::string& name2,
                    bool warn);
  [[nodiscard]] std::uint32_t lookup(const std::string& prefix, const std::string& name, const char* what);
  [[nodiscard]] std::uint32_t lookup_key(const std::string& key, const char* what);
  /// prefix + separator + name (EFHNConcat) in key_, reused between lookups.
  [[nodiscard]] const std::string& scratch_key(std::string_view prefix, std::string_view name, bool from_string);
  [[nodiscard]] std::uint32_t lookup_local(const std::string& prefix, const Def& def, std::uint32_t local,
                                           const char* what);
  void flat_single_cap(const HierContext& hc, const Conn& conn, const std::string& name1, const std::string& name2);
  [[nodiscard]] bool device_killed(const HierContext& hc, const Def& def, const Device& dev);
  [[nodiscard]] FlatDevice device_out(const HierContext& hc, const Def& def, const Device& dev);
  [[nodiscard]] FlatTerminal terminal_out(const std::string& prefix, const Def& def, std::uint32_t local,
                                          std::int32_t length, const std::optional<std::string>& attrs);
  [[nodiscard]] std::string_view local_key(const Def& def, std::uint32_t local) const;
  void report_split_global(std::string_view leaf, std::uint32_t glob, std::uint32_t flat);
  void report_names(std::uint32_t node);
  void name_globals(const GlobalTable& global);
  [[nodiscard]] FlatNode output(std::uint32_t node, bool hier) const;

  FlatContext& ctx_;
  const FlatPlan& plan_;
  std::uint32_t root_;
  bool clear_root_;
  NodeGraph flat_;
  std::unordered_map<std::string, std::uint32_t> table_;  // efNodeHashTable
  /// The flat names of anonymous nodes (NameEntry::anonymous): only the
  /// devices of their own definition look them up.
  std::unordered_map<std::string, std::uint32_t> anon_table_;
  std::map<std::pair<std::uint32_t, std::uint32_t>, float> cap_table_;  // efCapHashTable
  std::vector<char> on_stack_;
  std::vector<char> nodevices_;             // DEF_NODEVICES
  std::vector<std::size_t> output_index_;  // flat node -> index in the visited list
  std::uint64_t instances_ = 0;
  std::uint64_t connections_ = 0;
  std::string key_;  // scratch_key's buffer
};

bool Flattener::subckt(std::uint32_t def) const {
  switch (plan_.options.subcircuits) {
    case FlattenOptions::Subcircuits::none: return false;
    case FlattenOptions::Subcircuits::root_cleared:
      if (def == root_) {
        return false;
      }
      break;
    case FlattenOptions::Subcircuits::as_read: break;
  }
  return ctx_.defs.at(def).subcircuit && !(clear_root_ && def == root_);
}

bool Flattener::count_instance() {
  if (!fits_instances(1)) {
    return false;
  }
  ++instances_;
  return true;
}

// Whether `count` more instances stay within the cap. Each one is still
// counted as it is visited; an array too large for what is left of the cap
// fails here, before any element is visited (the walk would reach the cap
// anyway, after flattening as many elements as the cap allows).
bool Flattener::fits_instances(std::uint64_t count) {
  if (count > remaining(instances_, ctx_.options.limits.max_instances)) {
    ctx_.fail(codes::kLimit, "the flattened .ext hierarchy has more than " +
                                 std::to_string(ctx_.options.limits.max_instances) + " instances");
    return false;
  }
  return true;
}

// The same for the element pairs of an arrayed connection.
bool Flattener::fits_connections(std::uint64_t count) {
  if (count > remaining(connections_, ctx_.options.limits.max_connections)) {
    ctx_.fail(codes::kLimit, "the .ext merge records expand to more than " +
                                 std::to_string(ctx_.options.limits.max_connections) + " connections");
    return false;
  }
  return true;
}

namespace {

// One axis of an array use, walked from its lower index; the separation
// changes sign when the range is written high to low.
struct AxisRange {
  std::int64_t lo;
  std::int64_t hi;
  std::int64_t sep;
};

AxisRange axis_range(std::int32_t from, std::int32_t to, std::int32_t sep) {
  if (from <= to) {
    return AxisRange{from, to, std::int64_t{sep}};
  }
  return AxisRange{to, from, -std::int64_t{sep}};
}

}  // namespace

// The elements of an array use, x outer and y inner, each from its lower
// index (efHierSrUses). An array larger than what is left of the instance
// cap is refused before its first element.
template <typename F>
void Flattener::for_each_element(const HierContext& hc, const Use& use, F&& visit_child) {
  if (!fits_instances(element_count(use))) {
    return;
  }
  const AxisRange xr = axis_range(use.xlo, use.xhi, use.xsep);
  const AxisRange yr = axis_range(use.ylo, use.yhi, use.ysep);
  const std::int64_t xsep = xr.sep;
  const std::int64_t ysep = yr.sep;
  const Transform t = compose(use.trans, hc.trans);
  for (std::int64_t x = xr.lo; x <= xr.hi; ++x) {
    for (std::int64_t y = yr.lo; y <= yr.hi; ++y) {
      if (!count_instance()) {
        return;
      }
      const std::string component = use_component(use.id, use.xlo != use.xhi, use.ylo != use.yhi,
                                                  static_cast<std::int32_t>(x), static_cast<std::int32_t>(y));
      HierContext child{use.def, concat_names(hc.prefix, component),
                        translate_then(xsep * (x - use.xlo), ysep * (y - use.ylo), t)};
      visit_child(child);
      if (ctx_.fatal) {
        return;
      }
    }
  }
}

// efHierSrUses: the uses in use-table order. A use cycle and the depth cap
// stop the flattening (Magic recurses until the stack overflows).
template <typename F>
void Flattener::for_each_use(const HierContext& hc, std::size_t depth, F&& visit_child) {
  const Def& def = ctx_.defs.at(hc.def);
  on_stack_[hc.def] = 1;
  def.use_table.for_each([&](const auto& entry) {
    if (ctx_.fatal || !entry.value) {
      return;
    }
    const Use& use = def.uses[*entry.value];
    if (on_stack_[use.def] != 0) {
      ctx_.fail(codes::kUseCycle, "the .ext uses of cell " + def.name + " reach it again through " + use.id);
      return;
    }
    if (depth + 1 > ctx_.options.limits.max_depth) {
      ctx_.fail(codes::kLimit, "the .ext hierarchy is deeper than " + std::to_string(ctx_.options.limits.max_depth));
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

// efHierSrArray over a connection list, newest first (Magic prepends).
template <typename F>
void Flattener::for_each_conn(const HierContext& hc, const std::vector<Conn>& conns, F&& visit) {
  (void)hc;
  for (auto it = conns.rbegin(); it != conns.rend() && !ctx_.fatal; ++it) {
    const Conn& conn = *it;
    if (conn.name1.nsubs == 0) {
      visit(conn, conn.name1.before, conn.name2.before);
    } else {
      expand_conn(conn, visit);
    }
  }
}

// One arrayed connection: each element pair of its subscript ranges,
// counted against the connection limit. A connection with more pairs than
// what is left of the limit is refused before its first pair.
template <typename F>
void Flattener::expand_conn(const Conn& conn, F& visit) {
  const ConnName& c1 = conn.name1;
  const ConnName& c2 = conn.name2;
  if (!fits_connections(pair_count(c1))) {
    return;
  }
  const bool two = c1.nsubs == 2;
  const std::int64_t jlo = two ? c1.subs[1].lo : 0;
  const std::int64_t jhi = two ? c1.subs[1].hi : 0;
  for (std::int64_t i = c1.subs[0].lo; i <= c1.subs[0].hi && !ctx_.fatal; ++i) {
    for (std::int64_t j = jlo; j <= jhi; ++j) {
      if (!fits_connections(1)) {
        return;
      }
      ++connections_;
      const std::int64_t i2 = i - c1.subs[0].lo + c2.subs[0].lo;
      const std::int64_t j2 = two ? j - jlo + c2.subs[1].lo : 0;
      visit(conn, c1.element(i, j), c2.element(i2, j2));
    }
  }
}

// efFlatNodesStdCell: children first, except below a subcircuit, then the
// def's own nodes and (outside subcircuits) its connections.
void Flattener::stdcell_nodes(const HierContext& hc, std::size_t depth) {
  const bool subcircuit = subckt(hc.def);
  if (!subcircuit) {
    for_each_use(hc, depth, [&](const HierContext& child) { stdcell_nodes(child, depth + 1); });
  }
  if (ctx_.fatal) {
    return;
  }
  add_nodes(hc, kStdCell);
  if (!subcircuit && !ctx_.fatal) {
    add_conns(hc, kDoWarn);
  }
}

// efFlatNodes: every use (subcircuits too), then the def's own nodes and
// connections. Below FLATNODE_HIER the children carry FLATNODE_CHILD.
void Flattener::full_nodes(const HierContext& hc, std::size_t depth, unsigned pass) {
  if ((pass & kNoAbstract) != 0 && ctx_.defs.at(hc.def).abstract) {
    ctx_.message(codes::kAbstractView,
                 "Error:  Cell " + ctx_.defs.at(hc.def).name + " was extracted as an abstract view.");
  }
  const unsigned child_pass = pass | ((pass & kHier) != 0 ? kChild : 0U);
  for_each_use(hc, depth, [&](const HierContext& child) { full_nodes(child, depth + 1, child_pass); });
  if (ctx_.fatal) {
    return;
  }
  add_nodes(hc, pass);
  if (!ctx_.fatal) {
    add_conns(hc, pass);
  }
}

// efFlatNodesDeviceless: a device-less cell whose uses were all absorbed is
// absorbed too (its ports and connections flattened), flagged
// DEF_NODEVICES, and taken off its parent's count unless it is a
// subcircuit, abstract or primitive. Each array element counts once.
void Flattener::deviceless_nodes(const HierContext& hc, std::size_t depth, std::int64_t& usecount) {
  const Def& def = ctx_.defs.at(hc.def);
  std::int64_t newcount = use_count(def);
  if (newcount > 0) {
    for_each_use(hc, depth, [&](const HierContext& child) { deviceless_nodes(child, depth + 1, newcount); });
  }
  if (ctx_.fatal || !def.devices.empty() || newcount != 0) {
    return;
  }
  add_nodes(hc, kStdCell);
  add_conns(hc, kDoWarn);
  mark_nodevices(hc.def);
  if (!def.subcircuit && !def.abstract && !def.primitive) {
    --usecount;
  }
}

// EFFlatBuildOneLevel's efHierSrUses(root, efFlatNodesDeviceless): each use
// of the root, never the root itself (its connections were added once).
void Flattener::deviceless_uses(const HierContext& root, std::int64_t& usecount) {
  for_each_use(root, 0, [&](const HierContext& child) { deviceless_nodes(child, 1, usecount); });
}

// efAddNodes: the def's live nodes in list order (reverse creation); with
// FLATNODE_STDCELL a subcircuit gives its ports only. Capacitance and
// area/perimeter are copied except with FLATNODE_STDCELL or
// FLATNODE_CHILD (one-level flattening counts child parasitics once).
void Flattener::add_nodes(const HierContext& hc, unsigned pass) {
  const Def& def = ctx_.defs.at(hc.def);
  const bool stdcell = (pass & kStdCell) != 0;
  const bool keep_parasitics = !stdcell && (pass & kChild) == 0;
  const bool ports_only = stdcell && subckt(hc.def);
  for (std::uint32_t i = def.graph.node_count(); i-- > 0 && !ctx_.fatal;) {
    const GraphNode& local = def.graph.node(i);
    if (!def.graph.live(i) || (ports_only && (local.flags & kFlagPort) == 0)) {
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
    for (auto a = local.attrs.rbegin(); a != local.attrs.rend(); ++a) {
      node.attrs.push_back(NodeAttr{a->text, a->layer, transform_rect(hc.trans, a->rect)});
    }
    node.pa.resize(local.pa.size());
    if (keep_parasitics) {
      node.cap = local.cap;
      node.pa = local.pa;
    }
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
    if (ctx_.fatal) {
      return;
    }
    const std::string& key_local = def.graph.name(n).key;
    std::string key = bare ? key_local : concat_names(hc.prefix, key_local);
    auto& table = def.graph.name(n).anonymous ? anon_table_ : table_;
    if (const auto it = table.find(key); it != table.end()) {
      const std::uint32_t old = flat_.node_of(it->second);
      current = flat_.find(current);
      current = old != current ? flat_.merge(old, current) : old;
      return;
    }
    if (flat_.name_count() >= ctx_.options.limits.max_flat_names) {
      ctx_.fail(codes::kLimit, "the flattened .ext netlist has more than " +
                                   std::to_string(ctx_.options.limits.max_flat_names) + " names");
      return;
    }
    current = flat_.find(current);
    const std::uint32_t entry = flat_.add_name(key, current);
    table.emplace(std::move(key), entry);
    flat_.link_after_head(current, entry);
  });
}

// efAddConns: the merge records, newest first.
void Flattener::add_conns(const HierContext& hc, unsigned pass) {
  const Def& def = ctx_.defs.at(hc.def);
  const bool warn = (pass & kDoWarn) != 0;
  for_each_conn(hc, def.conns, [&](const Conn& conn, const std::string& n1, const std::string& n2) {
    add_one_conn(hc, conn, n1, n2, warn);
  });
}

const std::string& Flattener::scratch_key(std::string_view prefix, std::string_view name, bool from_string) {
  key_.assign(prefix);
  if (!prefix.empty()) {
    key_.push_back(kSep);
  }
  const std::size_t first = key_.size();
  key_.append(name);
  if (from_string) {
    std::replace(key_.begin() + static_cast<std::ptrdiff_t>(first), key_.end(), '/', kSep);  // EFStrToHN
  }
  return key_;
}

std::uint32_t Flattener::lookup_key(const std::string& key, const char* what) {
  const auto it = table_.find(key);
  if (it == table_.end()) {
    if (what != nullptr) {
      ctx_.message(codes::kNoSuchNode, std::string{what} + ": no such node " + name_to_string(key));
    }
    return kNil;
  }
  return flat_.node_of(it->second);
}

// The flat node of a definition's own node, through the table its
// canonical name is in.
std::uint32_t Flattener::lookup_local(const std::string& prefix, const Def& def, std::uint32_t local,
                                      const char* what) {
  const std::string& key = scratch_key(prefix, local_key(def, local), false);
  const std::uint32_t head = local_head(def.graph, local);
  if (head == kNil || !def.graph.name(head).anonymous) {
    return lookup_key(key, what);
  }
  const auto it = anon_table_.find(key);
  return it == anon_table_.end() ? kNil : flat_.node_of(it->second);
}

// EFHNLook, with a message when the name is not flat and `what` is given.
std::uint32_t Flattener::lookup(const std::string& prefix, const std::string& name, const char* what) {
  return lookup_key(scratch_key(prefix, name, true), what);
}

// efAddOneConn: the deltas go to the first node, then the two merge.
void Flattener::add_one_conn(const HierContext& hc, const Conn& conn, const std::string& name1,
                             const std::string& name2, bool warn) {
  const std::uint32_t node1 = lookup(hc.prefix, name1, warn ? "connect(1)" : nullptr);
  if (node1 == kNil) {
    return;
  }
  GraphNode& n = flat_.node(node1);
  n.cap += conn.cap;
  add_perim_area(n.pa, conn.pa);
  const std::uint32_t node2 = lookup(hc.prefix, name2, warn ? "connect(2)" : nullptr);
  if (node2 != kNil && node1 != node2) {
    flat_.merge(node1, node2);
  }
}

// efFlatKills: children first (every use, subcircuits too), then the
// def's kills, newest first.
void Flattener::flat_kills(const HierContext& hc, std::size_t depth) {
  for_each_use(hc, depth, [&](const HierContext& child) { flat_kills(child, depth + 1); });
  const Def& def = ctx_.defs.at(hc.def);
  for (auto it = def.kills.rbegin(); it != def.kills.rend() && !ctx_.fatal; ++it) {
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
      ctx_.message(codes::kGlobalSplit, "    .... (no more names will be printed)");
      return;
    }
    ctx_.message(codes::kGlobalSplit, "    " + name_to_string(flat_.name(n).key));
    ++count;
  }
}

// efFlatGlobError.
void Flattener::report_split_global(std::string_view leaf, std::uint32_t glob, std::uint32_t flat) {
  ctx_.message(codes::kGlobalSplit, "*** Global name " + std::string{leaf} + " not fully connected:");
  ctx_.message(codes::kGlobalSplit, "One portion contains the names:");
  report_names(glob);
  ctx_.message(codes::kGlobalSplit, "The other portion contains the names:");
  report_names(flat);
  ctx_.message(codes::kGlobalSplit, "I'm merging the two pieces into a single node, but you");
  ctx_.message(codes::kGlobalSplit, "should be sure eventually to connect them in the layout.");
  ctx_.message(codes::kGlobalSplit, "");
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
    if (!ctx_.is_global(leaf)) {
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

// efFlatSingleCap: below the threshold an unresolved name is silent; a
// killed node or a self coupling drops the record; a coupling to the
// global substrate goes to the other node's ground capacitance.
void Flattener::flat_single_cap(const HierContext& hc, const Conn& conn, const std::string& name1,
                                const std::string& name2) {
  const bool silent = std::fabs(static_cast<double>(conn.cap) / 1000) < plan_.options.cap_threshold;
  const std::uint32_t n1 = lookup(hc.prefix, name1, silent ? nullptr : "cap(1)");
  if (n1 == kNil || (flat_.node(n1).flags & kFlagKilled) != 0) {
    return;
  }
  const std::uint32_t n2 = lookup(hc.prefix, name2, silent ? nullptr : "cap(2)");
  if (n2 == kNil || (flat_.node(n2).flags & kFlagKilled) != 0 || n1 == n2) {
    return;
  }
  if ((flat_.node(n1).flags & kFlagGlobSubs) != 0) {
    flat_.node(n2).cap += conn.cap;
  } else if ((flat_.node(n2).flags & kFlagGlobSubs) != 0) {
    flat_.node(n1).cap += conn.cap;
  } else {
    cap_table_[{std::min(n1, n2), std::max(n1, n2)}] += conn.cap;
  }
}

// efFlatCaps (every use, then the def's couplings, newest first), or
// efFlatCapsDeviceless, which skips the couplings of a processed cell that
// has devices (every cell below the one being flattened is processed
// before it in EFHierSrDefs order).
void Flattener::flat_caps(const HierContext& hc, std::size_t depth, bool deviceless) {
  const Def& def = ctx_.defs.at(hc.def);
  if (!deviceless || use_count(def) > 0) {
    for_each_use(hc, depth, [&](const HierContext& child) { flat_caps(child, depth + 1, deviceless); });
  }
  if (ctx_.fatal || (deviceless && hc.def != root_ && !nodevices(hc.def))) {
    return;
  }
  for_each_conn(hc, def.caps, [&](const Conn& conn, const std::string& n1, const std::string& n2) {
    flat_single_cap(hc, conn, n1, n2);
  });
}

std::string_view Flattener::local_key(const Def& def, std::uint32_t local) const {
  const std::uint32_t head = local_head(def.graph, local);
  return head == kNil ? std::string_view{} : std::string_view{def.graph.name(head).key};
}

// efnode_pa of a definition's own node (not the flat node).
std::vector<std::pair<std::int32_t, std::int32_t>> local_area_perim(const Def& def, std::uint32_t local) {
  std::uint32_t id = local;
  while (!def.graph.live(id)) {
    id = def.graph.node(id).parent;
  }
  std::vector<std::pair<std::int32_t, std::int32_t>> out;
  for (const PerimArea& pa : def.graph.node(id).pa) {
    out.emplace_back(pa.area, pa.perim);
  }
  return out;
}

// efDevKilled: a terminal whose node (instance path + its canonical local
// name) is killed; an unresolved name is reported as "kill".
bool Flattener::device_killed(const HierContext& hc, const Def& def, const Device& dev) {
  for (const DevTerm& term : dev.terms) {
    const std::uint32_t node = lookup_local(hc.prefix, def, term.node, "kill");
    if (node != kNil && (flat_.node(node).flags & kFlagKilled) != 0) {
      return true;
    }
  }
  return false;
}

FlatTerminal Flattener::terminal_out(const std::string& prefix, const Def& def, std::uint32_t local,
                                     std::int32_t length, const std::optional<std::string>& attrs) {
  FlatTerminal t;
  const std::uint32_t node = lookup_local(prefix, def, local, nullptr);
  if (node != kNil) {
    t.node = name_to_string(flat_.name(flat_.node(node).head).key);
    t.node_index = output_index_[node];
  } else {
    t.node = name_to_string(scratch_key(prefix, local_key(def, local), false));
  }
  t.length = length;
  const std::string_view local_name = local_key(def, local);
  t.local_node.reserve(def.name.size() + 1 + local_name.size());
  t.local_node.append(def.name).append("/").append(local_name);
  t.local_area_perim = local_area_perim(def, local);
  if (attrs) {
    t.attribute_text = *attrs;
    t.attributes = split_attrs(*attrs);
  }
  return t;
}

FlatDevice Flattener::device_out(const HierContext& hc, const Def& def, const Device& dev) {
  FlatDevice out;
  out.device_class = std::string{device_class_name(dev.device_class)};
  out.model = dev.type;
  out.area = transform_rect(hc.trans, dev.rect);
  out.local_area = dev.rect;
  for (const DevTerm& term : dev.terms) {
    out.terminals.push_back(terminal_out(hc.prefix, def, term.node, term.length, term.attrs));
  }
  if (dev.subs != kNil) {
    const FlatTerminal subs = terminal_out(hc.prefix, def, dev.subs, 0, std::nullopt);
    out.substrate = subs.node;
    out.substrate_index = subs.node_index;
    out.substrate_local = name_to_string(local_key(def, dev.subs));
    out.substrate_area_perim = subs.local_area_perim;
  }
  out.params = dev.params;
  out.hier_prefix = name_to_string(hc.prefix);
  out.cap = dev.cap;
  out.res = dev.res;
  out.device_area = dev.area;
  out.perimeter = dev.perim;
  const LengthWidth lw = device_length_width(dev, dev.terms, plan_.options.symbols);
  out.length = lw.length;
  out.width = lw.width;
  out.scale = (plan_.scale_changed && def.scale != 1.0F) ? def.scale : 1.0F;
  out.def = def.name;
  return out;
}

// efVisitDevs: not into subcircuits; children first, then the def's
// devices in device-table order, those on a killed node left out.
void Flattener::visit_devices(const HierContext& hc, std::size_t depth, std::vector<FlatDevice>& out) {
  if (subckt(hc.def)) {
    return;
  }
  for_each_use(hc, depth, [&](const HierContext& child) { visit_devices(child, depth + 1, out); });
  const Def& def = ctx_.defs.at(hc.def);
  def.for_each_device([&](const Device& dev) {
    if (!ctx_.fatal && !device_killed(hc, def, dev)) {
      out.push_back(device_out(hc, def, dev));
    }
  });
}

// efVisitResists: not into subcircuits; children first, then the def's
// resistors, newest first; killed and self resistors left out.
void Flattener::visit_resists(const HierContext& hc, std::size_t depth, std::vector<FlatResist>& out) {
  if (subckt(hc.def)) {
    return;
  }
  for_each_use(hc, depth, [&](const HierContext& child) { visit_resists(child, depth + 1, out); });
  const Def& def = ctx_.defs.at(hc.def);
  for_each_conn(hc, def.resistors, [&](const Conn& conn, const std::string& name1, const std::string& name2) {
    const std::uint32_t n1 = lookup(hc.prefix, name1, "resist(1)");
    if (n1 == kNil || (flat_.node(n1).flags & kFlagKilled) != 0) {
      return;
    }
    const std::uint32_t n2 = lookup(hc.prefix, name2, "resist(2)");
    if (n2 == kNil || (flat_.node(n2).flags & kFlagKilled) != 0 || n1 == n2) {
      return;
    }
    out.push_back(FlatResist{name_to_string(flat_.name(flat_.node(n1).head).key),
                             name_to_string(flat_.name(flat_.node(n2).head).key), static_cast<double>(conn.res)});
  });
}

// efHierVisitDevs: the def's own devices; one on a killed node is reported
// and still visited.
void Flattener::hier_devices(const HierContext& hc, std::vector<FlatDevice>& out) {
  const Def& def = ctx_.defs.at(hc.def);
  def.for_each_device([&](const Device& dev) {
    bool killed = false;
    for (const DevTerm& term : dev.terms) {
      const std::uint32_t node = lookup_key(scratch_key({}, local_key(def, term.node), false), nullptr);
      killed = killed || (node != kNil && (flat_.node(node).flags & kFlagKilled) != 0);
    }
    if (killed) {
      ctx_.message(codes::kEliminatedNodes, "Error:  Device at (" + std::to_string(dev.rect.x0) + " " +
                                                std::to_string(dev.rect.y0) +
                                                ") is connected to one or more eliminated nodes.");
    }
    FlatDevice d = device_out(hc, def, dev);
    d.on_killed_node = killed;
    out.push_back(std::move(d));
  });
}

// efHierVisitResists: the def's own resistors, resolved in its own node
// table and given by their local canonical names.
void Flattener::hier_resists(const HierContext& hc, std::vector<FlatResist>& out) {
  const Def& def = ctx_.defs.at(hc.def);
  auto local = [&def](const std::string& name) -> std::uint32_t {
    const auto it = def.names.find(name);
    if (it == def.names.end()) {
      return kNil;
    }
    std::uint32_t entry = it->second;
    while (def.graph.name(entry).redirect != kNil) {
      entry = def.graph.name(entry).redirect;
    }
    std::uint32_t id = def.graph.name(entry).node;
    while (!def.graph.live(id)) {
      id = def.graph.node(id).parent;
    }
    return id;
  };
  for_each_conn(hc, def.resistors, [&](const Conn& conn, const std::string& name1, const std::string& name2) {
    const std::uint32_t n1 = local(name1);
    const std::uint32_t n2 = n1 == kNil ? kNil : local(name2);
    if (n1 == kNil || n2 == kNil || n1 == n2) {
      return;
    }
    out.push_back(FlatResist{name_to_string(local_key(def, n1)), name_to_string(local_key(def, n2)), static_cast<double>(conn.res)});
  });
}

// EFVisitSubcircuits: the root if it is a subcircuit, then for each use a
// subcircuit is called and any other cell is searched.
void Flattener::visit_subcircuits(const HierContext& top, std::vector<FlatSubcircuit>& out) {
  auto record = [&](const HierContext& hc, bool is_top) {
    out.push_back(FlatSubcircuit{ctx_.defs.at(hc.def).name, name_to_string(hc.prefix), name_components(hc.prefix),
                                 is_top});
  };
  if (subckt(top.def)) {
    record(top, true);
  }
  std::function<void(const HierContext&, std::size_t)> search = [&](const HierContext& hc, std::size_t depth) {
    if (subckt(hc.def)) {
      record(hc, false);
      return;
    }
    for_each_use(hc, depth, [&](const HierContext& child) { search(child, depth + 1); });
  };
  for_each_use(top, 0, [&](const HierContext& child) { search(child, 1); });
}

FlatNode Flattener::output(std::uint32_t node, bool hier) const {
  const GraphNode& n = flat_.node(node);
  FlatNode out;
  out.name = name_to_string(flat_.name(n.head).key);
  out.layer = ctx_.layers.at(n.layer);
  out.location = n.loc;
  out.substrate = (n.flags & kFlagGlobSubs) != 0;
  out.port = (n.flags & kFlagPort) != 0;
  out.top_port = (n.flags & kFlagTopPort) != 0;
  flat_.for_each_name(node, [&](std::uint32_t e) { out.names.push_back(name_components(flat_.name(e).key)); });
  for (const SavedLocation& loc : n.disjoint) {
    out.locations.push_back(NodeLocation{loc.ll, ctx_.layers.at(loc.layer)});
  }
  out.raw_cap = n.cap;
  out.cap = n.cap;
  const std::string& head = flat_.name(n.head).key;
  const bool compat_ground = !hier && plan_.substrate_compat && name_component_count(head) == 1 &&
                             (head == "GND!" || (!plan_.options.gnd.empty() && head == plan_.options.gnd));
  if (compat_ground || ((hier || !plan_.substrate_compat) && out.substrate)) {
    out.cap = 0.0F;
  }
  out.resistance = node_resistance(n.pa, plan_.resists);
  for (const PerimArea& pa : n.pa) {
    out.area_perim.emplace_back(pa.area, pa.perim);
  }
  for (const NodeAttr& a : n.attrs) {
    out.attributes.push_back(a.text);
  }
  out.substrate_port = (n.flags & kFlagSubsPort) != 0;
  out.device_substrate = (n.flags & kFlagSubsNode) != 0;
  out.device_terminal_only = (n.flags & kFlagDevTerm) != 0;
  return out;
}

// EFVisitNodes (or EFHierVisitNodes): the flat node list, killed nodes
// skipped. Records each node's index for the device terminals.
std::vector<FlatNode> Flattener::visit_nodes(bool hier) {
  std::vector<FlatNode> out;
  output_index_.assign(flat_.node_count(), kNoNode);
  for (std::uint32_t i = flat_.node_count(); i-- > 0;) {
    const GraphNode& n = flat_.node(i);
    if (flat_.live(i) && n.head != kNil && (n.flags & kFlagKilled) == 0) {
      output_index_[i] = out.size();
      out.push_back(output(i, hier));
    }
  }
  return out;
}

// The coupling table in the deterministic order of Q-NETLIST-1: each pair
// with the smaller name first, the pairs by name.
std::vector<FlatCap> Flattener::visit_caps() const {
  std::vector<FlatCap> out;
  for (const auto& [key, cap] : cap_table_) {
    std::string a = name_to_string(flat_.name(flat_.node(key.first).head).key);
    std::string b = name_to_string(flat_.name(flat_.node(key.second).head).key);
    if (b < a) {
      std::swap(a, b);
    }
    out.push_back(FlatCap{std::move(a), std::move(b), static_cast<double>(cap)});
  }
  std::sort(out.begin(), out.end(), [](const FlatCap& x, const FlatCap& y) {
    return x.a != y.a ? x.a < y.a : x.b < y.b;
  });
  return out;
}

Netlist finish(Flattener& f, std::vector<FlatNode> nodes, std::vector<FlatDevice> devices,
               std::vector<FlatResist> resists) {
  Netlist netlist{std::move(nodes), {}};
  netlist.devices = std::move(devices);
  netlist.resists = std::move(resists);
  netlist.caps = f.visit_caps();
  return netlist;
}

}  // namespace

std::int32_t node_resistance(const std::vector<PerimArea>& pa, const std::vector<std::int32_t>& resists) {
  std::int32_t resist = 0;
  for (std::size_t n = 0; n < pa.size() && n < resists.size(); ++n) {
    const std::int32_t area = pa[n].area;
    const std::int32_t perim = pa[n].perim;
    if (area <= 0 || perim <= 0) {
      continue;
    }
    const double v = static_cast<double>(perim) * static_cast<double>(perim) - 16.0 * area;
    const float s = v < 0.0 ? 0.0F : static_cast<float>(std::sqrt(v));
    const auto fperim = static_cast<float>(perim);
    const double dresist = (static_cast<double>(fperim) + static_cast<double>(s)) /
                           (static_cast<double>(fperim) - static_cast<double>(s)) * resists[n];
    if (std::isnan(dresist) || dresist + static_cast<double>(resist) > static_cast<double>(INT_MAX)) {
      resist = INT_MAX;
    } else {
      resist = static_cast<std::int32_t>(static_cast<double>(resist) + dresist);
    }
  }
  return resist;
}

namespace {

char ascii_tolower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// StrIsInt: an optional sign, then only digits (the empty rest included).
bool is_c_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  return std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// efDevFixLW: `ext:l=<v>` and `ext:w=<v>` in a comma-separated attribute
// list, v an integer or a known symbol.
void fix_length_width(std::string_view attrs, const std::map<std::string, std::int32_t, std::less<>>& symbols,
                      LengthWidth& lw) {
  std::size_t cp = 0;
  while (cp < attrs.size()) {
    const std::size_t end = std::min(attrs.find(',', cp), attrs.size());
    const std::string_view item = attrs.substr(cp, end - cp);
    if (item.size() >= 6 && item.substr(0, 4) == "ext:" && item[5] == '=') {
      const char which = ascii_tolower(item[4]);
      const std::string_view value = item.substr(6);
      std::optional<std::int32_t> v;
      if (is_c_int(value)) {
        v = c_atoi(value);
      } else if (const auto it = symbols.find(value); it != symbols.end()) {
        v = it->second;
      }
      if (v && which == 'w') {
        lw.width = *v;
      } else if (v && which == 'l') {
        lw.length = *v;
      }
    }
    cp = end + 1;
  }
}

// A legacy `fet`: L and W from its area and perimeter (two terminals) or
// its terminal lengths. Magic's int square and its division by a zero
// length are undefined behaviour; both are made total in both modes.
LengthWidth fet_length_width(const Device& dev, const std::vector<DevTerm>& terms) {
  LengthWidth lw;
  if (terms.size() == 2) {
    const std::int64_t perim = dev.perim;
    const double v = static_cast<double>(perim * perim) - 16 * static_cast<double>(dev.area);
    const auto root = static_cast<std::int64_t>(v > 0.0 ? std::sqrt(v) : 0.0);
    const std::int64_t l = (perim - root) >> 2;
    lw.length = static_cast<std::int32_t>(l);
    lw.width = l == 0 ? 0 : static_cast<std::int32_t>(dev.area / l);
    return lw;
  }
  if (terms.empty()) {
    return lw;
  }
  const DevTerm& source = terms.size() > 1 ? terms[1] : terms[0];
  const DevTerm& drain = terms.size() >= 3 ? terms[2] : source;
  lw.length = terms[0].length / 2;
  lw.width = static_cast<std::int32_t>((std::int64_t{source.length} + drain.length) / 2);
  return lw;
}

}  // namespace

LengthWidth device_length_width(const Device& dev, const std::vector<DevTerm>& terms,
                                const std::map<std::string, std::int32_t, std::less<>>& symbols) {
  if (dev.device_class != kDevFet) {
    if (dev.device_class == kDevVolt) {
      return {};
    }
    return {dev.length, dev.width};
  }
  LengthWidth lw = fet_length_width(dev, terms);
  if (!terms.empty() && terms[0].attrs) {
    fix_length_width(*terms[0].attrs, symbols, lw);
  }
  return lw;
}

std::vector<FlatNode> flatten_nodes(ReadState& st, std::uint32_t root) {
  FlatContext ctx{st.sink, st.options, st.globals, st.defs, st.layers, std::nullopt};
  FlatPlan plan;
  plan.flags = FlatFlags::flat_nodes | FlatFlags::no_flat_subckt;
  plan.substrate_compat = st.compat;
  plan.resists = st.resists;
  Flattener f{ctx, plan, root, true};
  const HierContext top{root, {}, Transform{}};
  f.stdcell_nodes(top, 0);
  if (!ctx.fatal) {
    f.begin_pass();
    f.flat_kills(top, 0);
  }
  if (!ctx.fatal) {
    f.flat_glob();
  }
  if (ctx.fatal) {
    st.fatal = ctx.fatal;
    return {};
  }
  return f.visit_nodes(false);
}

Netlist flatten_full(FlatContext& ctx, std::uint32_t root, const FlatPlan& plan) {
  if (!has(plan.flags, FlatFlags::flat_nodes)) {
    return Netlist{};
  }
  const bool stdcell = has(plan.flags, FlatFlags::no_flat_subckt);
  Flattener f{ctx, plan, root, stdcell};
  const HierContext top{root, {}, Transform{}};
  if (stdcell) {
    f.stdcell_nodes(top, 0);
  } else {
    f.full_nodes(top, 0, kDoWarn | (has(plan.flags, FlatFlags::warn_abstract) ? kNoAbstract : 0U));
  }
  if (!f.failed()) {
    f.begin_pass();
    f.flat_kills(top, 0);
  }
  if (!f.failed() && !has(plan.flags, FlatFlags::no_name_merge)) {
    f.flat_glob();
  }
  if (!f.failed() && has(plan.flags, FlatFlags::flat_caps)) {
    f.begin_pass();
    f.flat_caps(top, 0, false);
  }
  std::vector<FlatNode> nodes = f.visit_nodes(false);
  std::vector<FlatDevice> devices;
  std::vector<FlatResist> resists;
  std::vector<FlatSubcircuit> subcircuits;
  f.begin_pass();
  f.visit_devices(top, 0, devices);
  f.begin_pass();
  f.visit_resists(top, 0, resists);
  f.begin_pass();
  f.visit_subcircuits(top, subcircuits);
  if (f.failed()) {
    return Netlist{};
  }
  Netlist netlist = finish(f, std::move(nodes), std::move(devices), std::move(resists));
  netlist.subcircuits = std::move(subcircuits);
  return netlist;
}

Netlist flatten_level(FlatContext& ctx, std::uint32_t def_id, const FlatPlan& plan, std::vector<char>& nodevices) {
  Flattener f{ctx, plan, def_id, false};
  f.set_nodevices(nodevices);
  const HierContext top{def_id, {}, Transform{}};
  const Def& def = ctx.defs.at(def_id);
  f.full_nodes(top, 0, kHier);
  std::int64_t usecount = use_count(def);
  if (!f.failed() && usecount > 0) {
    // efHierSrUses over the root's uses: the root itself is never absorbed.
    f.begin_pass();
    f.deviceless_uses(top, usecount);
  }
  if (usecount == 0 && def.devices.empty()) {
    f.mark_nodevices(def_id);
  }
  nodevices = f.nodevices_marks();
  if (!f.failed()) {
    f.begin_pass();
    f.flat_kills(top, 0);
  }
  if (!f.failed() && !has(plan.flags, FlatFlags::no_name_merge)) {
    f.flat_glob();
  }
  if (!f.failed() && has(plan.flags, FlatFlags::flat_caps)) {
    f.begin_pass();
    f.flat_caps(top, 0, true);
  }
  std::vector<FlatNode> nodes = f.visit_nodes(true);
  std::vector<FlatDevice> devices;
  std::vector<FlatResist> resists;
  f.hier_devices(top, devices);
  f.hier_resists(top, resists);
  if (f.failed()) {
    return Netlist{};
  }
  return finish(f, std::move(nodes), std::move(devices), std::move(resists));
}

}  // namespace hefesto::extflat::detail
