// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Zero-valued device removal during pre-match (lvs-device-reduction
// "Zero-valued device removal").
//
// Owner: LVSRED.

#include <algorithm>

#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/si_value.hpp"
#include "lvs/reduce/reduce_internal.hpp"

namespace hefesto::lvs {

namespace {

bool removable_class(DeviceClass device_class) {
  return device_class == DeviceClass::resistor || device_class == DeviceClass::res3 ||
         device_class == DeviceClass::vsource || device_class == DeviceClass::isource;
}

bool zero_text(const std::string& text) {
  if (text == "0") {
    return true;
  }
  const auto value = is_si_number(text) ? parse_si_value(text) : std::nullopt;
  return value && *value == 0.0;
}

bool zero_valued(const Instance& instance) {
  if (instance.properties.empty()) {
    return false;
  }
  const auto& record = instance.properties.front();
  for (const char* key : {"value", "length"}) {
    const auto* property = find_property(record, key);
    if (property != nullptr && zero_text(property->value)) {
      return true;
    }
  }
  return false;
}

std::size_t class_count(const NetlistDb& db, const Cell& cell, std::string_view name, bool case_insensitive) {
  return static_cast<std::size_t>(std::count_if(cell.instances.begin(), cell.instances.end(), [&](const Instance& i) {
    return names_equal(db.cells.at(i.cell).name, name, case_insensitive);
  }));
}

/// Replaces `drop` by `keep` in every connection and port of `cell`, then
/// drops the net.
void merge_nets(Cell& cell, NetId keep, NetId drop) {
  for (auto& instance : cell.instances) {
    std::replace(instance.connections.begin(), instance.connections.end(), drop, keep);
  }
  for (auto& port : cell.ports) {
    port.net = port.net == drop ? keep : port.net;
  }
  std::vector<bool> kept(cell.nets.size(), true);
  kept[drop - 1] = false;
  cell.compact_nets(kept);
}

std::string port_name(const Cell& cell, NetId net) {
  for (const auto& port : cell.ports) {
    if (port.net == net) {
      return port.name;
    }
  }
  return {};
}

class ZeroValued {
 public:
  ZeroValued(NetlistDb& db, CellId id, const NetlistDb& other_db, const Cell& other, bool queue_top,
             ReductionSummary& summary)
      : db_(db), id_(id), other_db_(other_db), other_(other), queue_top_(queue_top), summary_(summary) {}

  void run() {
    const bool insensitive = compare_case_insensitive(db_.cells.at(id_), other_);
    std::vector<CellId> classes;
    for (const auto& instance : db_.cells.at(id_).instances) {
      if (std::find(classes.begin(), classes.end(), instance.cell) == classes.end()) {
        classes.push_back(instance.cell);
      }
    }
    for (const CellId klass : classes) {
      const Cell& class_cell = db_.cells.at(klass);
      if (!removable_class(class_cell.device_class) ||
          class_count(db_, db_.cells.at(id_), class_cell.name, insensitive) ==
              class_count(other_db_, other_, class_cell.name, insensitive)) {
        continue;
      }
      remove_class(klass);
    }
  }

 private:
  void remove_class(CellId klass) {
    for (std::size_t i = 0; i < db_.cells.at(id_).instances.size();) {
      const Instance& instance = db_.cells.at(id_).instances[i];
      if (instance.cell != klass || !zero_valued(instance)) {
        ++i;
        continue;
      }
      remove(i);
    }
  }

  void remove(std::size_t index) {
    Cell& cell = db_.cells.at(id_);
    const Instance instance = cell.instances[index];
    const Cell& klass = db_.cells.at(instance.cell);
    cell.instances.erase(cell.instances.begin() + static_cast<std::ptrdiff_t>(index));
    const NetId a = instance.connections.empty() ? kNoNet : instance.connections[0];
    const NetId b = instance.connections.size() < 2 ? kNoNet : instance.connections[1];
    const auto ports = reduce_detail::port_nets(cell);
    const bool a_port = a != kNoNet && ports[a];
    const bool b_port = b != kNoNet && ports[b];
    if (!queue_top_ && klass.device_class != DeviceClass::isource && a_port && b_port && a != b) {
      summary_.log.push_back("Warning: zero-valued device connects port " + port_name(cell, a) +
                             " to another port;  pin matching may be affected.");
    }
    if (a != kNoNet && b != kNoNet && a != b) {
      const bool keep_b = b_port && !a_port;
      merge_nets(cell, keep_b ? b : a, keep_b ? a : b);
    }
    ++summary_.zero_valued_removed;
    summary_.log.push_back("Removing zero-valued device " + klass.name + " from cell " + cell.name + " (" +
                           reduce_detail::file_label(cell) + ") makes a better match");
  }

  NetlistDb& db_;
  CellId id_;
  const NetlistDb& other_db_;
  const Cell& other_;
  bool queue_top_;
  ReductionSummary& summary_;
};

}  // namespace

ReductionSummary remove_zero_valued(NetlistDb& db1, CellId cell1, NetlistDb& db2, CellId cell2, bool queue_top) {
  ReductionSummary summary;
  ZeroValued{db1, cell1, db2, db2.cells.at(cell2), queue_top, summary}.run();
  ZeroValued{db2, cell2, db1, db1.cells.at(cell1), queue_top, summary}.run();
  return summary;
}

}  // namespace hefesto::lvs
