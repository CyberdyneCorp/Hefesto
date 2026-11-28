// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mismatch fragments and property values of a compared pair (lvs-reports
// "Net mismatch report", "Device mismatch report", "Structured list
// result"; Q-LVS-19). Entries are listed in a deterministic order: nets in
// net order, devices in instance order, fanout entries in order of first
// appearance (Q-LVS-2).
//
// Owner: LVSHIER.

#include "lvs/hier/fragments.hpp"

#include <algorithm>
#include <set>
#include <string>

#include "hefesto/lvs/net/expression.hpp"
#include "hefesto/lvs/net/permute.hpp"
#include "hefesto/lvs/prop/properties.hpp"

namespace hefesto::lvs::hier_detail {

namespace {

std::string strip_slash(const std::string& name) {
  return name.starts_with('/') ? name.substr(1) : name;
}

/// Device pins per net of `cell`.
std::vector<std::uint64_t> fanouts(const Cell& cell) {
  std::vector<std::uint64_t> counts(cell.nets.size() + 1, 0);
  for (const auto& instance : cell.instances) {
    for (const NetId net : instance.connections) {
      if (net != kNoNet && net < counts.size()) {
        ++counts[net];
      }
    }
  }
  return counts;
}

}  // namespace

std::string pin_label(const Cell& klass, std::size_t pin) {
  const auto groups = permute_groups(klass);
  if (pin >= groups.size()) {
    return pin < klass.ports.size() ? klass.ports[pin].name : std::string{};
  }
  std::vector<std::size_t> members;
  for (std::size_t p = 0; p < groups.size(); ++p) {
    if (groups[p] == groups[pin]) {
      members.push_back(p);
    }
  }
  if (members.size() < 2) {
    return klass.ports[pin].name;
  }
  std::string label = "(";
  for (const auto p : members) {
    label += (label.size() > 1 ? "|" : "") + klass.ports[p].name;
  }
  return label + ")";
}

NetFragment net_fragment(const NetlistDb& db, const Cell& cell, NetId net) {
  NetFragment fragment;
  fragment.name = cell.net(net).name;
  for (const auto& instance : cell.instances) {
    const Cell& klass = db.cells[instance.cell];
    for (std::size_t p = 0; p < instance.connections.size(); ++p) {
      if (instance.connections[p] != net) {
        continue;
      }
      const std::string label = pin_label(klass, p);
      auto it = std::find_if(fragment.entries.begin(), fragment.entries.end(), [&](const FanoutEntry& e) {
        return e.device_class == klass.name && e.pin == label;
      });
      if (it == fragment.entries.end()) {
        fragment.entries.push_back(FanoutEntry{klass.name, label, 0});
        it = std::prev(fragment.entries.end());
      }
      ++it->count;
    }
  }
  return fragment;
}

namespace {

/// The row of pin `p` and the later pins of its permutation group, each
/// marked done.
PinFanout group_row(const Cell& klass, const Instance& instance, const std::vector<std::uint64_t>& counts,
                    const std::vector<std::uint32_t>& groups, std::size_t p, std::vector<bool>& done) {
  PinFanout row;
  const std::size_t pins = std::min(instance.connections.size(), klass.ports.size());
  for (std::size_t q = p; q < pins; ++q) {
    const bool same_group = q < groups.size() && p < groups.size() && groups[q] == groups[p];
    if (q != p && !same_group) {
      continue;
    }
    done[q] = true;
    const NetId net = instance.connections[q];
    row.pins.push_back(klass.ports[q].name);
    row.fanouts.push_back(net == kNoNet || net >= counts.size() ? 0 : counts[net]);
  }
  return row;
}

}  // namespace

InstanceFragment instance_fragment(const NetlistDb& db, const Cell& cell, const Instance& instance) {
  InstanceFragment fragment;
  fragment.name = strip_slash(instance.name);
  const Cell& klass = db.cells[instance.cell];
  const auto counts = fanouts(cell);
  const auto groups = permute_groups(klass);
  std::vector<bool> done(instance.connections.size(), false);
  for (std::size_t p = 0; p < instance.connections.size() && p < klass.ports.size(); ++p) {
    if (!done[p]) {
      fragment.pins.push_back(group_row(klass, instance, counts, groups, p, done));
    }
  }
  return fragment;
}

namespace {

std::set<std::string> name_set(const std::vector<std::string>& names) {
  return {names.begin(), names.end()};
}

std::vector<NetFragment> unmatched_nets(const NetlistDb& db, const Cell& cell, const std::set<std::string>& bad,
                                        const std::set<NetId>& matched) {
  std::vector<NetFragment> result;
  const auto counts = fanouts(cell);
  for (NetId net = 1; net <= cell.nets.size(); ++net) {
    if (counts[net] == 0 || matched.count(net) != 0 || bad.count(cell.net(net).name) == 0) {
      continue;
    }
    result.push_back(net_fragment(db, cell, net));
  }
  return result;
}

std::vector<InstanceFragment> unmatched_devices(const NetlistDb& db, const Cell& cell,
                                                const std::set<std::string>& bad,
                                                const std::set<std::uint32_t>& matched) {
  std::vector<InstanceFragment> result;
  for (std::uint32_t i = 0; i < cell.instances.size(); ++i) {
    if (matched.count(i) != 0 || bad.count(cell.instances[i].name) == 0) {
      continue;
    }
    result.push_back(instance_fragment(db, cell, cell.instances[i]));
  }
  return result;
}

}  // namespace

std::vector<MismatchClass<NetFragment>> net_classes(const PairCells& cells, const VerifyOutput& bad,
                                                    const Matching& matching) {
  const auto names = name_set(bad.bad_nets);
  std::set<NetId> matched1;
  std::set<NetId> matched2;
  for (const auto& [a, b] : matching.nets) {
    matched1.insert(a);
    matched2.insert(b);
  }
  MismatchClass<NetFragment> cls;
  cls.first = unmatched_nets(*cells.db1, cells.first(), names, matched1);
  cls.second = unmatched_nets(*cells.db2, cells.second(), names, matched2);
  if (cls.first.empty() && cls.second.empty()) {
    return {};
  }
  return {std::move(cls)};
}

std::vector<MismatchClass<InstanceFragment>> device_classes(const PairCells& cells, const VerifyOutput& bad,
                                                            const Matching& matching) {
  const auto names = name_set(bad.bad_elements);
  std::set<std::uint32_t> matched1;
  std::set<std::uint32_t> matched2;
  for (const auto& [a, b] : matching.devices) {
    matched1.insert(a);
    matched2.insert(b);
  }
  MismatchClass<InstanceFragment> cls;
  cls.first = unmatched_devices(*cells.db1, cells.first(), names, matched1);
  cls.second = unmatched_devices(*cells.db2, cells.second(), names, matched2);
  if (cls.first.empty() && cls.second.empty()) {
    return {};
  }
  return {std::move(cls)};
}

namespace {

const Instance* find_instance(const Cell& cell, const std::string& name) {
  for (const auto& instance : cell.instances) {
    if (instance.name == name || strip_slash(instance.name) == name) {
      return &instance;
    }
  }
  return nullptr;
}

/// The value of `key` on `instance` for the structured outputs: as written,
/// or for a value or expression property its number, else
/// `(unresolved expression)` (Q-LVS-19).
PropertyValue value_of(const NetlistDb& db, const Instance* instance, const std::string& key) {
  const Property* property =
      instance == nullptr || instance->properties.empty() ? nullptr : find_property(instance->properties.front(), key);
  if (property == nullptr) {
    return PropertyValue{"(no matching parameter)", "(no value)"};
  }
  const Cell& klass = db.cells[instance->cell];
  const PropertyDef* def = klass.find_property_def(key);
  const bool numeric = def != nullptr && (def->type == PropertyType::value || def->type == PropertyType::expression);
  if (!numeric) {
    return PropertyValue{key, property->value};
  }
  ParameterScope scope;
  scope.instance = &instance->properties.front();
  scope.parent = &klass;
  const auto resolved = resolve_value(property->value, scope);
  return PropertyValue{key, resolved.type == PropertyType::value ? format_g(resolved.number)
                                                                 : std::string{"(unresolved expression)"}};
}

}  // namespace

std::vector<PropertyPair> property_pairs(const PairCells& cells, const std::vector<PropertyError>& errors) {
  std::vector<PropertyPair> pairs;
  for (const auto& error : errors) {
    auto it = std::find_if(pairs.begin(), pairs.end(), [&](const PropertyPair& p) {
      return p.device1 == error.device1 && p.device2 == error.device2;
    });
    if (it == pairs.end()) {
      pairs.push_back(PropertyPair{error.device1, error.device2, {}, {}});
      it = std::prev(pairs.end());
    }
    if (error.property.empty()) {
      continue;
    }
    it->values1.push_back(value_of(*cells.db1, find_instance(cells.first(), error.device1), error.property));
    it->values2.push_back(value_of(*cells.db2, find_instance(cells.second(), error.device2), error.property));
  }
  return pairs;
}

}  // namespace hefesto::lvs::hier_detail
