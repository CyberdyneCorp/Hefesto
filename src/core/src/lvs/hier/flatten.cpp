// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Flattening, duplicate pins and global-to-port conversion (lvs-hierarchy
// "Flattening semantics", "Duplicate and shorted pins"; lvs-setup-file
// "global and convert commands"). Instances are rewritten in one pass per
// cell, and nets that a flattened cell shorts are merged with a union-find
// before one renumbering, so the cost stays linear in the cell's size.
//
// Owner: LVSHIER.

#include <algorithm>
#include <cstdint>
#include <numeric>

#include "hefesto/lvs/hier/hierarchy.hpp"
#include "hefesto/lvs/net/expression.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/prop/properties.hpp"

namespace hefesto::lvs {

namespace {

/// Union-find over the nets of one cell; merge() keeps the smaller id.
class NetMerger {
 public:
  explicit NetMerger(std::size_t nets) : parent_(nets + 1) { std::iota(parent_.begin(), parent_.end(), NetId{0}); }

  void grow(std::size_t nets) {
    const auto old = parent_.size();
    parent_.resize(nets + 1);
    std::iota(parent_.begin() + static_cast<std::ptrdiff_t>(old), parent_.end(), static_cast<NetId>(old));
  }

  NetId find(NetId id) {
    while (parent_[id] != id) {
      parent_[id] = parent_[parent_[id]];
      id = parent_[id];
    }
    return id;
  }

  bool merge(NetId a, NetId b) {
    a = find(a);
    b = find(b);
    if (a == b) {
      return false;
    }
    (a < b ? parent_[b] : parent_[a]) = std::min(a, b);
    merged_ = true;
    return true;
  }

  /// Points every port and connection at its root and drops the other nets.
  void apply(Cell& cell) {
    if (!merged_) {
      return;
    }
    const auto root = [this](NetId id) { return id == kNoNet ? kNoNet : find(id); };
    for (auto& port : cell.ports) {
      port.net = root(port.net);
    }
    for (auto& instance : cell.instances) {
      for (auto& connection : instance.connections) {
        connection = root(connection);
      }
    }
    std::vector<bool> keep(cell.nets.size(), false);
    for (std::size_t i = 0; i < cell.nets.size(); ++i) {
      keep[i] = find(static_cast<NetId>(i + 1)) == i + 1;
    }
    cell.compact_nets(keep);
  }

 private:
  std::vector<NetId> parent_;
  bool merged_ = false;
};

/// A property value written as an expression or as a parameter name of the
/// instance is evaluated with the instance's parameters; a value that
/// resolves to a number is stored as that number.
std::string evaluate_value(const std::string& value, const ParameterScope& scope) {
  const bool parameter = scope.instance != nullptr && find_property(*scope.instance, value) != nullptr;
  if (!parameter && !is_expression_text(value)) {
    return value;
  }
  const auto resolved = resolve_value(value, scope);
  return resolved.type == PropertyType::value ? format_g(resolved.number, 17) : value;
}

/// Whether flattening every instance of `klass` (id `klass_id`) in `parent`
/// would leave more than `limit` instances in `parent`
/// (crafted-input limit, both netgen-compat modes).
bool too_large(const Cell& parent, const Cell& klass, CellId klass_id, std::uint64_t limit) {
  const auto count = static_cast<std::uint64_t>(std::count_if(
      parent.instances.begin(), parent.instances.end(), [klass_id](const Instance& i) { return i.cell == klass_id; }));
  const std::uint64_t kept = parent.instances.size() - count;
  const std::uint64_t each = klass.instances.size();
  return each != 0 && count > (limit - std::min(kept, limit)) / each;
}

class Flattener {
 public:
  Flattener(NetlistDb& db, CellId parent, CellId klass)
      : db_(db), parent_(db.cells[parent]), klass_(db.cells[klass]), klass_id_(klass) {}

