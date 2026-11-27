// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pin matching (lvs-hierarchy "Pin matching"; lvs-reports "Pin table";
// Q-LVS-17: the results −1 and −2 are never produced; Q-LVS-23: the net
// correspondence is a local value, nothing is written after it is freed;
// Q-LVS-24: the size-1-array equivalence goes through
// placeholder_pin_equivalence, its one branch point).
//
// Owner: LVSHIER.

#include <algorithm>
#include <map>
#include <optional>

#include "hefesto/lvs/hier/hierarchy.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/report/format.hpp"

namespace hefesto::lvs {

namespace {

std::string_view normalized(std::string_view name, bool array_equivalence) {
  if (name.starts_with('\\')) {
    name.remove_prefix(1);
  }
  if (name.ends_with('!')) {
    name.remove_suffix(1);
  }
  if (array_equivalence && name.ends_with("[0]") && name.size() > 3) {
    name.remove_suffix(3);
  }
  return name;
}

}  // namespace

bool pin_names_equal(std::string_view a, std::string_view b, bool case_insensitive, bool array_equivalence) {
  return names_equal(normalized(a, array_equivalence), normalized(b, array_equivalence), case_insensitive);
}

namespace {

constexpr int kUnpaired = -1;
constexpr int kNoPinOnNet = -2;  // the partner net has no port: `(no pin, node is <net2>)`

/// Device pins per net of `cell` (index NetId).
std::vector<std::size_t> device_pins(const Cell& cell) {
  std::vector<std::size_t> pins(cell.nets.size() + 1, 0);
  for (const auto& instance : cell.instances) {
    for (const NetId net : instance.connections) {
      if (net != kNoNet && net < pins.size()) {
        ++pins[net];
      }
    }
  }
  return pins;
}

std::size_t real_port_count(const Cell& cell) {
  return static_cast<std::size_t>(
      std::count_if(cell.ports.begin(), cell.ports.end(), [](const Port& p) { return !p.proxy && !p.name.empty(); }));
}

/// Appends a port `proxy<name>` on `net` (a new net when kNoNet) to `cell`
/// and an unconnected pin to every instance of it.
void add_proxy(NetlistDb& db, CellId id, const std::string& name, NetId net) {
  Cell& cell = db.cells[id];
  const std::string proxy = "proxy" + name;
  if (net == kNoNet) {
    net = cell.add_net(proxy);
  }
  cell.ports.push_back(Port{proxy, net, false});
  for (auto& parent : db.cells) {
    for (auto& instance : parent.instances) {
      if (instance.cell == id) {
        instance.connections.push_back(kNoNet);
      }
    }
  }
}

class PinMatcher {
 public:
  PinMatcher(NetlistDb& db1, CellId c1, NetlistDb& db2, CellId c2, const PinMatchInput& input)
      : db1_(db1), db2_(db2), c1_(c1), c2_(c2), in_(input) {}

  PinMatchResult run(const ColumnWidths& widths) {
    Cell& a = db1_.cells[c1_];
    Cell& b = db2_.cells[c2_];
    if (real_port_count(a) == 0 && real_port_count(b) == 0) {
      out_.result = 2;
      out_.output.push_back(OutputText{"Cells have no pins;  pin matching not needed.\n", false});
      return std::move(out_);
    }
    placeholders(a, b);
    array_equivalence_ = placeholder_pin_equivalence(a, in_.netgen_compat) ||
                         placeholder_pin_equivalence(b, in_.netgen_compat);
    pins1_ = device_pins(a);
    pins2_ = device_pins(b);
    partner_.assign(a.ports.size(), kUnpaired);
    used2_.assign(b.ports.size(), false);
    rows_.assign(a.ports.size(), std::nullopt);
    by_net_class();
    by_name();
    finish(widths);
    return std::move(out_);
  }

 private:
  void placeholders(Cell& a, Cell& b) {
    if (in_.matching != nullptr || in_.blackbox || !(a.flags.placeholder || b.flags.placeholder)) {
      return;
    }
    const Cell& shown = a.flags.placeholder ? a : b;
    out_.output.push_back(OutputText{
        "Warning: Equate pins:  cell " + shown.name + " is a placeholder, treated as a black box.\n", false});
    a.flags.placeholder = true;
    b.flags.placeholder = true;
  }

  [[nodiscard]] bool disconnected(const std::vector<std::size_t>& pins, NetId net) const {
    return net == kNoNet || net >= pins.size() || pins[net] == 0;
  }

