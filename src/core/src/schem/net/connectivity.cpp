// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Connectivity and net naming of one schematic (capability
// schematic-netlisting "Connection geometry", "Net naming order", "Unnamed
// nets", "Pass-through and shorted symbols", "Bus taps", "Global nets";
// design D9, Q-SCHEM-16).
//
// Wires and instance pins are nodes; geometry, pass-through pins and
// shorted instances join nodes into components (union-find), so a name
// given to any node of a component spreads to all of it. Bus taps are
// directed links from the component of pin 1 to that of pin 0, followed
// with an explicit work list. Every short is reported, wherever it is found
// (Q-SCHEM-16). Coordinates are compared exactly; points with a NaN
// coordinate touch nothing.
//
// Owner: NETLIST.

#include "hefesto/schem/net/connectivity.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <unordered_set>
#include <utility>

#include "hefesto/schem/net/bus.hpp"
#include "hefesto/schem/net/codes.hpp"

namespace hefesto::schem {

namespace {

bool has_nan(Point p) noexcept {
  return std::isnan(p.x) || std::isnan(p.y);
}

/// The endpoints ordered as the file reader orders them: x1 < x2, or x1 = x2
/// and y1 <= y2.
std::pair<Point, Point> ordered(Point a, Point b) noexcept {
  if (b.x < a.x || (b.x == a.x && b.y < a.y)) {
    return {b, a};
  }
  return {a, b};
}

class UnionFind {
 public:
  explicit UnionFind(std::size_t n) : parent_(n) { std::iota(parent_.begin(), parent_.end(), std::size_t{0}); }

  std::size_t find(std::size_t x) noexcept {
    while (parent_[x] != x) {
      parent_[x] = parent_[parent_[x]];
      x = parent_[x];
    }
    return x;
  }

  void unite(std::size_t a, std::size_t b) noexcept {
    a = find(a);
    b = find(b);
    if (a != b) {
      parent_[std::max(a, b)] = std::min(a, b);
    }
  }

 private:
  std::vector<std::size_t> parent_;
};

/// A point that connects to the wires it touches (a wire endpoint or an
/// instance pin) and its node.
struct Probe {
  Point at;
  std::size_t node = 0;
};

/// Probes on one vertical or horizontal line, sorted by the coordinate
/// along the line.
using Line = std::vector<std::pair<double, std::size_t>>;
using Lines = std::map<double, Line>;

/// Joins every probe with every non-ignored wire it touches. Vertical and
/// horizontal wires are matched with the probes on their line by binary
/// search, so the cost is O((n + k) log n) for k touches; other wires are
/// tested against the probes within their x range, found the same way.
class TouchJoiner {
 public:
  TouchJoiner(const std::vector<NetWire>& wires, const std::vector<Probe>& probes, UnionFind& uf)
      : wires_{wires}, probes_{probes}, uf_{uf} {
    for (const Probe& probe : probes) {
      if (!has_nan(probe.at)) {
        by_x_[probe.at.x].emplace_back(probe.at.y, probe.node);
        by_y_[probe.at.y].emplace_back(probe.at.x, probe.node);
      }
    }
    for (std::size_t i = 0; i < probes.size(); ++i) {
      if (!has_nan(probes[i].at)) {
        by_probe_x_.emplace_back(probes[i].at.x, i);
      }
    }
    std::sort(by_probe_x_.begin(), by_probe_x_.end());
    for (auto* lines : {&by_x_, &by_y_}) {
      for (auto& [key, line] : *lines) {
        std::sort(line.begin(), line.end());
      }
    }
  }

  void run() {
    for (std::size_t w = 0; w < wires_.size(); ++w) {
      const NetWire& wire = wires_[w];
      if (wire.ignored || has_nan(wire.p1) || has_nan(wire.p2)) {
        continue;
      }
      if (wire.p1.x == wire.p2.x) {
        join_line(w, by_x_, wire.p1.x, wire.p1.y, wire.p2.y);
      } else if (wire.p1.y == wire.p2.y) {
        join_line(w, by_y_, wire.p1.y, wire.p1.x, wire.p2.x);
      } else {
        join_any(w);
      }
    }
  }

 private:
  void join_line(std::size_t w, const Lines& lines, double key, double a, double b) {
    const auto it = lines.find(key);
    if (it == lines.end()) {
      return;
    }
    const Line& line = it->second;
    const auto lo = std::lower_bound(line.begin(), line.end(), std::pair{std::min(a, b), std::size_t{0}});
    for (auto p = lo; p != line.end() && p->first <= std::max(a, b); ++p) {
      uf_.unite(w, p->second);
    }
  }

