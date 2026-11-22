// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Capacitance and lumped resistance (capabilities extract-capacitance,
// extract-resistance-lumped; openspec/changes/m7-extraction/design.md D9).
// Substrate, overlap, sidewall, sideoverlap (legacy and fringe-halo) and
// corner capacitance accumulate into a CouplingTable, which is keyed by
// node index while extracting and by name only at output (Q-EXTRACT-7), so
// no address order leaks into the file. Each compat split is one named
// function in cap/compat_branches.cpp (D9).
//
// Scaffold state (tasks.md 3.3): every accumulator reports CORE-NOTIMPL-008
// and the pure functions answer 0 until EXTCAP lands them (tasks.md
// 6.1-6.7).
//
// Owner: INTEGRATION (this header); EXTCAP (the implementation).

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/extract/node/regions.hpp"
#include "hefesto/extract/tech/style.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::extract {

/// Coupling capacitance between pairs of nodes (by index into the cell's
/// NodeRegionList, smaller index first), in attofarads.
class CouplingTable {
 public:
  void add(std::size_t a, std::size_t b, double cap) {
    if (b < a) {
      std::swap(a, b);
    }
    pairs_[{a, b}] += cap;
  }
  [[nodiscard]] const std::map<std::pair<std::size_t, std::size_t>, double>& pairs() const noexcept { return pairs_; }

 private:
  std::map<std::pair<std::size_t, std::size_t>, double> pairs_;
};

/// The inputs every accumulator shares.
struct CapInputs {
  const db::CellDef* cell = nullptr;
  const ExtStyle* style = nullptr;
  const compat::CompatSettings* compat = nullptr;
};

/// Area and perimeter capacitance to substrate, into each node's
/// substrate_cap.
[[nodiscard]] diag::Status substrate_caps(NodeRegionList& nodes, const CapInputs& in);
/// Overlap coupling with shielding.
[[nodiscard]] diag::Status overlap_caps(const NodeRegionList& nodes, const CapInputs& in, CouplingTable& out);
/// Sidewall coupling (Q-EXTRACT-20, single-precision denominator).
[[nodiscard]] diag::Status sidewall_caps(const NodeRegionList& nodes, const CapInputs& in, CouplingTable& out);
/// Sideoverlap with the given fringe model (Q-EXTRACT-17, Q-EXTRACT-18).
[[nodiscard]] diag::Status sideoverlap_caps(NodeRegionList& nodes, const CapInputs& in, FringeModel model,
                                            CouplingTable& out);
/// Corner fringe to substrate and corner coupling (Q-EXTRACT-24).
[[nodiscard]] diag::Status corner_caps(NodeRegionList& nodes, const CapInputs& in, double factor,
                                       CouplingTable& out);

/// The lumped resistance of a node in milliohms (ExtBasic.c:585-645).
[[nodiscard]] std::int64_t lumped_resistance(const NodeRegion& node, const ExtStyle& style);

/// 0.6366 * atan(mult * d) with Magic's bits on every platform (Open
/// Question 3, design D22).
[[nodiscard]] double fringe_fraction(double mult, double d);

}  // namespace hefesto::extract