  /// Rule 1: ports on matched nets.
  void by_net_class() {
    if (in_.matching == nullptr || in_.forced) {
      return;
    }
    std::map<NetId, NetId> partner_net(in_.matching->nets.begin(), in_.matching->nets.end());
    const Cell& a = db1_.cells[c1_];
    const Cell& b = db2_.cells[c2_];
    for (std::size_t i = 0; i < a.ports.size(); ++i) {
      const auto it = partner_net.find(a.ports[i].net);
      if (a.ports[i].net == kNoNet || it == partner_net.end()) {
        continue;
      }
      const auto j = port_on(b, it->second, a.ports[i].name);
      if (!j) {
        partner_[i] = kNoPinOnNet;
        net2_[i] = it->second;
        rows_[i] = PinRow{a.ports[i].name, "(no pin, node is " + b.net(it->second).name + ")"};
        out_.result = 0;
        continue;
      }
      pair(i, *j);
      if (!names_equal(a.ports[i].name, b.ports[*j].name, in_.case_insensitive)) {
        rows_[i] = PinRow{a.ports[i].name, b.ports[*j].name + " **Mismatch**"};
        out_.result = 0;
      }
    }
  }

  /// The unused port of `b` on `net` with `name`, else the first unused one.
  std::optional<std::size_t> port_on(const Cell& b, NetId net, std::string_view name) const {
    std::optional<std::size_t> first;
    for (std::size_t j = 0; j < b.ports.size(); ++j) {
      if (used2_[j] || b.ports[j].net != net) {
        continue;
      }
      if (names_equal(b.ports[j].name, name, in_.case_insensitive)) {
        return j;
      }
      first = first ? first : j;
    }
    return first;
  }

  void pair(std::size_t i, std::size_t j) {
    partner_[i] = static_cast<int>(j);
    used2_[j] = true;
    rows_[i] = PinRow{db1_.cells[c1_].ports[i].name, db2_.cells[c2_].ports[j].name};
  }

  [[nodiscard]] bool may_pair_by_name(std::size_t i, std::size_t j) const {
    const Cell& a = db1_.cells[c1_];
    const Cell& b = db2_.cells[c2_];
    const bool open1 = disconnected(pins1_, a.ports[i].net);
    const bool open2 = disconnected(pins2_, b.ports[j].net);
    return in_.forced || in_.matching == nullptr || (open1 && open2) ||
           (a.flags.placeholder && b.flags.placeholder);
  }

  [[nodiscard]] bool names_match(std::size_t i, std::size_t j) const {
    const auto& n1 = db1_.cells[c1_].ports[i].name;
    const auto& n2 = db2_.cells[c2_].ports[j].name;
    if (pin_names_equal(n1, n2, in_.case_insensitive, array_equivalence_)) {
      return true;
    }
    // A `proxy` prefix matches a disconnected pin of that name.
    const bool open2 = disconnected(pins2_, db2_.cells[c2_].ports[j].net);
    const bool open1 = disconnected(pins1_, db1_.cells[c1_].ports[i].net);
    return (open2 && n1.starts_with("proxy") && names_equal(n1.substr(5), n2, in_.case_insensitive)) ||
           (open1 && n2.starts_with("proxy") && names_equal(n2.substr(5), n1, in_.case_insensitive));
  }

  /// Rule 2: still-unpaired circuit-1 ports, by name.
  void by_name() {
    const Cell& b = db2_.cells[c2_];
    for (std::size_t i = 0; i < partner_.size(); ++i) {
      if (partner_[i] != kUnpaired) {
        continue;
      }
      for (std::size_t j = 0; j < b.ports.size(); ++j) {
        if (!used2_[j] && names_match(i, j) && may_pair_by_name(i, j)) {
          pair(i, j);
          break;
        }
      }
    }
  }

  /// Rules 3 and 4, the reordering, the port connection errors and the
  /// messages.
  void finish(const ColumnWidths& widths);
  void unpaired_second(std::vector<std::size_t>& appended, std::vector<PinRow>& tail);
  void unpaired_first(std::vector<bool>& drop1);
  void reorder(const std::vector<std::size_t>& appended, const std::vector<bool>& drop1);
  void port_connection_errors();

