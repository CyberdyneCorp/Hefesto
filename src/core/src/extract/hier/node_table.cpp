// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// HierNodeTable (extract-hierarchy "Name unification order", "Merge records
// and pass-2 adjustments"). Ported from Magic 8.3.694 (fc5daa9e)
// extract/ExtHier.c (extHierNewNode, extOutputConns and the unification in
// extHierConnectFunc2); Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include "hefesto/extract/hier/node_table.hpp"

#include <cstdint>
#include <iterator>

namespace hefesto::extract {

HierNodeTable::HierNodeTable(std::size_t classes) : classes_(classes) {}

std::size_t HierNodeTable::node_of(const std::string& name) {
  auto& entry = table_.find_or_insert(name);
  if (entry.value) {
    return names_[*entry.value].node;
  }
  const std::size_t name_index = names_.size();
  const std::size_t node_index = nodes_.size();
  entry.value = name_index;
  names_.push_back(Name{name, node_index});
  HierNode node;
  node.names.push_back(name_index);
  node.classes.resize(classes_);
  nodes_.push_back(std::move(node));
  return node_index;
}

std::optional<std::size_t> HierNodeTable::find(const std::string& name) const {
  const auto* entry = table_.find(name);
  if (entry == nullptr || !entry->value) {
    return std::nullopt;
  }
  return names_[*entry->value].node;
}

std::size_t HierNodeTable::unify(std::size_t cumulative, std::size_t added, bool sum_values) {
  if (cumulative == added) {
    return cumulative;
  }
  HierNode& n1 = nodes_[cumulative];
  HierNode& n2 = nodes_[added];
  const bool to_added = n1.names.size() < n2.names.size();
  HierNode& keep = to_added ? n2 : n1;
  HierNode& gone = to_added ? n1 : n2;
  const std::size_t survivor = to_added ? added : cumulative;
  for (const std::size_t n : gone.names) {
    names_[n].node = survivor;
  }
  if (to_added) {
    // n1's names go after the first name of n2.
    keep.names.insert(std::next(keep.names.begin()), gone.names.begin(), gone.names.end());
  } else {
    // n2's names go before n1's.
    keep.names.insert(keep.names.begin(), gone.names.begin(), gone.names.end());
  }
  if (sum_values) {
    keep.cap += gone.cap;
    for (std::size_t c = 0; c < keep.classes.size() && c < gone.classes.size(); ++c) {
      keep.classes[c].area += gone.classes[c].area;
      keep.classes[c].perimeter += gone.classes[c].perimeter;
    }
  }
  gone.names.clear();
  gone.cap = 0.0;
  return survivor;
}

void HierNodeTable::write(ExtWriter& writer, MergeClass cls, double cscale) const {
  std::vector<bool> written(nodes_.size(), false);
  table_.for_each([&](const auto& entry) {
    if (!entry.value) {
      return;
    }
    const std::size_t index = names_[*entry.value].node;
    if (written[index]) {
      return;
    }
    written[index] = true;
    const HierNode& node = nodes_[index];
    if (node.names.size() < 2) {
      return;
    }
    std::vector<std::int64_t> pairs;
    for (const ClassArea& c : node.classes) {
      pairs.push_back(c.area);
      pairs.push_back(c.perimeter);
    }
    writer.merge(names_[node.names[0]].text, names_[node.names[1]].text, node.cap / cscale, pairs, cls);
    for (std::size_t i = 1; i + 1 < node.names.size(); ++i) {
      writer.merge_more(names_[node.names[i]].text, names_[node.names[i + 1]].text, cls);
    }
  });
}

}  // namespace hefesto::extract
