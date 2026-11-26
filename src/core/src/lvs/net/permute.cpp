// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Permutable pins of a device class (see permute.hpp).
//
// Owner: LVSREAD (lvs/net); written by LVSALG/LVSRED in M9 R2.

#include "hefesto/lvs/net/permute.hpp"

#include <algorithm>
#include <numeric>

#include "hefesto/lvs/net/names.hpp"

namespace hefesto::lvs {

std::optional<std::uint32_t> port_position(const Cell& klass, std::string_view pin) {
  for (std::size_t i = 0; i < klass.ports.size(); ++i) {
    if (names_equal(klass.ports[i].name, pin, klass.flags.case_insensitive)) {
      return static_cast<std::uint32_t>(i);
    }
  }
  return std::nullopt;
}

PermutePairs permute_pairs(const Cell& klass) {
  PermutePairs result;
  for (const auto& pair : klass.permutes) {
    const auto first = port_position(klass, pair.first);
    const auto second = port_position(klass, pair.second);
    if (!first || !second) {
      result.missing = !first ? pair.first : pair.second;
      result.pairs.clear();
      return result;
    }
    if (*first != *second) {
      result.pairs.emplace_back(*first, *second);
    }
  }
  return result;
}

namespace {

std::uint32_t find_root(std::vector<std::uint32_t>& parent, std::uint32_t i) {
  while (parent[i] != i) {
    parent[i] = parent[parent[i]];
    i = parent[i];
  }
  return i;
}

}  // namespace

std::vector<std::uint32_t> permute_groups(const Cell& klass) {
  std::vector<std::uint32_t> parent(klass.ports.size());
  std::iota(parent.begin(), parent.end(), 0U);
  for (const auto& [a, b] : permute_pairs(klass).pairs) {
    const auto ra = find_root(parent, a);
    const auto rb = find_root(parent, b);
    // The smaller position becomes the root, so the result does not depend
    // on the order of the pairs.
    parent[std::max(ra, rb)] = std::min(ra, rb);
  }
  for (std::uint32_t i = 0; i < parent.size(); ++i) {
    parent[i] = find_root(parent, i);
  }
  return parent;
}

}  // namespace hefesto::lvs
