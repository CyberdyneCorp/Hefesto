// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The report data of one compared pair (lvs-reports "Net mismatch report",
// "Device mismatch report", "Structured list result"): mismatch fragments
// with their fanout counts, and the property values of the pairs with
// property errors (Q-LVS-19).
//
// Owner: LVSHIER.

#pragma once

#include <vector>

#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/report/model.hpp"

namespace hefesto::lvs::hier_detail {

/// The two cells of a pair.
struct PairCells {
  const NetlistDb* db1 = nullptr;
  CellId cell1 = 0;
  const NetlistDb* db2 = nullptr;
  CellId cell2 = 0;

  [[nodiscard]] const Cell& first() const { return db1->cells[cell1]; }
  [[nodiscard]] const Cell& second() const { return db2->cells[cell2]; }
};

[[nodiscard]] NetFragment net_fragment(const NetlistDb& db, const Cell& cell, NetId net);
[[nodiscard]] InstanceFragment instance_fragment(const NetlistDb& db, const Cell& cell, const Instance& instance);

/// The illegal net and device classes after a mismatch: the unmatched nets
/// and devices that `verify -list` names (`bad`), one class per circuit
/// side. lvs/cmp does not expose its class partition yet, so all of them
/// form one class (an LVSALG open item; the canonical comparison sorts the
/// fragments of a class, so only the grouping into classes can differ).
[[nodiscard]] std::vector<MismatchClass<NetFragment>> net_classes(const PairCells& cells, const VerifyOutput& bad,
                                                                  const Matching& matching);
[[nodiscard]] std::vector<MismatchClass<InstanceFragment>> device_classes(const PairCells& cells,
                                                                          const VerifyOutput& bad,
                                                                          const Matching& matching);

/// The `properties` entries of the pair's property errors, grouped per
/// device pair in order of first error.
[[nodiscard]] std::vector<PropertyPair> property_pairs(const PairCells& cells,
                                                       const std::vector<PropertyError>& errors);

/// The pin label of port `pin` of `klass`: its name, or `(a|b|...)` for a
/// group of permutable pins.
[[nodiscard]] std::string pin_label(const Cell& klass, std::size_t pin);

}  // namespace hefesto::lvs::hier_detail
