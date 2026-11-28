// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The state of one run_lvs call shared by its steps (lvs-hierarchy
// "Preparing a cell pair", "Pre-match flattening", "lvs per-pair flow",
// "Final result"). Nothing in it outlives the call.
//
// Owner: LVSHIER.

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/lvs/hier/hierarchy.hpp"
#include "hefesto/lvs/hier/run.hpp"

namespace hefesto::lvs::hier_detail {

struct RunState {
  RunState(const RunOptions& run_options, NetlistDb& first, NetlistDb& second, LvsResult& run_result)
      : options(run_options), db1(first), db2(second), result(run_result) {}

  const RunOptions& options;
  NetlistDb& db1;
  NetlistDb& db2;
  LvsResult& result;
  bool case_insensitive = true;
  /// The setup's `equate classes` pairs, then the pairs pin matching equated.
  std::vector<std::pair<std::string, std::string>> equated;
  /// Cells that are never flattened: `flatten prohibit`, `-noflatten`, and
  /// the pairs continued as black boxes.
  std::vector<bool> prohibited1;
  std::vector<bool> prohibited2;
  /// Set when a crafted-input limit ends the run (LVS-HIER-006); the run is
  /// then an input error.
  std::optional<diag::Diagnostic> stop;

  /// True when the run ends now: cancelled, or stopped by a limit.
  [[nodiscard]] bool halted() const { return stop.has_value() || options.cancel.requested(); }

  [[nodiscard]] NetlistDb& db(bool first) const { return first ? db1 : db2; }
  [[nodiscard]] std::vector<bool>& prohibited(bool first) { return first ? prohibited1 : prohibited2; }
  [[nodiscard]] ClassIdentity identity() const { return ClassIdentity{case_insensitive, &equated}; }

  /// A log line (written to the log, echoed when echo is on).
  void log(const std::string& line) { result.output.push_back(OutputText{line + "\n", false}); }
  /// A console-only line.
  void console(const std::string& line) { result.output.push_back(OutputText{line + "\n", true}); }
  /// Text with its own line ends.
  void text(std::string text, bool console_only) {
    result.output.push_back(OutputText{std::move(text), console_only});
  }
};

/// Whether `klass` of circuit `first` may be flattened: a subcircuit that is
/// not a placeholder and not prohibited.
[[nodiscard]] bool may_flatten(const RunState& state, bool first, CellId klass);

/// Flattens the direct instances of `klass` in `parent` of circuit
/// `first`, unless that passes the device limit (flatten_exceeds_limit):
/// then nothing changes, `state.stop` is set and the result is nullopt.
std::optional<std::size_t> flatten_bounded(RunState& state, bool first, CellId parent, CellId klass);

/// "Preparing a cell pair", steps 1 to 5 (duplicate and shorted pins,
/// globals to ports, black boxes, unmatched subcells, pre-match).
void prepare_pair(RunState& state, CellId cell1, CellId cell2, bool top);

/// Step 4: flattens the unmatched subcircuit instances of `cell`.
void flatten_unmatched(RunState& state, bool first, CellId cell);

}  // namespace hefesto::lvs::hier_detail
