// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMPROBE.

#include "hefesto/sim/probe/net_name_map.hpp"

#include <algorithm>
#include <numeric>
#include <tuple>
#include <utility>

namespace hefesto::sim {

namespace {

bool spice_less(const NetNameEntry& a, const NetNameEntry& b) {
  return std::tie(a.spice_name, a.rnode) < std::tie(b.spice_name, b.rnode);
}

}  // namespace

NetNameMap::NetNameMap(std::vector<NetNameEntry> entries) : entries_(std::move(entries)) {
  std::stable_sort(entries_.begin(), entries_.end(), spice_less);
  by_layout_name_.resize(entries_.size());
  std::iota(by_layout_name_.begin(), by_layout_name_.end(), std::size_t{0});
  std::stable_sort(by_layout_name_.begin(), by_layout_name_.end(), [this](std::size_t a, std::size_t b) {
    return std::tie(entries_[a].layout_name, entries_[a].rnode) < std::tie(entries_[b].layout_name, entries_[b].rnode);
  });
}

const NetNameEntry* NetNameMap::find_layout_name(std::string_view name) const {
  const auto it = std::lower_bound(by_layout_name_.begin(), by_layout_name_.end(), name,
                                   [this](std::size_t index, std::string_view key) {
                                     return entries_[index].layout_name < key;
                                   });
  if (it == by_layout_name_.end() || entries_[*it].layout_name != name) {
    return nullptr;
  }
  return &entries_[*it];
}

const NetNameEntry* NetNameMap::find_spice_name(std::string_view name) const {
  const auto it = std::lower_bound(entries_.begin(), entries_.end(), name,
                                   [](const NetNameEntry& entry, std::string_view key) {
                                     return entry.spice_name < key;
                                   });
  if (it == entries_.end() || it->spice_name != name) {
    return nullptr;
  }
  return &*it;
}

}  // namespace hefesto::sim