  /// A wire that is neither vertical nor horizontal can only touch probes
  /// whose x lies within its x range: those are found by binary search.
  void join_any(std::size_t w) {
    const NetWire& wire = wires_[w];
    const double lo = std::min(wire.p1.x, wire.p2.x);
    const double hi = std::max(wire.p1.x, wire.p2.x);
    const auto first = std::lower_bound(by_probe_x_.begin(), by_probe_x_.end(), std::pair{lo, std::size_t{0}});
    for (auto p = first; p != by_probe_x_.end() && p->first <= hi; ++p) {
      const Probe& probe = probes_[p->second];
      if (touches(probe.at, wire.p1, wire.p2)) {
        uf_.unite(w, probe.node);
      }
    }
  }

  const std::vector<NetWire>& wires_;
  const std::vector<Probe>& probes_;
  UnionFind& uf_;
  Lines by_x_;
  Lines by_y_;
  std::vector<std::pair<double, std::size_t>> by_probe_x_;  // (x, probe index), sorted
};

struct Tap {
  std::size_t instance = 0;
  std::size_t bus_root = 0;  // component of pin 1
  std::size_t tap_root = 0;  // component of pin 0
};

/// The bus base name of "Bus taps": after the last `,` and the spaces after
/// it, up to the first `[`.
std::string_view bus_base(std::string_view bus) noexcept {
  if (bus.starts_with('#')) {
    bus.remove_prefix(1);  // `net3[2]` built from `#net3` is not automatic
  }
  const std::size_t comma = bus.rfind(',');
  if (comma != std::string_view::npos) {
    bus.remove_prefix(comma + 1);
    while (!bus.empty() && bus.front() == ' ') {
      bus.remove_prefix(1);
    }
  }
  return bus.substr(0, bus.find('['));
}

bool all_digits(std::string_view text) noexcept {
  return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

std::string tap_name(std::string_view bus, std::string_view lab) {
  if (lab.empty()) {
    return {};
  }
  if (lab.front() == '[' || all_digits(lab)) {
    return std::string{bus_base(bus)} + std::string{lab};
  }
  return std::string{lab};
}

/// The SCHEM-NET-001 text of a short. A `Bus tap` short names the tap name
/// computed from `lab` first, then the name pin 0 kept ("Bus taps",
/// scenario "Tap side already named", Q-SCHEM-37 approved 2026-11-25);
/// the other shorts name the kept name first.
std::string short_text(const NetShort& s) {
  const bool tap = s.tag == "Bus tap";
  return s.tag + " shorted: " + (tap ? s.incoming : s.existing) + " - " + (tap ? s.existing : s.incoming);
}

bool is_automatic(std::string_view name) noexcept {
  return !name.empty() && name.front() == '#';
}

class Namer {
 public:
  explicit Namer(const SchematicNets& schematic)
      : s_{schematic}, uf_{node_count(schematic)} {}

  NetNaming run() {
    build_components();
    name_from_labels();
    name_unnamed_wires();
    name_unnamed_pins();
    return collect();
  }

 private:
  struct Component {
    std::string name;
    bool tap_named = false;
  };

  static std::size_t node_count(const SchematicNets& s) {
    std::size_t n = s.wires.size();
    for (const auto& inst : s.instances) {
      n += inst.pins.size();
    }
    return n;
  }

  std::size_t pin_node(std::size_t instance, std::size_t pin) const { return pin_base_[instance] + pin; }

  [[nodiscard]] bool active(std::size_t instance) const { return !s_.instances[instance].ignored; }

  void build_components() {
    std::size_t base = s_.wires.size();
    for (const auto& inst : s_.instances) {
      pin_base_.push_back(base);
      base += inst.pins.size();
    }
    std::vector<Probe> probes;
    for (std::size_t w = 0; w < s_.wires.size(); ++w) {
      if (!s_.wires[w].ignored) {
        probes.push_back({s_.wires[w].p1, w});
        probes.push_back({s_.wires[w].p2, w});
      }
    }
    join_pins_at_points(probes);
    TouchJoiner{s_.wires, probes, uf_}.run();
    for (std::size_t i = 0; i < s_.instances.size(); ++i) {
      if (active(i)) {
        join_inside(i);
      }
    }
    collect_taps();
  }

  /// Adds the pins to `probes` and joins pins at equal points.
  void join_pins_at_points(std::vector<Probe>& probes) {
    std::map<std::pair<double, double>, std::size_t> at_point;
    for (std::size_t i = 0; i < s_.instances.size(); ++i) {
      if (!active(i)) {
        continue;
      }
      for (std::size_t p = 0; p < s_.instances[i].pins.size(); ++p) {
        const Point at = s_.instances[i].pins[p].at;
        if (has_nan(at)) {
          continue;
        }
        const std::size_t node = pin_node(i, p);
        probes.push_back({at, node});
        const auto [it, inserted] = at_point.try_emplace({at.x, at.y}, node);
        if (!inserted) {
          uf_.unite(node, it->second);
        }
      }
    }
  }

  /// Pass-through pins (same name) and shorted instances (all pins).
  void join_inside(std::size_t i) {
    const NetInstance& inst = s_.instances[i];
    std::map<std::string_view, std::size_t> by_name;
    for (std::size_t p = 0; p < inst.pins.size(); ++p) {
      if (inst.shorted && p > 0) {
        uf_.unite(pin_node(i, 0), pin_node(i, p));
      }
      const auto [it, inserted] = by_name.try_emplace(inst.pins[p].name, pin_node(i, p));
      if (!inserted) {
        uf_.unite(pin_node(i, p), it->second);
      }
    }
  }

  void collect_taps() {
    for (std::size_t i = 0; i < s_.instances.size(); ++i) {
      const NetInstance& inst = s_.instances[i];
      if (active(i) && inst.type == "bus_tap" && inst.pins.size() >= 2) {
        const Tap tap{i, uf_.find(pin_node(i, 1)), uf_.find(pin_node(i, 0))};
        taps_by_bus_[tap.bus_root].push_back(tap);
      }
    }
  }

  void name_from_labels() {
    for (std::size_t i = 0; i < s_.instances.size(); ++i) {
      const NetInstance& inst = s_.instances[i];
      if (active(i) && is_naming_type(inst.type) && inst.lab && !inst.lab->empty() && !inst.pins.empty()) {
        assign(uf_.find(pin_node(i, 0)), *inst.lab, "Net");
      }
    }
  }

  void name_unnamed_wires() {
    for (std::size_t w = 0; w < s_.wires.size(); ++w) {
      if (!s_.wires[w].ignored && unnamed(uf_.find(w))) {
        assign(uf_.find(w), next_automatic(), "Net");
      }
    }
  }

  void name_unnamed_pins() {
    for (std::size_t i = 0; i < s_.instances.size(); ++i) {
      if (!active(i)) {
        continue;
      }
      for (std::size_t p = 0; p < s_.instances[i].pins.size(); ++p) {
        const std::size_t root = uf_.find(pin_node(i, p));
        if (unnamed(root)) {
          assign(root, next_automatic(), "Instance pin");
        }
      }
    }
  }

  [[nodiscard]] bool unnamed(std::size_t root) const {
    const auto it = components_.find(root);
    return it == components_.end() || it->second.name.empty();
  }

  std::string next_automatic() {
    std::string name;
    do {
      name = "net" + std::to_string(++automatic_);
    } while (in_use_.contains(name));
    return "#" + name;
  }

  void set_name(std::size_t root, const std::string& name, bool by_tap) {
    Component& c = components_[root];
    c.name = name;
    c.tap_named = c.tap_named || by_tap;
    if (!is_automatic(name)) {
      in_use_.insert(name);
    }
  }

  void assign(std::size_t root, const std::string& name, std::string_view tag) {
    if (unnamed(root)) {
      set_name(root, name, false);
      spread_through_taps(root);
    } else if (components_[root].name != name) {
      report_short(tag, components_[root].name, name);
    }
  }

  /// Renames the tap side of every bus tap reached from `start`; each
  /// component is renamed through a tap at most once, so this ends.
  void spread_through_taps(std::size_t start) {
    std::vector<std::size_t> work{start};
    while (!work.empty()) {
      const std::size_t root = work.back();
      work.pop_back();
      const auto it = taps_by_bus_.find(root);
      if (it == taps_by_bus_.end()) {
        continue;
      }
      for (const Tap& tap : it->second) {
        const NetInstance& inst = s_.instances[tap.instance];
        const std::string name = tap_name(components_[root].name, inst.lab.value_or(std::string{}));
        if (rename_tap_side(tap.tap_root, name)) {
          work.push_back(tap.tap_root);
        }
      }
    }
  }

  bool rename_tap_side(std::size_t root, const std::string& name) {
    if (name.empty()) {
      return false;
    }
    const auto it = components_.find(root);
    const bool renamable =
        it == components_.end() || it->second.name.empty() || (is_automatic(it->second.name) && !it->second.tap_named);
    if (renamable) {
      set_name(root, name, true);
      return true;
    }
    if (it->second.name != name) {
      report_short("Bus tap", it->second.name, name);
    }
    return false;
  }

  void report_short(std::string_view tag, const std::string& existing, const std::string& incoming) {
    shorts_.push_back({std::string{tag}, existing, incoming});
  }

  [[nodiscard]] std::string name_of(std::size_t node) {
    const auto it = components_.find(uf_.find(node));
    return it == components_.end() ? std::string{} : it->second.name;
  }

  NetNaming collect() {
    NetNaming out;
    out.wire_nets.resize(s_.wires.size());
    for (std::size_t w = 0; w < s_.wires.size(); ++w) {
      if (!s_.wires[w].ignored) {
        out.wire_nets[w] = name_of(w);
      }
    }
    out.pin_nets.resize(s_.instances.size());
    for (std::size_t i = 0; i < s_.instances.size(); ++i) {
      if (active(i)) {
        collect_instance(i, out);
      }
    }
    for (NetShort& s : shorts_) {
      out.diagnostics.push_back(diag::make(net_codes::kShort, short_text(s)));
    }
    out.shorts = std::move(shorts_);
    return out;
  }

  void collect_instance(std::size_t i, NetNaming& out) {
    const NetInstance& inst = s_.instances[i];
    auto& nets = out.pin_nets[i];
    for (std::size_t p = 0; p < inst.pins.size(); ++p) {
      nets.push_back(name_of(pin_node(i, p)));
      if (is_automatic(nets.back())) {
        const std::size_t m = inst.pins[p].width * inst.vector_width;
        const auto [it, inserted] = out.multiplicity.try_emplace(nets.back(), m);
        it->second = std::min(it->second, m);
      }
    }
    if (nets.empty()) {
      return;
    }
    if (is_naming_type(inst.type) && (!inst.lab || inst.lab->empty())) {
      out.label_labs.emplace_back(i, is_automatic(nets[0]) ? nets[0].substr(1) : nets[0]);
    }
    if (inst.type == "label" && inst.global) {
      add_globals(nets[0], out);
    }
  }

  // First-seen order; the set keeps a wide global bus linear, not quadratic.
  void add_globals(const std::string& net, NetNaming& out) {
    const std::string_view name = is_automatic(net) ? std::string_view{net}.substr(1) : std::string_view{net};
    for (std::string& bit : expand_bus(name, s_.bus_budget)) {
      if (bit != "0" && globals_seen_.insert(bit).second) {
        out.globals.push_back(std::move(bit));
      }
    }
  }

  const SchematicNets& s_;
  UnionFind uf_;
  std::vector<std::size_t> pin_base_;
  std::map<std::size_t, std::vector<Tap>> taps_by_bus_;
  std::map<std::size_t, Component> components_;
  std::set<std::string> in_use_;
  std::unordered_set<std::string> globals_seen_;
  std::vector<NetShort> shorts_;
  unsigned long long automatic_ = 0;
};

}  // namespace

Point pin_point(const BoxRecord& box) noexcept {
  return {(box.p1.x + box.p2.x) / 2.0, (box.p1.y + box.p2.y) / 2.0};
}

bool touches(Point p, Point a, Point b) noexcept {
  const auto [p1, p2] = ordered(a, b);
  if ((p2.x - p1.x) * (p.y - p1.y) != (p2.y - p1.y) * (p.x - p1.x)) {
    return false;
  }
  if (p1.x < p2.x) {
    return p1.x <= p.x && p.x <= p2.x;
  }
  return p1.x == p2.x && p1.y <= p.y && p.y <= p2.y;
}

bool is_naming_type(std::string_view type) noexcept {
  return type == "label" || type == "ipin" || type == "opin" || type == "iopin";
}

NetNaming name_nets(const SchematicNets& schematic) {
  return Namer{schematic}.run();
}

std::string net_text(std::string_view net, const NetNaming& naming, std::string_view bus_replacement,
                     BusBudget* budget) {
  std::vector<std::string> bits;
  if (is_automatic(net)) {
    const auto it = naming.multiplicity.find(std::string{net});
    const std::size_t m = it == naming.multiplicity.end() ? 1 : it->second;
    const std::string base{net.substr(1)};
    bits = m > 1 ? expand_bus(base + "_[" + std::to_string(m - 1) + "..0]", budget) : std::vector<std::string>{base};
  } else {
    bits = expand_bus(net, budget);
  }
  return replace_bus_chars(join_bits(bits, ","), bus_replacement);
}

std::string net_name(std::string_view net, std::string_view bus_replacement) {
  return replace_bus_chars(net, bus_replacement);
}

}  // namespace hefesto::schem
