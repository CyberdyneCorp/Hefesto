// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The counts of `extcheck` (capability netlist-extcheck "Counts and report
// format"; openspec/changes/m7-extraction/design.md D15): devVisit,
// capVisit, resistVisit and nodeVisit of extcheck/extcheck.c over a
// flattened netlist, and the report main() prints. netlist::extcheck::run
// (extcheck.hpp) reads and flattens, then calls these.
//
// Ported from Magic 8.3.694 (fc5daa9e) extcheck/extcheck.c, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: SIM (design D3).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hefesto::netlist::extcheck {

/// INFINITE_THRESHOLD and INFINITE_THRESHOLD_F of extflat/EFtypes.h.
inline constexpr std::int64_t kInfiniteResist = 0x7fffffff;
inline constexpr double kInfiniteCap = 1.0E38;

/// The thresholds of one run: 2 fF and 10 ohms unless `-c`, `-C`, `-r`,
/// `-R` change them (Q-NETLIST-36).
struct Thresholds {
  double cap = 2.0;            // EFCapThreshold (fF)
  std::int64_t resist = 10;    // EFResistThreshold (ohms)
};

/// What nodeVisit counts of one flat node.
struct NodeSample {
  double cap = 0.0;          // aF, the value EFVisitNodes passes
  std::int64_t resist = 0;   // mohm, EFNodeResist
  bool global = false;       // EFHNIsGlob of the canonical name
};

/// The flattened netlist as extcheck's visitors see it.
struct CheckInput {
  std::size_t devices = 0;          // EFVisitDevs calls
  std::vector<double> caps;         // coupling capacitors, aF
  std::vector<double> resists;      // explicit resistors, mohm
  std::vector<NodeSample> nodes;    // EFVisitNodes order
};

/// The ec* counters of extcheck.c.
struct Counts {
  int devices = 0;
  int caps = 0;
  int thresh_caps = 0;
  int resists = 0;
  int thresh_resists = 0;
  int nodes = 0;
  int global_nodes = 0;
  int node_caps = 0;
  int node_resists = 0;

  friend bool operator==(const Counts&, const Counts&) = default;
};

/// The visits of main(): every device, the couplings only when the
/// capacitance threshold is finite, the resistors only when the resistance
/// threshold is, and every node.
[[nodiscard]] Counts count(const CheckInput& input, const Thresholds& thresholds);

/// The report of main() after `Memory used: <memory>`, one line each.
[[nodiscard]] std::string report(const Counts& counts, const std::string& memory);

}  // namespace hefesto::netlist::extcheck