  std::size_t run() {
    if (&parent_ == &klass_ || klass_.flags.placeholder || !is_flattenable(klass_.device_class) ||
        too_large(parent_, klass_, klass_id_, kMaxDevicesPerRun)) {
      return 0;
    }
    std::vector<Instance> result;
    std::size_t count = 0;
    NetMerger merger{parent_.nets.size()};
    for (auto& instance : parent_.instances) {
      if (instance.cell != klass_id_) {
        result.push_back(std::move(instance));
        continue;
      }
      ++count;
      expand(instance, result, merger);
    }
    parent_.instances = std::move(result);
    merger.apply(parent_);
    return count;
  }

 private:
  /// The parent net of each net of the class for `instance`.
  std::vector<NetId> map_nets(const Instance& instance, NetMerger& merger) {
    std::vector<NetId> map(klass_.nets.size() + 1, kNoNet);
    for (std::size_t p = 0; p < klass_.ports.size(); ++p) {
      const NetId inner = klass_.ports[p].net;
      const NetId outer = p < instance.connections.size() ? instance.connections[p] : kNoNet;
      if (inner == kNoNet || outer == kNoNet) {
        continue;
      }
      if (map[inner] == kNoNet) {
        map[inner] = outer;
      } else {
        merger.merge(map[inner], outer);  // the class shorts two of its ports
      }
    }
    for (std::size_t n = 1; n <= klass_.nets.size(); ++n) {
      if (map[n] != kNoNet) {
        continue;
      }
      const Net& net = klass_.net(static_cast<NetId>(n));
      if (net.global) {
        const NetId id = parent_.net_named(net.name);
        parent_.net(id).global = true;
        map[n] = id;
      } else {
        map[n] = parent_.add_net(instance.name + "/" + net.name);
      }
    }
    merger.grow(parent_.nets.size());
    return map;
  }

  void expand(const Instance& instance, std::vector<Instance>& out, NetMerger& merger) {
    const auto map = map_nets(instance, merger);
    ParameterScope scope;
    scope.instance = instance.properties.empty() ? nullptr : &instance.properties.front();
    scope.parent = &klass_;
    const auto params = db_.parameters.find(klass_.file);
    scope.parameters = params == db_.parameters.end() ? nullptr : &params->second;
    for (const auto& child : klass_.instances) {
      out.push_back(copy_child(instance, child, map, scope));
    }
  }

  /// The copy of the class's instance `child` inside `instance`.
  static Instance copy_child(const Instance& instance, const Instance& child, const std::vector<NetId>& map,
                             const ParameterScope& scope) {
    Instance copy = child;
    copy.name = instance.name + "/" + child.name;
    for (auto& name : copy.merged) {
      name = instance.name + "/" + name;
    }
    for (auto& connection : copy.connections) {
      connection = connection == kNoNet || connection >= map.size() ? kNoNet : map[connection];
    }
    for (auto& record : copy.properties) {
      for (auto& property : record) {
        property.value = evaluate_value(property.value, scope);
      }
    }
    return copy;
  }

