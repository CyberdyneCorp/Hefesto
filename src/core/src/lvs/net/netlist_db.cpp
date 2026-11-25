// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The netlist database (lvs-netlist-input "Netlist database model").
//
// Owner: LVSREAD.

#include "hefesto/lvs/net/netlist_db.hpp"

#include <algorithm>
#include <cstddef>

#include "hefesto/lvs/net/names.hpp"

namespace hefesto::lvs {

const Property* find_property(const PropertyRecord& record, std::string_view key) noexcept {
  for (const auto& property : record) {
    if (names_equal(property.key, key, true)) {
      return &property;
    }
  }
  return nullptr;
}

bool CellFlags::any() const noexcept {
  return top || placeholder || case_insensitive || duplicate || matched || verilog || series_combinable ||
         no_parallel;
}

NetId Cell::find_net(std::string_view net_name) const {
  const auto it = net_index_.find(name_key(net_name));
  if (it == net_index_.end()) {
    return kNoNet;
  }
  for (const NetId id : it->second) {
    if (names_equal(nets[id - 1].name, net_name, flags.case_insensitive)) {
      return id;
    }
  }
  return kNoNet;
}

NetId Cell::add_net(std::string net_name, bool global) {
  const auto id = static_cast<NetId>(nets.size() + 1);
  if (!net_name.empty()) {
    net_index_[name_key(net_name)].push_back(id);
  }
  nets.push_back(Net{std::move(net_name), global});
  return id;
}

NetId Cell::net_named(std::string_view net_name) {
  const NetId found = find_net(net_name);
  return found != kNoNet ? found : add_net(std::string{net_name});
}

std::vector<NetId> Cell::internal_nets() const {
  std::vector<bool> is_port(nets.size(), false);
  for (const auto& port : ports) {
    if (port.net != kNoNet && port.net <= nets.size()) {
      is_port[port.net - 1] = true;
    }
  }
  std::vector<NetId> result;
  for (std::size_t i = 0; i < nets.size(); ++i) {
    if (!is_port[i]) {
      result.push_back(static_cast<NetId>(i + 1));
    }
  }
  return result;
}

void Cell::compact_nets(const std::vector<bool>& keep) {
  std::vector<NetId> renumber(nets.size() + 1, kNoNet);
  std::vector<Net> kept;
  for (std::size_t i = 0; i < nets.size(); ++i) {
    if (i < keep.size() && keep[i]) {
      kept.push_back(std::move(nets[i]));
      renumber[i + 1] = static_cast<NetId>(kept.size());
    }
  }
  const auto map = [&renumber](NetId id) { return id < renumber.size() ? renumber[id] : kNoNet; };
  for (auto& port : ports) {
    port.net = map(port.net);
  }
  for (auto& instance : instances) {
    for (auto& connection : instance.connections) {
      connection = map(connection);
    }
  }
  nets = std::move(kept);
  reindex_nets();
}

void Cell::clear_nets() {
  nets.clear();
  net_index_.clear();
}

const PropertyDef* Cell::find_property_def(std::string_view key) const noexcept {
  for (const auto& definition : properties) {
    if (names_equal(definition.key, key, true)) {
      return &definition;
    }
  }
  return nullptr;
}

void Cell::reindex_nets() {
  net_index_.clear();
  for (std::size_t i = 0; i < nets.size(); ++i) {
    if (!nets[i].name.empty()) {
      net_index_[name_key(nets[i].name)].push_back(static_cast<NetId>(i + 1));
    }
  }
}

CellId NetlistDb::add_cell(Cell cell) {
  const auto id = static_cast<CellId>(cells.size());
  cell_index_[name_key(cell.name)].push_back(id);
  cells.push_back(std::move(cell));
  return id;
}

std::optional<CellId> NetlistDb::find_cell(std::string_view name, FileNumber file) const {
  const auto it = cell_index_.find(name_key(name));
  if (it == cell_index_.end()) {
    return std::nullopt;
  }
  for (auto id = it->second.rbegin(); id != it->second.rend(); ++id) {
    const Cell& cell = cells[*id];
    if (cell.file == file && names_equal(cell.name, name, cell.flags.case_insensitive)) {
      return *id;
    }
  }
  return std::nullopt;
}

std::optional<CellId> NetlistDb::find_cell_exact(std::string_view name, FileNumber file) const {
  const auto it = cell_index_.find(name_key(name));
  if (it == cell_index_.end()) {
    return std::nullopt;
  }
  for (auto id = it->second.rbegin(); id != it->second.rend(); ++id) {
    if (cells[*id].file == file && cells[*id].name == name) {
      return *id;
    }
  }
  return std::nullopt;
}

void NetlistDb::rename_cell(CellId id, std::string name) {
  auto& old_bucket = cell_index_[name_key(cells.at(id).name)];
  std::erase(old_bucket, id);
  auto& new_bucket = cell_index_[name_key(name)];
  new_bucket.push_back(id);
  std::sort(new_bucket.begin(), new_bucket.end());
  cells[id].name = std::move(name);
}

std::uint64_t NetlistDb::instance_count() const noexcept {
  std::uint64_t count = 0;
  for (const auto& cell : cells) {
    count += cell.instances.size();
  }
  return count;
}

bool compare_case_insensitive(const Cell& first, const Cell& second) noexcept {
  return first.flags.case_insensitive && second.flags.case_insensitive;
}

bool placeholder_pin_equivalence(const Cell& cell, bool netgen_compat) noexcept {
  if (netgen_compat) {
    return cell.flags.any();  // netgen's logical test: any flag counts
  }
  return cell.flags.placeholder;
}

}  // namespace hefesto::lvs
