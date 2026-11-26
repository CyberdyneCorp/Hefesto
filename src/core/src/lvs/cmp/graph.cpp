// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The comparison graph, class identities and pin labels
// (lvs-comparison-algorithm "Comparison graph", "Class identity and pin
// labels", "Pseudo-random labels").
//
// Owner: LVSALG.

#include <map>
#include <string>

#include "hefesto/lvs/cmp/codes.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/permute.hpp"
#include "lvs/cmp/state.hpp"

namespace hefesto::lvs {

namespace {

using cmp::kNoVertex;
using cmp::SplitMix64;

/// FNV-1a over the bytes, finished by SplitMix64's mixer: the 64-bit class
/// identity of a (canonical) class name.
std::uint64_t name_hash(std::string_view name) {
  std::uint64_t hash = 0xCBF2'9CE4'8422'2325ULL;
  for (const char c : name) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 0x0000'0100'0000'01B3ULL;
  }
  return SplitMix64::mix(hash);
}

/// The pin label of pin group `group` of a class with `identity`: equal
/// identities give equal labels in both circuits; permutable pins share
/// their group's label.
std::uint64_t pin_label(std::uint64_t identity, std::uint32_t group) {
  return SplitMix64::mix(identity ^ SplitMix64::mix(static_cast<std::uint64_t>(group) + 1U));
}

class GraphBuilder {
 public:
  explicit GraphBuilder(Comparison::State& state) : s_(state), g_(state.graph) {}

  void build() {
    collect_aliases();
    add_nets(*s_.cell1);
    g_.nets1 = static_cast<std::uint32_t>(g_.net_id.size());
    add_nets(*s_.cell2);
    g_.nets = static_cast<std::uint32_t>(g_.net_id.size());
    g_.dev_offset.push_back(0);
    add_devices(*s_.db1, *s_.cell1, 0);
    g_.devices1 = static_cast<std::uint32_t>(g_.dev_instance.size());
    add_devices(*s_.db2, *s_.cell2, 1);
    g_.devices = static_cast<std::uint32_t>(g_.dev_instance.size());
    link_nets();
  }

 private:
  [[nodiscard]] std::string canonical(const std::string& name) const {
    std::string key = s_.case_insensitive ? name_key(name) : name;
    const auto alias = aliases_.find(key);
    return alias != aliases_.end() ? alias->second : key;
  }

  void collect_aliases() {
    for (const auto& [first, second] : s_.options.equated_classes) {
      const std::string a = s_.case_insensitive ? name_key(first) : first;
      const std::string b = s_.case_insensitive ? name_key(second) : second;
      aliases_[b] = a;
    }
  }

  void add_nets(const Cell& cell) {
    std::vector<bool> used(cell.nets.size() + 1, false);
    for (const auto& instance : cell.instances) {
      for (const NetId net : instance.connections) {
        if (net != kNoNet && net < used.size()) {
          used[net] = true;
        }
      }
    }
    std::vector<bool> port(cell.nets.size() + 1, false);
    for (const auto& p : cell.ports) {
      if (p.net != kNoNet && p.net < port.size()) {
        port[p.net] = true;
      }
    }
    auto& vertex = vertices_.emplace_back(cell.nets.size() + 1, kNoVertex);
    for (NetId net = 1; net <= cell.nets.size(); ++net) {
      if (used[net]) {
        vertex[net] = static_cast<std::uint32_t>(g_.net_id.size());
        g_.net_id.push_back(net);
        g_.net_port.push_back(port[net]);
      }
    }
  }

