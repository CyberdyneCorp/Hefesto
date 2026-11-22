// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The record builders of EFbuild.c (see build.hpp).
// Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D13; M6 EXTSUB).

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

// (int)(0.5 + (float)v * locScale * locScale), an area delta.
std::int32_t scale_area(const ReadState& st, std::int32_t v) noexcept {
  return round_half_up_trunc(static_cast<double>(static_cast<float>(v) * st.loc_scale * st.loc_scale));
}

// The area/perimeter pairs of a node record: one per resistance class
// while two tokens remain, the rest zero.
std::vector<PerimArea> node_perim_area(const ReadState& st, std::span<const std::string> av) {
  std::vector<PerimArea> pa(st.classes());
  for (std::size_t n = 0; n < pa.size() && 2 * n + 1 < av.size(); ++n) {
    pa[n] = PerimArea{c_atoi(av[2 * n]), c_atoi(av[2 * n + 1])};
  }
  return pa;
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
  if (st.options.warnings) {
    st.read_error("Warning: duplicate node name " + std::string{r.name});
  }
  GraphNode& node = def.graph.node(def.graph.node_of(entry));
  node.cap += r.cap;
  add_perim_area(node.pa, node_perim_area(st, r.area_perim));
  if (r.device_substrate) {
    node.flags |= kFlagSubsNode;
  }
  if (r.substrate) {
    node.flags |= kFlagGlobSubs;
    st.compat = false;
  }
  if (st.options.noscale && r.ext_node) {
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

char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// strcasecmp() == 0 in the C locale.
bool equal_ignoring_case(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (ascii_lower(a[i]) != ascii_lower(b[i])) {
      return false;
    }
  }
  return true;
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
      return 1;  // vsource: its value (EFread.c sets no token count for it; Q-NETLIST-51)
  }
}

bool has_optional_substrate(std::uint8_t cls) noexcept {
  return cls != kFet && cls != kMosfet && cls != kAsymmetric && cls != kVsource;
}