  NetlistDb& db1_;
  NetlistDb& db2_;
  CellId c1_;
  CellId c2_;
  const PinMatchInput& in_;
  PinMatchResult out_;
  bool array_equivalence_ = false;
  std::vector<std::size_t> pins1_;
  std::vector<std::size_t> pins2_;
  std::vector<int> partner_;
  std::map<std::size_t, NetId> net2_;
  std::vector<bool> used2_;
  std::vector<std::optional<PinRow>> rows_;
};

void PinMatcher::unpaired_second(std::vector<std::size_t>& appended, std::vector<PinRow>& tail) {
  const Cell& b = db2_.cells[c2_];
  for (std::size_t j = 0; j < b.ports.size(); ++j) {
    if (used2_[j] || b.ports[j].proxy) {
      continue;
    }
    if (!disconnected(pins2_, b.ports[j].net)) {
      appended.push_back(j);
      tail.push_back(PinRow{"(no matching pin)", b.ports[j].name});
      out_.result = 0;
      continue;
    }
    // A port emptied by flattening is removed (reorder() leaves it out).
    if (in_.top) {
      tail.push_back(PinRow{"(no matching pin)", b.ports[j].name});
      out_.result = 0;
    }
  }
}

void PinMatcher::unpaired_first(std::vector<bool>& drop1) {
  const Cell& a = db1_.cells[c1_];
  for (std::size_t i = 0; i < partner_.size(); ++i) {
    if (partner_[i] != kUnpaired) {
      continue;
    }
    if (a.ports[i].proxy) {
      drop1[i] = true;  // the proxy port of a port-less cell pairs with nothing
      continue;
    }
    rows_[i] = PinRow{a.ports[i].name, "(no matching pin)"};
    if (disconnected(pins1_, a.ports[i].net)) {
      drop1[i] = true;
    } else {
      out_.result = 0;
    }
  }
}

void PinMatcher::reorder(const std::vector<std::size_t>& appended, const std::vector<bool>& drop1) {
  // Circuit 2's new order: the partner of each kept circuit-1 port (a proxy
  // for one that has none), then the appended circuit-2 ports, each with a
  // proxy in circuit 1.
  std::vector<std::size_t> order;
  for (std::size_t i = 0; i < partner_.size(); ++i) {
    if (drop1[i]) {
      continue;
    }
    if (partner_[i] >= 0) {
      order.push_back(static_cast<std::size_t>(partner_[i]));
      continue;
    }
    const auto net = net2_.count(i) != 0 ? net2_.at(i) : kNoNet;
    add_proxy(db2_, c2_, db1_.cells[c1_].ports[i].name, net);
    order.push_back(db2_.cells[c2_].ports.size() - 1);
  }
  for (const auto j : appended) {
    add_proxy(db1_, c1_, db2_.cells[c2_].ports[j].name, kNoNet);
    order.push_back(j);
  }
  remove_ports(db1_, c1_, drop1);
  // Apply `order` to circuit 2's ports and to the pins of its instances;
  // the circuit-2 ports it leaves out (emptied by flattening, or the proxy
  // of a port-less cell) are dropped.
  const auto permute = [&order](auto& list) {
    std::remove_reference_t<decltype(list)> result;
    result.reserve(order.size());
    for (const auto j : order) {
      result.push_back(j < list.size() ? list[j] : typename std::remove_reference_t<decltype(list)>::value_type{});
    }
    list = std::move(result);
  };
  permute(db2_.cells[c2_].ports);
  for (auto& parent : db2_.cells) {
    for (auto& instance : parent.instances) {
      if (instance.cell == c2_) {
        permute(instance.connections);
      }
    }
  }
}

void PinMatcher::port_connection_errors() {
  if (out_.result != 1 || in_.matching == nullptr) {
    return;
  }
  const Cell& a = db1_.cells[c1_];
  const Cell& b = db2_.cells[c2_];
  const auto ports_on = [](const Cell& cell, NetId net) {
    return static_cast<std::size_t>(
        std::count_if(cell.ports.begin(), cell.ports.end(), [net](const Port& p) { return p.net == net; }));
  };
  const auto net_line = [](const Cell& cell, NetId net, std::size_t ports) {
    return "    " + cell.net(net).name + " (" + std::to_string(cell.file) + ")" + (ports == 0 ? " (no port)" : "") +
           "\n";
  };
  std::string text;
  for (const auto& [n1, n2] : in_.matching->nets) {
    const auto p1 = ports_on(a, n1);
    const auto p2 = ports_on(b, n2);
    if (p1 != p2) {
      text += net_line(a, n1, p1) + net_line(b, n2, p2);
    }
  }
  if (!text.empty()) {
    out_.output.push_back(OutputText{"Port connection errors found:\n" + text, false});
  }
}

void PinMatcher::finish(const ColumnWidths& widths) {
  std::vector<std::size_t> appended;
  std::vector<PinRow> tail;
  unpaired_second(appended, tail);
  std::vector<bool> drop1(partner_.size(), false);
  unpaired_first(drop1);
  std::vector<PinRow> rows;
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i]) {
      rows.push_back(*rows_[i]);
    }
  }
  rows.insert(rows.end(), tail.begin(), tail.end());
  out_.rows = rows;
  const Cell& a = db1_.cells[c1_];
  const Cell& b = db2_.cells[c2_];
  out_.output.push_back(OutputText{pin_table(a.name, b.name, rows, widths), false});
  reorder(appended, drop1);
  port_connection_errors();
  const std::string names = db1_.cells[c1_].name + " and " + db2_.cells[c2_].name;
  out_.output.push_back(OutputText{
      out_.result == 1 ? "Cell pin lists are equivalent.\n" : "Cell pin lists for " + names + " altered to match.\n",
      false});
  for (const auto& port : db1_.cells[c1_].ports) {
    out_.pins1.push_back(port.name);
  }
  for (const auto& port : db2_.cells[c2_].ports) {
    out_.pins2.push_back(port.name);
  }
}

}  // namespace

PinMatchResult match_pins(NetlistDb& db1, CellId cell1, NetlistDb& db2, CellId cell2, const PinMatchInput& input,
                          const ColumnWidths& widths) {
  return PinMatcher{db1, cell1, db2, cell2, input}.run(widths);
}

}  // namespace hefesto::lvs