  void add_devices(const NetlistDb& db, const Cell& cell, std::size_t circuit) {
    const auto& vertex = vertices_.at(circuit);
    std::map<CellId, std::vector<std::uint32_t>> groups;
    for (std::uint32_t i = 0; i < cell.instances.size(); ++i) {
      const Instance& instance = cell.instances[i];
      const Cell& klass = db.cells.at(instance.cell);
      const std::string name = canonical(klass.name);
      const auto [kind, added] = kinds_.try_emplace(name, static_cast<std::uint32_t>(kinds_.size()));
      const std::uint64_t identity = name_hash(name);
      auto [group, fresh] = groups.try_emplace(instance.cell);
      if (fresh) {
        group->second = permute_groups(klass);
      }
      for (std::uint32_t p = 0; p < instance.connections.size(); ++p) {
        const NetId net = instance.connections[p];
        g_.dev_net.push_back(net != kNoNet && net < vertex.size() ? vertex[net] : kNoVertex);
        g_.dev_label.push_back(pin_label(identity, p < group->second.size() ? group->second[p] : p));
      }
      g_.dev_offset.push_back(static_cast<std::uint32_t>(g_.dev_net.size()));
      g_.dev_instance.push_back(i);
      g_.dev_identity.push_back(identity);
      g_.dev_kind.push_back(kind->second);
    }
  }

  void link_nets() {
    std::vector<std::uint32_t> count(g_.nets + 1, 0);
    for (const auto n : g_.dev_net) {
      if (n != kNoVertex) {
        ++count[n + 1];
      }
    }
    for (std::uint32_t n = 0; n < g_.nets; ++n) {
      count[n + 1] += count[n];
    }
    g_.net_offset = count;
    g_.net_dev.assign(count[g_.nets], 0);
    g_.net_label.assign(count[g_.nets], 0);
    std::vector<std::uint32_t> fill(count.begin(), count.end() - 1);
    for (std::uint32_t d = 0; d < g_.devices; ++d) {
      for (std::uint32_t p = g_.dev_offset[d]; p < g_.dev_offset[d + 1]; ++p) {
        const auto n = g_.dev_net[p];
        if (n != kNoVertex) {
          g_.net_dev[fill[n]] = d;
          g_.net_label[fill[n]] = g_.dev_label[p];
          ++fill[n];
        }
      }
    }
  }

  Comparison::State& s_;
  cmp::Graph& g_;
  std::map<std::string, std::string> aliases_;
  std::map<std::string, std::uint32_t> kinds_;
  std::vector<std::vector<std::uint32_t>> vertices_;  // per circuit: NetId -> net vertex
};

}  // namespace

void Comparison::State::build() {
  case_insensitive = compare_case_insensitive(*cell1, *cell2);
  GraphBuilder{*this}.build();
  const auto fail = [this](const Cell& cell, std::string_view what) {
    std::string text = "Circuit " + cell.name + " contains no " + std::string{what} + ".";
    log.push_back(LogLine{text, false});
    diagnostics.push_back(diag::make(cmp_codes::kEmptyCircuit, std::move(text)));
  };
  if (graph.devices1 == 0 || graph.devices == graph.devices1) {
    fail(graph.devices1 == 0 ? *cell1 : *cell2, "devices");
    return;
  }
  if (graph.nets1 == 0 || graph.nets == graph.nets1) {
    fail(graph.nets1 == 0 ? *cell1 : *cell2, "nets");
    return;
  }
  initialized = true;
  initial_partition();
}

const Cell& Comparison::State::cell_of_device(std::uint32_t d) const {
  return graph.first_device(d) ? *cell1 : *cell2;
}

const Instance& Comparison::State::instance_of(std::uint32_t d) const {
  return cell_of_device(d).instances[graph.dev_instance[d]];
}

const Cell& Comparison::State::class_of_device(std::uint32_t d) const {
  const NetlistDb& db = graph.first_device(d) ? *db1 : *db2;
  return db.cells.at(instance_of(d).cell);
}

std::string Comparison::State::device_name(std::uint32_t d) const {
  return instance_of(d).name;
}

std::string Comparison::State::net_name(std::uint32_t n) const {
  const Cell& cell = graph.first_net(n) ? *cell1 : *cell2;
  return cell.net(graph.net_id[n]).name;
}

}  // namespace hefesto::lvs