// efBuildDevNode.
std::uint32_t build_dev_node(ReadState& st, Def& def, std::string_view name, bool is_substrate) {
  bool is_new = false;
  if (lookup_name(def, name) == kNil) {
    if (st.options.warnings && !is_substrate) {
      st.read_error("Node " + std::string{name} + " doesn't exist so creating it");
    }
    build_node(st, def, NodeRecord{name, 0, 0, std::nullopt, false, is_substrate, false});
    is_new = true;
  }
  const std::uint32_t entry = lookup_name(def, name);
  const std::uint32_t id = def.graph.node_of(entry);
  GraphNode& node = def.graph.node(id);
  if (!is_substrate && (node.flags & kFlagGlobSubs) == 0) {
    return id;
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
  return id;
}

// The values a device record parses before the device table check
// (efBuildDevice's devtmp).
struct DevScratch {
  float cap = 0.0F;
  float res = 0.0F;
  std::int32_t area = 0;
  std::int32_t perim = 0;
  std::int32_t length = 0;
  std::int32_t width = 0;
  std::vector<std::string> params;  // newest first
  std::uint32_t subs = kNil;
};

// strncasecmp(parm_type, key, 2) == 0.
bool param_type_matches(std::string_view ptype, std::string_view key) noexcept {
  for (std::size_t i = 0; i < 2; ++i) {
    const char a = i < ptype.size() ? ptype[i] : '\0';
    const char b = i < key.size() ? key[i] : '\0';
    if (ascii_lower(a) != ascii_lower(b)) {
      return false;
    }
    if (a == '\0') {
      return true;
    }
  }
  return true;
}

bool declared(const ReadState& st, std::string_view type, std::string_view key) {
  const auto it = st.params.find(type);
  if (it == st.params.end()) {
    return false;
  }
  for (const DevParam& p : it->second) {
    if (param_type_matches(p.type, key)) {
      return true;
    }
  }
  return false;
}

// a, p, l and w are kept as values when the key is one character or its
// digit is 0 (a0, p0, l0, w0).
bool primary_key(std::string_view token, std::size_t eq) noexcept {
  return eq == 1 || token[1] == '0';
}

// One declared parameter of efBuildDevice's parameter loop.
void apply_param(const ReadState& st, const std::string& token, std::size_t eq, DevScratch& t) {
  const std::string_view value = std::string_view{token}.substr(eq + 1);
  const bool primary = primary_key(token, eq);
  switch (token[0]) {
    case 'a':
      if (primary) {
        t.area = scale_area(st, c_atoi(value));
      }
      break;
    case 'p':
      if (primary) {
        t.perim = scale_coord(st, c_atoi(value));
      }
      break;
    case 'l':
    case 'w': {
      if (!primary) {
        t.params.insert(t.params.begin(), token);
        break;
      }
      (token[0] == 'l' ? t.length : t.width) = scale_coord(st, c_atoi(value));
      break;
    }
    case 'c': t.cap = static_cast<float>(c_atof(value)); break;
    case 'r': t.res = static_cast<float>(c_atof(value)); break;
    default: break;
  }
}

// efBuildDevice's parameter loop: returns the first token after them.
std::size_t parse_params(const ReadState& st, std::string_view type, std::span<const std::string> args,
                         std::size_t termstart, DevScratch& t) {
  // Magic reads past the tokens when every remaining one holds '='; the
  // reader stops there.
  while (termstart < args.size()) {
    const std::string& token = args[termstart];
    const std::size_t eq = token.find('=');
    if (eq == std::string::npos) {
      break;
    }
    if (declared(st, type, std::string_view{token}.substr(0, eq))) {
      apply_param(st, token, eq, t);
    } else {
      t.params.insert(t.params.begin(), token);
    }
    ++termstart;
  }
  return termstart;
}

// The tile type of a device name for the device table's key.
std::optional<std::int32_t> device_tile_type(ReadState& st, std::string_view type) {
  if (st.options.device_tile_type) {
    return st.options.device_tile_type(type);
  }
  if (type == "None") {
    return std::nullopt;
  }
  // No extraction style: number each name in order of first appearance,
  // below the unknown names' range.
  const auto [it, inserted] =
      st.synthetic_types.try_emplace(std::string{type}, -1 - static_cast<std::int32_t>(st.synthetic_types.size()));
  return it->second;
}

std::string device_key(const db::Rect& r, std::int32_t ttype) {
  return std::to_string(r.x0) + "x" + std::to_string(r.y0) + "_" + std::to_string(ttype);
}

// The device table key: the type's tile type, or for an unknown type the
// smallest index from DBNumTypes whose key is unused.
std::string device_table_key(ReadState& st, const Def& def, std::string_view type, const db::Rect& r,
                             std::int32_t& ttype) {
  if (const auto known = device_tile_type(st, type)) {
    ttype = *known;
    return device_key(r, ttype);
  }
  ttype = st.options.tile_type_count;
  while (def.dev_table.find(device_key(r, ttype)) != nullptr && ttype < INT32_MAX) {
    ++ttype;
  }
  return device_key(r, ttype);
}

// The check of a record that lands on an existing device: false (with the
// overlap message) when it is incompatible.
bool compatible_duplicate(ReadState& st, const Device& old, std::uint8_t cls, std::string_view type,
                          const db::Rect& r, std::size_t terminals) {
  const std::string where = std::string{kDeviceClasses[cls]} + " " + std::string{type} + " at (" +
                            std::to_string(r.x0) + ", " + std::to_string(r.y0) + ")";
  if (old.device_class != cls) {
    st.message(codes::kDeviceOverlap, "Device " + where + " overlaps incompatible device " +
                                          std::string{kDeviceClasses[old.device_class]} + " " + old.type + "!");
    return false;
  }
  if (old.terms.size() != terminals) {
    st.message(codes::kDeviceOverlap, "Device " + where +
                                          " overlaps device with incompatible number of terminals (" +
                                          std::to_string(terminals) + " vs. " + std::to_string(old.terms.size()) +
                                          ")!");
    return false;
  }
  return true;
}

// The class values of a resistor or capacitor (efBuildDevice): L and W,
// or the value, or the error.
void rc_values(ReadState& st, Device& dev, bool has_model, std::span<const std::string> args) {
  const std::string& a0 = args[0];
  const std::string& a1 = args.size() > 1 ? args[1] : a0;
  if (has_model && str_is_int(a0) && str_is_int(a1)) {
    dev.length = c_atoi(a0);
    dev.width = c_atoi(a1);
    return;
  }
  if (str_is_numeric(a0)) {
    (dev.device_class == kRes ? dev.res : dev.cap) = static_cast<float>(c_atof(a0));
    return;
  }
  if (has_model) {
    st.read_error("Error: expected L and W, got " + a0 + " " + a1);
    dev.length = 0;
    dev.width = 0;
  } else {
    st.read_error(std::string{"Error: expected "} + (dev.device_class == kRes ? "resistance" : "capacitance") +
                  " value, got " + a0);
    (dev.device_class == kRes ? dev.res : dev.cap) = 0.0F;
  }
}

// The fixed values of a new device (efBuildDevice's class switch).
void class_values(ReadState& st, Device& dev, bool has_model, std::span<const std::string> args) {
  switch (dev.device_class) {
    case kFet:
      dev.area = args.empty() ? 0 : c_atoi(args[0]);
      dev.perim = args.size() > 1 ? c_atoi(args[1]) : 0;
      break;
    case kMosfet:
    case kAsymmetric:
    case kBjt:
      dev.length = args.empty() ? 0 : c_atoi(args[0]);
      dev.width = args.size() > 1 ? c_atoi(args[1]) : 0;
      break;
    case kRes:
    case kCap:
    case kCapRev:
      if (!args.empty()) {
        rc_values(st, dev, has_model, args);
      }
      break;
    default: break;
  }
}

Device new_device(std::uint8_t cls, std::string_view type, const db::Rect& r, std::size_t terminals,
                  DevScratch& t) {
  Device dev;
  dev.device_class = cls;
  dev.type = std::string{type};
  dev.rect = r;
  dev.cap = t.cap;
  dev.res = t.res;
  dev.area = t.area;
  dev.perim = t.perim;
  dev.length = t.length;
  dev.width = t.width;
  dev.params = std::move(t.params);
  dev.terms.resize(terminals);
  return dev;
}

// The substrate given in the fixed fields (efBuildDevice after the device
// table check).
std::uint32_t fixed_substrate(ReadState& st, Def& def, std::uint8_t cls, std::size_t termstart,
                              std::span<const std::string> args, std::uint32_t subs) {
  if (args.size() <= 2) {
    return subs;
  }
  if (cls == kFet) {
    return build_dev_node(st, def, args[2], true);
  }
  const bool fixed_class = cls == kMosfet || cls == kAsymmetric || cls == kBjt || cls == kRes || cls == kCap ||
                           cls == kCapRev;
  if (fixed_class && termstart == 3 && !starts_with_none(args[2])) {
    return build_dev_node(st, def, args[2], true);
  }
  return subs;
}

// Finds or creates the device of the table key; nullptr for an
// incompatible duplicate.
Device* place_device(ReadState& st, Def& def, std::uint8_t cls, std::string_view type, const db::Rect& r,
                     std::size_t terminals, DevScratch& t, std::span<const std::string> args) {
  std::int32_t ttype = 0;
  const std::string key = device_table_key(st, def, type, r, ttype);
  auto& entry = def.dev_table.find_or_insert(key);
  if (entry.value) {
    Device& old = def.devices[*entry.value];
    return compatible_duplicate(st, old, cls, type, r, terminals) ? &old : nullptr;
  }
  const bool has_model = type != "None";
  Device dev = new_device(cls, type, r, terminals, t);
  dev.tile_type = ttype;
  class_values(st, dev, has_model, args);
  def.devices.push_back(std::move(dev));
  entry.value = static_cast<std::uint32_t>(def.devices.size() - 1);
  return &def.devices.back();
}

// The zero-valued resistor or voltage source between two shorted ports
// (efBuildEquiv and efBuildMerge with `short resistor`/`short voltage`).
void build_short_device(ReadState& st, Def& def, std::string_view name1, std::string_view name2) {
  const std::array<std::string, 10> argv{"0.0", "dummy", "0", "0", std::string{name1}, "0", "0",
                                         std::string{name2}, "0", "0"};
  const std::uint8_t cls = st.options.shorts == ReadOptions::PortShorts::resistor ? kRes : kVsource;
  (void)build_device(st, def, cls, "None", db::Rect{}, argv);
}

bool shorts_on(const ReadState& st) noexcept {
  return st.options.shorts != ReadOptions::PortShorts::none;
}

enum class ShortAction : std::uint8_t { merge, keep_apart, device_built };

// efBuildEquiv for two ports of different indices: merge, keep apart, or
// build a short device between them.
ShortAction port_short(ReadState& st, Def& def, std::string_view name1, std::string_view name2, bool resist) {
  if (st.options.spice && equal_ignoring_case(name1, name2)) {
    return ShortAction::merge;
  }
  if (before_uq(name1) == before_uq(name2)) {
    return resist ? ShortAction::keep_apart : ShortAction::merge;
  }
  if (shorts_on(st)) {
    build_short_device(st, def, name1, name2);
    return ShortAction::device_built;
  }
  if (resist) {
    return ShortAction::keep_apart;
  }
  st.message(codes::kShortedPorts, "Warning:  Ports \"" + std::string{name1} + "\" and \"" + std::string{name2} +
                                       "\" are electrically shorted.");
  return ShortAction::merge;
}

bool different_ports(const Def& def, std::uint32_t nn1, std::uint32_t nn2) {
  const std::int32_t p1 = def.graph.name(nn1).port;
  const std::int32_t p2 = def.graph.name(nn2).port;
  return p1 >= 0 && p2 >= 0 && p1 != p2;
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

std::string_view device_class_name(std::uint8_t cls) noexcept {
  return cls < kDeviceClasses.size() ? kDeviceClasses[cls] : std::string_view{};
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
  node.cap = r.cap;
  node.pa = node_perim_area(st, r.area_perim);
  if (st.options.noscale && r.ext_node) {
    node.disjoint.push_back(SavedLocation{node.loc, node.layer});
  }
  if (r.substrate) {
    st.compat = false;
  }
  const std::uint32_t id = def.graph.add_node(std::move(node));
  const std::uint32_t name = def.graph.add_name(name_from_string(r.name), id);
  def.names[std::string{r.name}] = name;
  def.graph.link_head(id, name);
}

void adjust_subcap(ReadState& st, Def& def, std::string_view name, float cap) {
  const std::uint32_t entry = lookup_name(def, name);
  if (entry != kNil) {
    def.graph.node(def.graph.node_of(entry)).cap += cap;
  } else if (st.options.warnings) {
    st.read_error("Error: subcap has unknown node " + std::string{name});
  }
}

void build_attr(ReadState& st, Def& def, std::string_view name, const db::Rect& r, std::string_view layer,
                std::string_view text) {
  const std::uint32_t entry = lookup_name(def, name);
  if (entry == kNil) {
    st.read_error("Attribute for nonexistent node " + std::string{name} + " ignored");
    return;
  }
  GraphNode& node = def.graph.node(def.graph.node_of(entry));
  node.attrs.insert(node.attrs.begin(), NodeAttr{std::string{text}, st.layers.add(layer), r});
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

void build_equiv(ReadState& st, Def& def, std::string_view name1, std::string_view name2, bool resist) {
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
  if (different_ports(def, nn1, nn2) && port_short(st, def, name1, name2, resist) != ShortAction::merge) {
    return;
  }
  const std::uint32_t node1 = def.graph.node_of(nn1);
  const std::uint32_t node2 = def.graph.node_of(nn2);
  if (node1 == node2) {
    return;
  }
  if (st.options.warnings) {
    st.read_error("Merged nodes " + std::string{name1} + " and " + std::string{name2});
  }
  def.graph.merge(node1, node2);
  // efNodeMerge leaves nn2 without a node; the hash entries holding it
  // are pointed at nn1. Device terminals reach the kept node through
  // NodeGraph::find.
  def.graph.name(nn2).redirect = nn1;
}

void build_merge(ReadState& st, Def& def, std::string_view name1, std::string_view name2, float cap,
                 std::span<const std::string> area_perim) {
  if (before_uq(name1) != before_uq(name2) && shorts_on(st)) {
    const std::uint32_t nn1 = lookup_name(def, name1);
    const std::uint32_t nn2 = lookup_name(def, name2);
    if (nn1 != kNil && nn2 != kNil && different_ports(def, nn1, nn2)) {
      build_short_device(st, def, name1, name2);
      return;
    }
  }
  Conn conn;
  if (!init_subs(st, name1, name2, conn)) {
    return;
  }
  conn.cap = cap;
  conn.pa.resize(st.classes());
  for (std::size_t n = 0; n < conn.pa.size() && 2 * n + 1 < area_perim.size(); ++n) {
    conn.pa[n] = PerimArea{scale_area(st, c_atoi(area_perim[2 * n])), scale_coord(st, c_atoi(area_perim[2 * n + 1]))};
  }
  def.conns.push_back(std::move(conn));
}

void build_cap(ReadState& st, Def& def, std::string_view name1, std::string_view name2, float cap) {
  Conn conn;
  if (init_subs(st, name1, name2, conn)) {
    conn.cap = cap;
    def.caps.push_back(std::move(conn));
  }
}

void build_resistor(ReadState& st, Def& def, std::string_view name1, std::string_view name2, float res) {
  Conn conn;
  if (init_subs(st, name1, name2, conn)) {
    conn.res = res;
    def.resistors.push_back(std::move(conn));
  }
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

bool build_device(ReadState& st, Def& def, std::uint8_t cls, std::string_view type, const db::Rect& r,
                  std::span<const std::string> args) {
  DevScratch t;
  std::size_t termstart = parse_params(st, type, args, terminal_start(cls, type != "None"), t);
  if (termstart > args.size()) {
    return false;
  }
  if (has_optional_substrate(cls) && (args.size() - termstart) % 3 == 1) {
    if (!starts_with_none(args[termstart])) {
      t.subs = build_dev_node(st, def, args[termstart], true);
    }
    ++termstart;
  }
  if ((args.size() - termstart) % 3 != 0) {
    return false;
  }
  const std::size_t terminals = (args.size() - termstart) / 3;
  Device* dev = place_device(st, def, cls, type, r, terminals, t, args);
  if (dev == nullptr) {
    return true;
  }
  const std::size_t index = static_cast<std::size_t>(dev - def.devices.data());
  const std::uint32_t subs = fixed_substrate(st, def, cls, termstart, args, t.subs);
  def.devices[index].subs = subs;
  for (std::size_t n = 0; n < terminals; ++n) {
    const std::string& attrs = args[termstart + 3 * n + 2];
    DevTerm term;
    term.node = build_dev_node(st, def, args[termstart + 3 * n], false);
    term.length = c_atoi(args[termstart + 3 * n + 1]);
    if (attrs != "0") {
      term.attrs = attrs;
    }
    def.devices[index].terms[n] = std::move(term);
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
  // With overlays the short merges only while the .res.ext is read.
  if (!st.options.resist_overlay || st.reading_overlay) {
    build_equiv(st, def, args[start + 1], args[start + 4], false);
  }
}

namespace {

// One token of efBuildDeviceParams.
DevParam parse_declared_param(std::string_view type, std::string_view token, std::size_t eq, std::size_t index) {
  DevParam p;
  p.type = std::string{token.substr(0, eq == 1 ? 1 : 2)};
  std::string_view name = token.substr(eq + 1);
  std::size_t cut = std::string_view::npos;
  if (const std::size_t star = name.find('*'); star != std::string_view::npos) {
    cut = star;
    p.scale = c_atof(name.substr(star + 1));
    // Magic leaves parm_offset uninitialised here; 0 in both modes
    // (Q-NETLIST-8, tier 1).
  } else if (const std::size_t plus = name.find('+'); plus != std::string_view::npos) {
    cut = plus;
    p.offset = c_atoi(name.substr(plus + 1));
  } else if (const std::size_t minus = name.find('-'); minus != std::string_view::npos) {
    cut = minus;
    p.offset = static_cast<std::int32_t>(-static_cast<std::int64_t>(c_atoi(name.substr(minus + 1))));
  }
  if (!type.empty() && type.front() == ':') {
    // A cell's instance parameter: the token (cut where Magic wrote its
    // NUL) and the token index as the type.
    p.name = std::string{cut == std::string_view::npos ? token : token.substr(0, eq + 1 + cut)};
    p.type = {static_cast<char>('0' + (index / 10) % 10), static_cast<char>('0' + index % 10)};
  } else {
    p.name = std::string{name.substr(0, cut)};
  }
  return p;
}

}  // namespace

void build_device_params(ReadState& st, std::string_view type, std::span<const std::string> args) {
  auto& list = st.params[std::string{type}];
  if (!list.empty()) {
    return;  // the first declaration with a parameter wins
  }
  for (std::size_t n = 0; n < args.size(); ++n) {
    const std::size_t eq = args[n].find('=');
    if (eq == std::string::npos) {
      st.read_error("Bad parameter assignment \"" + args[n] + "\" for device \"" + std::string{type} + "\"");
      continue;
    }
    // Prepended: the list is in reverse token order (Q-NETLIST-9).
    list.insert(list.begin(), parse_declared_param(type, args[n], eq, n));
  }
}

}  // namespace hefesto::extflat::detail
