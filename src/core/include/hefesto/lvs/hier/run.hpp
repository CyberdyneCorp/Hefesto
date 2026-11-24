// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A whole LVS run (openspec/changes/m9-lvs-simulation design.md D8, D10,
// "Area contracts" LVSHIER; capability lvs-hierarchy): the compare queue,
// pre-match flattening, the per-pair flow and the final classification.
// Runs on one background worker; pairs are compared in queue order.
//
// Owner: LVSHIER.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/circuit.hpp"
#include "hefesto/lvs/prop/properties.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace hefesto::lvs {

struct RunOptions {
  bool netgen_compat = true;
  std::uint64_t seed = 0;
  std::size_t hierarchy_depth = 256;  // design D18 (Q-LVS-36)
  CancelToken cancel;
};

/// Progress of a run, called on the run's worker thread.
class ProgressSink {
 public:
  virtual ~ProgressSink() = default;
  /// Pair `index` (0-based) of `count` starts.
  virtual void pair_started(std::string_view cell1, std::string_view cell2, std::size_t index,
                            std::size_t count) = 0;
};

/// The structured result of "Final result" and lvs-reports "Result
/// classification".
struct LvsResult {
  std::vector<std::pair<std::string, std::string>> queue;  // the compare queue, in order
  std::vector<PairResult> pairs;                           // one per compared pair, in queue order
  std::vector<PropertyError> property_errors;
  std::vector<std::string> failed_subcells;
  bool top_pin_mismatch = false;
  bool clean = false;
  bool cancelled = false;
  bool netgen_compat = true;  // the setting's value for this run (design D5)
  std::vector<diag::Diagnostic> diagnostics;
};

[[nodiscard]] LvsResult run_lvs(Circuit first, Circuit second, const SetupState& setup, const RunOptions& options,
                                ProgressSink& progress);

}  // namespace hefesto::lvs
