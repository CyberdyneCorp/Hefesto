// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Device reduction (openspec/changes/m9-lvs-simulation design.md D8, "Area
// contracts" LVSRED; capability lvs-device-reduction): series and parallel
// combination and zero-valued removal of one cell, O(n log n) per class.
//
// Owner: LVSRED.

#pragma once

#include <cstdint>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace hefesto::lvs {

/// The merged counts of "Merged-device counts in the summary".
struct ReductionSummary {
  std::uint64_t parallel_merged = 0;
  std::uint64_t series_merged = 0;
  std::uint64_t zero_valued_removed = 0;
  std::vector<diag::Diagnostic> diagnostics;
};

[[nodiscard]] ReductionSummary reduce(Cell& cell, const SetupState& setup);

}  // namespace hefesto::lvs
