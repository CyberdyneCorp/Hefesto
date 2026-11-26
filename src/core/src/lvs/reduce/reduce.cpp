// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The reduction schedule, merged counts and shared helpers
// (lvs-device-reduction "Reduction schedule", "Merged-device counts in the
// summary", "Permutable pins in matching").
//
// Owner: LVSRED.

#include "hefesto/lvs/reduce/reduce.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <utility>

#include "hefesto/lvs/net/names.hpp"
#include "lvs/reduce/reduce_internal.hpp"

namespace hefesto::lvs {

namespace reduce_detail {

PropertyRecord unit_record() {
  return PropertyRecord{Property{"M", "1"}};
}

namespace {

/// The series marker of an appended record: `+` when it had none, else `S`
/// prepended; the marker is the record's first property.
void mark_series(PropertyRecord& record) {
  for (auto& property : record) {
    if (property.key == "_tag") {
      property.value = "S" + property.value;
      return;
    }
  }
  record.insert(record.begin(), Property{"_tag", "+"});
}

}  // namespace

void append_records(Instance& into, Instance& from, bool series) {
  if (into.properties.empty()) {
    into.properties.push_back(unit_record());
  }
  if (from.properties.empty()) {
    from.properties.push_back(unit_record());
  }
  for (auto& record : from.properties) {
    if (series) {
      mark_series(record);
    }
    into.properties.push_back(std::move(record));
  }
  from.properties.clear();
  into.merged.push_back(std::move(from.name));
  for (auto& name : from.merged) {
    into.merged.push_back(std::move(name));
  }
  from.merged.clear();
}

std::vector<std::uint32_t> net_fanout(const Cell& cell) {
  std::vector<std::uint32_t> fanout(cell.nets.size() + 1, 0);
  for (const auto& instance : cell.instances) {
    for (const NetId net : instance.connections) {
      if (net != kNoNet && net < fanout.size()) {
        ++fanout[net];
      }
    }
  }
  return fanout;
}

std::vector<bool> port_nets(const Cell& cell) {
  std::vector<bool> ports(cell.nets.size() + 1, false);
  for (const auto& port : cell.ports) {
    if (port.net != kNoNet && port.net < ports.size()) {
      ports[port.net] = true;
    }
  }
  return ports;
}

void erase_dead(Cell& cell, const std::vector<bool>& dead) {
  std::size_t out = 0;
  for (std::size_t i = 0; i < cell.instances.size(); ++i) {
    if (!dead[i]) {
      if (out != i) {
        cell.instances[out] = std::move(cell.instances[i]);
      }
      ++out;
    }
  }
  cell.instances.resize(out);
}

std::string file_label(const Cell& cell) {
  return std::to_string(cell.file);
}

}  // namespace reduce_detail

namespace {

/// True when `a` and `b`, in either order, are a declared permutable pair.
bool permute_declared(const Cell& cell, const std::string& a, const std::string& b) {
  const bool fold = cell.flags.case_insensitive;
  return std::any_of(cell.permutes.begin(), cell.permutes.end(), [&](const PinPair& pair) {
    const bool same = names_equal(pair.first, a, fold) && names_equal(pair.second, b, fold);
    const bool swapped = names_equal(pair.first, b, fold) && names_equal(pair.second, a, fold);
    return same || swapped;
  });
}

/// The real ports of `cell` by the net they are on, in port order.
std::map<NetId, std::vector<std::size_t>> ports_by_net(const Cell& cell) {
  std::map<NetId, std::vector<std::size_t>> by_net;
  for (std::size_t i = 0; i < cell.ports.size(); ++i) {
    if (cell.ports[i].net != kNoNet && !cell.ports[i].proxy) {
      by_net[cell.ports[i].net].push_back(i);
    }
  }
  return by_net;
}

}  // namespace

ReductionSummary reduce(NetlistDb& db, CellId cell, const ReduceOptions& options) {
  ReductionSummary summary;
  parallel_pass(db, cell, options, summary);
  while (!summary.cancelled) {
    std::uint64_t phase = 0;
    for (;;) {
      const std::uint64_t merged = series_pass(db, cell, options, summary);
      phase += merged;
      if (merged == 0 || summary.cancelled) {
        break;
      }
    }
    if (phase == 0 || summary.cancelled || parallel_pass(db, cell, options, summary) == 0) {
      break;
    }
  }
  return summary;
}

std::vector<std::string> declare_shorted_ports(NetlistDb& db, CellId id) {
  Cell& cell = db.cells.at(id);
  std::vector<std::string> messages;
  for (const auto& [net, ports] : ports_by_net(cell)) {
    for (std::size_t k = 1; k < ports.size(); ++k) {
      const std::string& a = cell.ports[ports[k - 1]].name;
      const std::string& b = cell.ports[ports[k]].name;
      if (names_equal(a, b, cell.flags.case_insensitive) || permute_declared(cell, a, b)) {
        continue;
      }
      cell.permutes.push_back(PinPair{a, b});
      messages.push_back("Pins " + a + " and " + b + " are shorted in cell " + cell.name + " (" +
                         reduce_detail::file_label(cell) + ")");
    }
  }
  return messages;
}

std::vector<ClassCount> class_counts(const NetlistDb& db, CellId id) {
  const Cell& cell = db.cells.at(id);
  std::vector<ClassCount> entries;
  std::map<CellId, std::size_t> index;
  for (const auto& instance : cell.instances) {
    const auto [it, added] = index.try_emplace(instance.cell, entries.size());
    if (added) {
      entries.push_back(ClassCount{db.cells.at(instance.cell).name, 0, 0});
    }
    ClassCount& entry = entries[it->second];
    ++entry.count;
    if (instance.properties.empty()) {
      ++entry.multiplicity;
    }
    for (const auto& record : instance.properties) {
      const auto* m = find_property(record, "M");
      std::uint64_t value = 1;
      if (m != nullptr) {
        const auto* end = m->value.data() + m->value.size();
        if (std::from_chars(m->value.data(), end, value).ptr != end) {
          value = 1;
        }
      }
      entry.multiplicity += value;
    }
  }
  return entries;
}

std::string format_class_count(const ClassCount& entry) {
  if (entry.multiplicity == entry.count) {
    return entry.name + " (" + std::to_string(entry.count) + ")";
  }
  return entry.name + " (" + std::to_string(entry.multiplicity) + "->" + std::to_string(entry.count) + ")";
}

}  // namespace hefesto::lvs
