// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device reduction (openspec/changes/m9-lvs-simulation design.md D8, "Area
// contracts" LVSRED; capability lvs-device-reduction): series and parallel
// combination and zero-valued removal of one cell, O(n log n) per pass.
//
// M9 R2 replaces the scaffold's reduce(Cell&, const SetupState&): a cell's
// instances name their class by CellId, so reduction takes the database.
// The setup commands (lvs/setup, task 7) fill the inputs: the class flags
// (CellFlags::no_parallel, series_combinable), the permutable pins
// (Cell::permutes) and the global `property parallel open|connected` mode
// (ReduceOptions::parallel_open).
//
// Owner: LVSRED.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"

namespace hefesto::lvs {

struct ReduceOptions {
  /// Q-LVS-9 (Resolution: compat): ON uses only the first declared
  /// permutable pair for the alternate parallel signature; OFF every
  /// permutation-equivalent pin order.
  bool netgen_compat = true;
  /// `property parallel open` (default) or `connected` (false): whether
  /// no-connect nets count as one net for parallel detection (Q-LVS-26).
  bool parallel_open = true;
  CancelToken cancel;
};

/// The merged counts of "Merged-device counts in the summary" and the
/// messages of the passes, in order.
struct ReductionSummary {
  std::uint64_t parallel_merged = 0;
  std::uint64_t series_merged = 0;
  std::uint64_t zero_valued_removed = 0;
  bool cancelled = false;
  std::vector<std::string> log;  // e.g. "Class inv (0):  Merged 1 parallel devices."
  std::vector<diag::Diagnostic> diagnostics;

  [[nodiscard]] bool modified() const noexcept { return parallel_merged + series_merged > 0; }
};

/// The "Reduction schedule" of `cell`: a parallel pass, then series phases
/// and parallel passes until one merges nothing. Polls the cancel token at
/// least every 65536 devices.
[[nodiscard]] ReductionSummary reduce(NetlistDb& db, CellId cell, const ReduceOptions& options);

/// One parallel pass ("Parallel combination"); returns the devices merged.
std::uint64_t parallel_pass(NetlistDb& db, CellId cell, const ReduceOptions& options, ReductionSummary& summary);

/// One series pass ("Series combination"); returns the devices merged.
std::uint64_t series_pass(NetlistDb& db, CellId cell, const ReduceOptions& options, ReductionSummary& summary);

/// "Zero-valued device removal" during pre-match of a cell pair; `queue_top`
/// is set when the pair is the top of the compare queue.
[[nodiscard]] ReductionSummary remove_zero_valued(NetlistDb& db1, CellId cell1, NetlistDb& db2, CellId cell2,
                                                  bool queue_top);

/// "Shorted ports": declares the different-named ports of `cell` that share
/// a net permutable and returns the console messages.
[[nodiscard]] std::vector<std::string> declare_shorted_ports(NetlistDb& db, CellId cell);

/// One class entry of the subcircuit summary: the total multiplicity
/// before reduction (M) and the devices after it (C).
struct ClassCount {
  std::string name;
  std::uint64_t multiplicity = 0;
  std::uint64_t count = 0;
};

/// The entries of `cell`, in order of first instance.
[[nodiscard]] std::vector<ClassCount> class_counts(const NetlistDb& db, CellId cell);

/// `<class> (<C>)` or `<class> (<M>-><C>)`.
[[nodiscard]] std::string format_class_count(const ClassCount& entry);

}  // namespace hefesto::lvs
