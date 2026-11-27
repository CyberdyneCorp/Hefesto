// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A whole LVS run (openspec/changes/m9-lvs-simulation design.md D8, D10,
// "Area contracts" LVSHIER; capability lvs-hierarchy): the compare queue,
// pre-match flattening, the per-pair flow and the final classification.
// Runs on one background worker; pairs are compared in queue order.
//
// M9 R3 fills the scaffold contract. Additions (design "As landed (LVSHIER,
// LVSREP, LVSCMD; M9 R3)"): RunOptions carries the hierarchy inputs of the
// setup file and of the `lvs` options (equated classes, no-flatten
// patterns, black-boxing, the property and reduction modes, the column
// widths, the log name and the object-reference resolver); LvsResult carries
// the run's output text, one PairReport per compared pair, the circuit-1
// names of the pairs with property errors, the top pair's pin-matching
// result and the input-error flag.
//
// Owner: LVSHIER.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/cmp/compare.hpp"
#include "hefesto/lvs/net/circuit.hpp"
#include "hefesto/lvs/prop/properties.hpp"
#include "hefesto/lvs/report/model.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace hefesto::lvs {

/// The hierarchy depth limit's default and ceiling (design D18, Q-LVS-36).
inline constexpr std::size_t kDefaultHierarchyDepth = 256;
inline constexpr std::size_t kMaxHierarchyDepth = 512;

/// Looks up the layout or schematic place of a net, device or pin of
/// circuit 1 or 2 (from the circuit's name map); nullopt: none.
using ReferenceResolver =
    std::function<std::optional<ObjectReference>(int circuit, ObjectReference::Kind kind, std::string_view name)>;

struct RunOptions {
  bool netgen_compat = true;
  std::uint64_t seed = 0;
  std::size_t hierarchy_depth = kDefaultHierarchyDepth;  // design D18 (Q-LVS-36)
  CancelToken cancel;

  // M9 R3 (additive): the inputs the setup file and the `lvs` options give
  // the hierarchical flow.
  /// `equate classes` pairs (circuit-1 class name, circuit-2 class name).
  std::vector<std::pair<std::string, std::string>> equated_classes;
  /// `-noflatten` entries: anchored patterns matched against each
  /// netlist's cell names ("Black boxes and flattening prohibitions").
  std::vector<std::string> noflatten;
  /// `flatten prohibit` cells of circuit 1 and of circuit 2, by name.
  std::vector<std::string> prohibited1;
  std::vector<std::string> prohibited2;
  /// `model blackbox on` or `lvs -blackbox`.
  bool blackbox = false;
  /// `property topology strict`.
  bool strict_topology = false;
  /// `property parallel open` (true, default) or `connected`.
  bool parallel_open = true;
  ColumnWidths widths;
  /// The log name printed in "Check the end of logfile <log> ...".
  std::string log_name = "comp.out";
  /// Fills PairReport::references; unset for netlist files.
  ReferenceResolver references;
};

/// Progress of a run, called on the run's worker thread.
class ProgressSink {
 public:
  virtual ~ProgressSink() = default;
  /// Pair `index` (0-based) of `count` starts.
  virtual void pair_started(std::string_view cell1, std::string_view cell2, std::size_t index,
                            std::size_t count) = 0;
};

/// A ProgressSink that ignores everything.
class NullProgress final : public ProgressSink {
 public:
  void pair_started(std::string_view /*cell1*/, std::string_view /*cell2*/, std::size_t /*index*/,
                    std::size_t /*count*/) override {}
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

  // M9 R3 (additive).
  /// The run's output in order; the log (`comp.out`) is the text that is
  /// not console-only. The final result starts at `final_output`, where
  /// the `lvs` command turns echo back on.
  std::vector<OutputText> output;
  std::size_t final_output = 0;
  /// One per entry of `pairs`.
  std::vector<PairReport> reports;
  /// Circuit-1 cell names of the pairs with property errors, in queue order.
  std::vector<std::string> property_error_cells;
  /// The top pair's pin-matching result (1, 0 or 2), when it ran.
  std::optional<int> top_pin_result;
  /// The run stopped because an input could not be used (hierarchy depth,
  /// no top cell, a bad setting): status `error`.
  bool input_error = false;
};

/// The `lvs` result codes of a pair (`verify unique` after `run resolve`),
/// mapped to the classification of lvs-reports.
[[nodiscard]] PairStatus status_of_unique(int unique) noexcept;

[[nodiscard]] LvsResult run_lvs(Circuit first, Circuit second, const SetupState& setup, const RunOptions& options,
                                ProgressSink& progress);

}  // namespace hefesto::lvs
