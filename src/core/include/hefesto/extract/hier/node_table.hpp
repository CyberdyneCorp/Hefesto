// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// HierNodeTable: the connection table of one cell's hierarchical
// extraction (Magic's ha_connHash with its Node and NodeName records;
// capability extract-hierarchy "Name unification order", "Merge records
// and pass-2 adjustments"). Names are hashed in a util::MagicHashTable of
// 32 buckets (HashInit(&ha_connHash, 32, 0)), so write() emits the `merge`
// groups in Magic's hash order (Q-EXTRACT-7).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtHier.c (extHierNewNode,
// the unification of extHierConnectFunc2/3 and extHierSubstrate,
// extOutputConns); Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/extract/file/writer.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::extract {

/// A hierarchical node: its names in unification order and the
/// adjustments of pass 2 (capacitance in attofarads, per-class area and
/// perimeter).
struct HierNode {
  std::vector<std::size_t> names;  // indices into HierNodeTable::name()
  double cap = 0.0;
  std::vector<ClassArea> classes;
};

class HierNodeTable {
 public:
  /// `classes`: the style's number of resistance classes.
  explicit HierNodeTable(std::size_t classes);

  /// HashFind then extHierNewNode: the node of `name`, a new node of size
  /// 1 when the name is not yet in the table.
  std::size_t node_of(const std::string& name);
  /// HashLookOnly: the node of `name` if the table has it.
  [[nodiscard]] std::optional<std::size_t> find(const std::string& name) const;

  /// Unifies `cumulative` (the node found on the cumulative side) with
  /// `added` (the new subtree's): when the first has fewer names its names
  /// go after the first name of the second, otherwise the names of the
  /// second go before those of the first. Returns the surviving node.
  /// Magic keeps only the survivor's adjustments; `sum_values` (proposed
  /// Q-EXTRACT-40, magic-compat OFF) adds the other node's to it.
  std::size_t unify(std::size_t cumulative, std::size_t added, bool sum_values);

  [[nodiscard]] HierNode& node(std::size_t index) { return nodes_[index]; }
  [[nodiscard]] const HierNode& node(std::size_t index) const { return nodes_[index]; }
  [[nodiscard]] const std::string& name(std::size_t index) const { return names_[index].text; }

  /// extOutputConns: for each name in hash order whose node was not yet
  /// written, `merge "n0" "n1" C a0 p0 ...` (C = cap / cscale) and
  /// `merge "ni" "ni+1"` for the rest of its names; a node of one name
  /// writes nothing.
  void write(ExtWriter& writer, MergeClass cls, double cscale) const;

 private:
  struct Name {
    std::string text;
    std::size_t node = 0;
  };

  util::MagicHashTable<std::size_t> table_{32};
  std::vector<Name> names_;
  std::vector<HierNode> nodes_;
  std::size_t classes_ = 0;
};

}  // namespace hefesto::extract