  NetlistDb& db_;
  Cell& parent_;
  const Cell& klass_;
  CellId klass_id_;
};

}  // namespace

std::size_t flatten_instances(NetlistDb& db, CellId parent, CellId klass) {
  return Flattener{db, parent, klass}.run();
}

bool flatten_exceeds_limit(const NetlistDb& db, CellId parent, CellId klass, std::uint64_t limit) {
  return parent != klass && too_large(db.cells[parent], db.cells[klass], klass, std::min(limit, kMaxDevicesPerRun));
}

std::size_t flatten_class(NetlistDb& db, CellId klass) {
  std::size_t count = 0;
  for (CellId id = 0; id < db.cells.size(); ++id) {
    count += flatten_instances(db, id, klass);
  }
  return count;
}

void remove_ports(NetlistDb& db, CellId cell, const std::vector<bool>& drop) {
  const auto erase = [&drop](auto& list) {
    std::size_t out = 0;
    for (std::size_t i = 0; i < list.size(); ++i) {
      if (i < drop.size() && drop[i]) {
        continue;
      }
      if (out != i) {
        list[out] = std::move(list[i]);  // never a self-move, which empties a string
      }
      ++out;
    }
    list.resize(out);
  };
  erase(db.cells[cell].ports);
  for (auto& parent : db.cells) {
    for (auto& instance : parent.instances) {
      if (instance.cell == cell) {
        erase(instance.connections);
      }
    }
  }
}

namespace {

/// For each port, the earlier port it duplicates (same net, same name), or
/// itself.
std::vector<std::size_t> duplicate_of(const Cell& cell) {
  std::vector<std::size_t> first(cell.ports.size());
  for (std::size_t j = 0; j < cell.ports.size(); ++j) {
    first[j] = j;
    const Port& b = cell.ports[j];
    for (std::size_t k = 0; k < j && b.net != kNoNet && !b.proxy; ++k) {
      const Port& a = cell.ports[k];
      if (first[k] == k && a.net == b.net && names_equal(a.name, b.name, cell.flags.case_insensitive)) {
        first[j] = k;
        break;
      }
    }
  }
  return first;
}

/// Merges, in every parent, the nets on a dropped pin and on its kept pin.
void merge_parent_nets(NetlistDb& db, CellId cell, const std::vector<std::size_t>& first) {
  for (CellId id = 0; id < db.cells.size(); ++id) {
    Cell& parent = db.cells[id];
    NetMerger merger{parent.nets.size()};
    for (const auto& instance : parent.instances) {
      if (instance.cell != cell) {
        continue;
      }
      for (std::size_t j = 0; j < first.size() && j < instance.connections.size(); ++j) {
        const NetId a = instance.connections[first[j]];
        const NetId b = instance.connections[j];
        if (first[j] != j && a != kNoNet && b != kNoNet) {
          merger.merge(a, b);
        }
      }
    }
    merger.apply(parent);
  }
}

}  // namespace

std::vector<std::string> remove_duplicate_ports(NetlistDb& db, CellId cell) {
  const auto first = duplicate_of(db.cells[cell]);
  std::vector<bool> drop(first.size(), false);
  std::vector<std::string> messages;
  for (std::size_t j = 0; j < first.size(); ++j) {
    if (first[j] != j) {
      drop[j] = true;
      const Cell& c = db.cells[cell];
      messages.push_back("Duplicate pin " + c.ports[j].name + " in cell " + c.name + " (" + std::to_string(c.file) +
                         ")");
    }
  }
  if (!messages.empty()) {
    merge_parent_nets(db, cell, first);
    remove_ports(db, cell, drop);
  }
  return messages;
}

namespace {

/// The global nets of `c` stop being global; those that are no port yet get
/// a new port at the end. Returns the names of the new ports.
std::vector<std::string> globals_to_new_ports(Cell& c) {
  std::vector<std::string> added;
  for (std::size_t i = 0; i < c.nets.size(); ++i) {
    Net& net = c.nets[i];
    if (!net.global) {
      continue;
    }
    net.global = false;
    const auto id = static_cast<NetId>(i + 1);
    if (std::any_of(c.ports.begin(), c.ports.end(), [id](const Port& port) { return port.net == id; })) {
      continue;  // already a port: it only stops being global
    }
    c.ports.push_back(Port{net.name, id, false});
    added.push_back(net.name);
  }
  return added;
}

}  // namespace

void convert_globals(NetlistDb& db, CellId cell) {
  const auto added = globals_to_new_ports(db.cells[cell]);
  if (added.empty()) {
    return;
  }
  for (CellId id = 0; id < db.cells.size(); ++id) {
    if (id == cell) {
      continue;
    }
    Cell& parent = db.cells[id];
    for (auto& instance : parent.instances) {
      if (instance.cell != cell) {
        continue;
      }
      for (const auto& name : added) {
        const NetId net = parent.net_named(name);
        parent.net(net).global = true;
        instance.connections.push_back(net);
      }
    }
  }
}

}  // namespace hefesto::lvs
