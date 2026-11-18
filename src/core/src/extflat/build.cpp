// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The record builders of EFbuild.c (see build.hpp).
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#include "build.hpp"

#include <array>
#include <cstdlib>

#include "hier_name.hpp"
#include "text.hpp"

namespace hefesto::extflat::detail {

namespace {

constexpr std::array<std::string_view, 17> kDeviceClasses{
    "fet",   "mosfet", "asymmetric", "bjt",    "devres",  "devcap",  "devcaprev", "vsource",  "diode",
    "pdiode", "ndiode", "subckt",     "rsubckt", "msubckt", "csubckt", "dsubckt",   "veriloga",
};

enum DevClass : std::uint8_t {
  kFet = 0,
  kMosfet,
  kAsymmetric,
  kBjt,
  kRes,
  kCap,
  kCapRev,
  kVsource,
  kDiode,
  kPDiode,
  kNDiode,
  kSubckt,
  kRSubckt,
  kMSubckt,
  kCSubckt,
  kDSubckt,
  kVerilogA,
};

// The entry the hash entry of `name` designates, or kNil.
std::uint32_t lookup_name(Def& def, std::string_view name) {
  const auto it = def.names.find(std::string{name});
  return it == def.names.end() ? kNil : def.graph.resolve(it->second);
}

std::uint32_t layer_index(ReadState& st, const std::optional<std::string_view>& layer) {
  return layer ? st.layers.add(*layer) : 0;
}

// efBuildNode's handling of a name defined again right after a kill of a
// node that aliases it: the alias leaves the killed node. Returns true
// when the name must become a new node.
bool detach_killed_alias(Def& def, std::string_view name, std::uint32_t entry) {
  if (def.kills.empty()) {
    return false;
  }
  const std::uint32_t killed = lookup_name(def, def.kills.back());
  if (killed == kNil || def.graph.node_of(killed) != def.graph.node_of(entry)) {
    return false;
  }
  const std::uint32_t node = def.graph.node_of(killed);
  for (std::uint32_t n = def.graph.node(node).head; n != kNil; n = def.graph.name(n).next) {
    if (name_to_string(def.graph.name(n).key) == name) {
      def.graph.unlink(node, n);
      break;
    }
  }
  return true;
}

void add_to_existing(ReadState& st, Def& def, std::uint32_t entry, const NodeRecord& r) {
  GraphNode& node = def.graph.node(def.graph.node_of(entry));
  if (r.device_substrate) {
    node.flags |= kFlagSubsNode;
  }
  if (r.substrate) {
    node.flags |= kFlagGlobSubs;
  }
  if (r.ext_node) {
    // A duplicate port name at another place: prepend the location.
    const SavedLocation loc{{scale_coord(st, r.x), scale_coord(st, r.y)}, layer_index(st, r.layer)};
    node.disjoint.insert(node.disjoint.begin(), loc);
  }
}

// efNodeAddName: the new name heads the list only when it is preferred,
// the node is no top-level port and the name is not a forced alias.
void add_alias(Def& def, std::uint32_t node, std::string_view name, bool never_preferred) {
  const std::uint32_t entry = def.graph.add_name(name_from_string(name), node);
  def.names[std::string{name}] = entry;
  const GraphNode& n = def.graph.node(node);
  const bool top_port = (n.flags & kFlagTopPort) != 0;
  if (n.head == kNil ||
      (prefer_name(def.graph.name(entry).key, def.graph.name(n.head).key) && !top_port && !never_preferred)) {
    def.graph.link_head(node, entry);
  } else {
    def.graph.link_after_head(node, entry);
  }
}

// The text before the first `_uq` (strstr cut).
std::string_view before_uq(std::string_view name) noexcept {
  const std::size_t at = name.find("_uq");
  return at == std::string_view::npos ? name : name.substr(0, at);
}

// sscanf("%d:%d") right after a '[' or ','.
bool scan_range(std::string_view text, Range& range) {
  std::size_t used = 0;
  const auto lo = scan_int(text, &used);
  if (!lo || used >= text.size() || text[used] != ':') {
    return false;
  }
  const auto hi = scan_int(text.substr(used + 1));
  if (!hi) {
    return false;
  }
  range = Range{*lo, *hi};
  return true;
}

// The position of the first '[' that opens a subscript range (a ':'
// before any ']'), as efConnBuildName's search finds it.
std::size_t find_range_start(std::string_view name) noexcept {
  std::size_t cp = 0;
  for (;;) {
    cp = name.find('[', cp);
    if (cp == std::string_view::npos) {
      return cp;
    }
    std::size_t dp = cp + 1;
    while (dp < name.size() && name[dp] != ':' && name[dp] != ']') {
      ++dp;
    }
    if (dp < name.size() && name[dp] == ']') {
      cp = dp + 1;
      continue;
    }
    return cp;
  }
}

// One subscript of efConnBuildName's loop; `cp` is at its '[' or ','.
bool parse_subscript(ReadState& st, std::string_view name, std::size_t& cp, Range& range) {
  ++cp;
  if (!scan_range(name.substr(cp), range)) {
    st.read_error("Subscript syntax error");
    return false;
  }
  if (range.lo > range.hi) {
    st.read_error("Backwards subscript range [" + std::to_string(range.lo) + ":" + std::to_string(range.hi) + "]");
    return false;
  }
  while (cp < name.size() && name[cp] != ']' && name[cp] != ',') {
    ++cp;
  }
  if (cp < name.size() && name[cp] == ']') {
    ++cp;
  }
  return true;
}

// efConnBuildName.
bool build_conn_name(ReadState& st, std::string_view name, ConnName& out) {
  std::size_t cp = find_range_start(name);
  if (cp == std::string_view::npos) {
    out.before = std::string{name};
    return true;
  }
  out.before = std::string{name.substr(0, cp)};
  while (cp < name.size() && (name[cp] == '[' || name[cp] == ',')) {
    if (out.nsubs >= out.subs.size()) {
      st.read_error("Too many array subscripts (maximum=2)");
      return false;
    }
    if (!parse_subscript(st, name, cp, out.subs[out.nsubs])) {
      return false;
    }
    ++out.nsubs;
  }
  out.after = std::string{name.substr(cp)};
  return true;
}

// efConnInitSubs.
bool init_subs(ReadState& st, std::string_view name1, std::string_view name2, Conn& conn) {
  if (!build_conn_name(st, name1, conn.name1) || !build_conn_name(st, name2, conn.name2)) {
    return false;
  }
  if (conn.name1.nsubs != conn.name2.nsubs) {
    st.read_error("Number of subscripts doesn't match");
    return false;
  }
  for (std::size_t n = 0; n < conn.name1.nsubs; ++n) {
    const Range& a = conn.name1.subs[n];
    const Range& b = conn.name2.subs[n];
    if (std::int64_t{a.hi} - a.lo != std::int64_t{b.hi} - b.lo) {
      st.read_error("Subscript " + std::to_string(n) + " range mismatch");
      return false;
    }
  }
  return true;
}

// sscanf("[%d:%d:%d][%d:%d:%d]").
bool scan_array(std::string_view text, std::array<std::int32_t, 6>& v) {
  std::size_t at = 0;
  for (std::size_t i = 0; i < v.size(); ++i) {
    const char want = (i % 3 == 0) ? '[' : ':';
    if (at >= text.size() || text[at] != want) {
      return false;
    }
    ++at;
    std::size_t used = 0;
    const auto value = scan_int(text.substr(at), &used);
    if (!value) {
      return false;
    }
    v[i] = *value;
    at += used;
    if (i % 3 == 2) {
      if (at >= text.size() || text[at] != ']') {
        return false;
      }
      ++at;
    }
  }
  return true;
}

bool str_is_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  for (const char c : s) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

bool str_is_numeric(std::string_view s) {
  const std::string copy{s};
  char* end = nullptr;
  (void)std::strtod(copy.c_str(), &end);
  return end != copy.c_str();
}

bool starts_with_none(std::string_view s) noexcept {
  return s.substr(0, 4) == "None";
}

// The first terminal token after the class-specific values.
std::size_t terminal_start(std::uint8_t cls, bool has_model) noexcept {
  switch (cls) {
    case kFet:
    case kMosfet:
    case kAsymmetric:
      return 3;
    case kBjt:
      return 2;
    case kRes:
    case kCap:
    case kCapRev:
      return has_model ? 2 : 1;
    case kDiode:
    case kPDiode:
    case kNDiode:
    case kSubckt:
    case kVerilogA:
    case kMSubckt:
    case kRSubckt:
    case kCSubckt:
    case kDSubckt:
      return 0;
    default:
      return 1;
  }
}

bool has_optional_substrate(std::uint8_t cls) noexcept {
  return cls != kFet && cls != kMosfet && cls != kAsymmetric && cls != kVsource;
}

// efBuildDevNode.
void build_dev_node(ReadState& st, Def& def, std::string_view name, bool is_substrate) {
  bool is_new = false;
  if (lookup_name(def, name) == kNil) {
    build_node(st, def, NodeRecord{name, 0, 0, std::nullopt, false, is_substrate, false});
    is_new = true;
  }
  const std::uint32_t entry = lookup_name(def, name);
  GraphNode& node = def.graph.node(def.graph.node_of(entry));
  if (!is_substrate && (node.flags & kFlagGlobSubs) == 0) {
    return;
  }
  if (!st.is_global(name_leaf(def.graph.name(entry).key))) {
    node.flags |= kFlagSubsPort;
    if (is_new) {
      def.graph.name(entry).port = -1;
    }
  }
  node.flags |= kFlagSubsNode;
  if (is_new) {
    node.flags |= kFlagDevTerm;
  }
}

// The value checks of a new resistor or capacitor (efBuildDevice).
void check_device_value(ReadState& st, std::uint8_t cls, bool has_model, std::span<const std::string> args) {
  if (cls != kRes && cls != kCap && cls != kCapRev) {
    return;
  }
  const std::string& a0 = args[0];
  const std::string& a1 = args.size() > 1 ? args[1] : a0;
  if ((has_model && str_is_int(a0) && str_is_int(a1)) || str_is_numeric(a0)) {
    return;
  }
  if (has_model) {
    st.read_error("Error: expected L and W, got " + a0 + " " + a1);
  } else {
    st.read_error(std::string{"Error: expected "} + (cls == kRes ? "resistance" : "capacitance") +
                  " value, got " + a0);
  }
}

enum class DeviceSlot : std::uint8_t { fresh, duplicate, incompatible };

// The device table check. An incompatible duplicate is dropped. Magic keys
// devices by the tile type of their name in the extraction style (a name
// the style does not know never collides); the subset keys them by the
// name, which agrees whenever two records name the same device type.
DeviceSlot register_device(ReadState& st, Def& def, std::uint8_t cls, std::string_view type, db::Point ll,
                     std::size_t terminals) {
  const std::string key = std::to_string(ll.x) + "x" + std::to_string(ll.y) + "_" + std::string{type};
  const auto [it, inserted] = def.devices.try_emplace(key, DeviceKey{cls, std::string{type}, terminals});
  if (inserted) {
    return DeviceSlot::fresh;
  }
  const DeviceKey& old = it->second;
  const std::string where = std::string{kDeviceClasses[cls]} + " " + std::string{type} + " at (" +
                            std::to_string(ll.x) + ", " + std::to_string(ll.y) + ")";
  if (old.device_class != cls) {
    st.message(codes::kDeviceOverlap, "Device " + where + " overlaps incompatible device " +
                                          std::string{kDeviceClasses[old.device_class]} + " " + old.type + "!");
    return DeviceSlot::incompatible;
  }
  if (old.terminals != terminals) {
    st.message(codes::kDeviceOverlap, "Device " + where +
                                          " overlaps device with incompatible number of terminals (" +
                                          std::to_string(terminals) + " vs. " + std::to_string(old.terminals) + ")!");
    return DeviceSlot::incompatible;
  }
  return DeviceSlot::duplicate;
}

// The substrate given in the fixed fields (efBuildDevice after the device
// table check).
void build_fixed_substrate(ReadState& st, Def& def, std::uint8_t cls, std::size_t termstart,
                           std::span<const std::string> args) {
  if (args.size() <= 2) {
    return;
  }
  if (cls == kFet) {
    build_dev_node(st, def, args[2], true);
    return;
  }
  const bool fixed_class = cls == kMosfet || cls == kAsymmetric || cls == kBjt || cls == kRes || cls == kCap ||
                           cls == kCapRev;
  if (fixed_class && termstart == 3 && !starts_with_none(args[2])) {
    build_dev_node(st, def, args[2], true);
  }
}

}  // namespace

std::int32_t scale_coord(const ReadState& st, std::int32_t v) noexcept {
  return round_half_up_trunc(static_cast<double>(static_cast<float>(v) * st.loc_scale));
}

std::optional<std::uint8_t> device_class(std::string_view word) noexcept {
  for (std::size_t i = 0; i < kDeviceClasses.size(); ++i) {
    if (kDeviceClasses[i] == word) {
      return static_cast<std::uint8_t>(i);
    }
  }
  return std::nullopt;
}

void build_node(ReadState& st, Def& def, const NodeRecord& r) {
  std::uint32_t entry = lookup_name(def, r.name);
  if (entry != kNil && detach_killed_alias(def, r.name, entry)) {
    entry = kNil;
  }
  if (entry != kNil) {
    add_to_existing(st, def, entry, r);
    return;
  }
  GraphNode node;
  node.loc = db::Point{scale_coord(st, r.x), scale_coord(st, r.y)};
  node.layer = layer_index(st, r.layer);
  node.flags = (r.substrate ? kFlagGlobSubs : 0U) | (r.device_substrate ? kFlagSubsNode : 0U);
  if (r.ext_node) {
    node.disjoint.push_back(SavedLocation{node.loc, node.layer});
  }
  const std::uint32_t id = def.graph.add_node(std::move(node));
  const std::uint32_t name = def.graph.add_name(name_from_string(r.name), id);
  def.names[std::string{r.name}] = name;
  def.graph.link_head(id, name);
}

void build_port_node(ReadState& st, Def& def, std::string_view name, std::int32_t index, std::int32_t x,
                     std::int32_t y, std::string_view layer, bool toplevel) {
  if (lookup_name(def, name) == kNil) {
    build_node(st, def, NodeRecord{name, x, y, layer, false, false, false});
  }
  const std::uint32_t entry = lookup_name(def, name);
  GraphNode& node = def.graph.node(def.graph.node_of(entry));
  node.flags |= kFlagPort | (toplevel ? kFlagTopPort : 0U);
  def.graph.name(entry).port = index;
}

void build_equiv(ReadState& st, Def& def, std::string_view name1, std::string_view name2) {
  const std::uint32_t nn1 = lookup_name(def, name1);
  const std::uint32_t nn2 = lookup_name(def, name2);
  if (nn1 == nn2) {
    return;  // already one name, or neither exists (Magic returns here)
  }
  if (nn2 == kNil) {
    add_alias(def, def.graph.node_of(nn1), name2, true);
    return;
  }
  if (nn1 == kNil) {
    add_alias(def, def.graph.node_of(nn2), name1, false);
    return;
  }
  const std::int32_t p1 = def.graph.name(nn1).port;
  const std::int32_t p2 = def.graph.name(nn2).port;
  if (p1 >= 0 && p2 >= 0 && p1 != p2 && before_uq(name1) != before_uq(name2)) {
    st.message(codes::kShortedPorts, "Warning:  Ports \"" + std::string{name1} + "\" and \"" + std::string{name2} +
                                         "\" are electrically shorted.");
  }
  const std::uint32_t node1 = def.graph.node_of(nn1);
  const std::uint32_t node2 = def.graph.node_of(nn2);
  if (node1 == node2) {
    return;
  }
  def.graph.merge(node1, node2);
  // efNodeMerge leaves nn2 without a node; the hash entries holding it
  // are pointed at nn1.
  def.graph.name(nn2).redirect = nn1;
}

void build_merge(ReadState& st, Def& def, std::string_view name1, std::string_view name2) {
  Conn conn;
  if (init_subs(st, name1, name2, conn)) {
    def.conns.push_back(std::move(conn));
  }
}

void check_connection(ReadState& st, std::string_view name1, std::string_view name2) {
  Conn conn;
  (void)init_subs(st, name1, name2, conn);
}

void build_use(ReadState& st, Def& def, std::string_view def_name, std::string_view use_id, const Transform& t) {
  Use use;
  use.def = st.defs.lookup_or_create(def_name);
  use.trans = t;
  use.id = std::string{use_id};
  const std::size_t bracket = use_id.find('[');
  std::array<std::int32_t, 6> v{};
  if (bracket != std::string_view::npos && scan_array(use_id.substr(bracket), v)) {
    use.id = std::string{use_id.substr(0, bracket)};
    use.xlo = v[0];
    use.xhi = v[1];
    use.xsep = v[2];
    use.ylo = v[3];
    use.yhi = v[4];
    use.ysep = v[5];
  }
  auto& entry = def.use_table.find_or_insert(use.id);
  if (entry.value) {
    st.message(codes::kDuplicateUse, "Warning: use " + use.id + " appears more than once in def!");
  }
  def.uses.push_back(std::move(use));
  entry.value = static_cast<std::uint32_t>(def.uses.size() - 1);
}

bool build_device(ReadState& st, Def& def, std::uint8_t cls, std::string_view type, db::Point ll,
                  std::span<const std::string> args) {
  const bool has_model = type != "None";
  std::size_t termstart = terminal_start(cls, has_model);
  // Parameters (name=value) follow the fixed fields. Magic reads past the
  // tokens when every remaining one holds '='; the subset stops there.
  while (termstart < args.size() && args[termstart].find('=') != std::string::npos) {
    ++termstart;
  }
  if (termstart > args.size()) {
    return false;
  }
  if (has_optional_substrate(cls) && (args.size() - termstart) % 3 == 1) {
    if (!starts_with_none(args[termstart])) {
      build_dev_node(st, def, args[termstart], true);
    }
    ++termstart;
  }
  if ((args.size() - termstart) % 3 != 0) {
    return false;
  }
  const std::size_t terminals = (args.size() - termstart) / 3;
  const DeviceSlot slot = register_device(st, def, cls, type, ll, terminals);
  if (slot == DeviceSlot::incompatible) {
    return true;
  }
  if (slot == DeviceSlot::fresh) {
    check_device_value(st, cls, has_model, args);
  }
  build_fixed_substrate(st, def, cls, termstart, args);
  for (std::size_t n = 0; n < terminals; ++n) {
    build_dev_node(st, def, args[termstart + 3 * n], false);
  }
  return true;
}

void build_short(ReadState& st, Def& def, std::span<const std::string> args) {
  std::size_t start = 0;
  while (start < args.size() && args[start].find('=') != std::string::npos) {
    ++start;
  }
  if (start + 4 >= args.size()) {
    st.read_error("Bad terminal description for Short device");
    return;
  }
  build_equiv(st, def, args[start + 1], args[start + 4]);
}

void build_device_params(ReadState& st, Def& /*def*/, std::string_view type, std::span<const std::string> args) {
  if (!st.device_params.insert(std::string{type}).second) {
    return;  // the first declaration of a type wins
  }
  for (const std::string& a : args) {
    if (a.find('=') == std::string::npos) {
      st.read_error("Bad parameter assignment \"" + a + "\" for device \"" + std::string{type} + "\"");
    }
  }
}

}  // namespace hefesto::extflat::detail
