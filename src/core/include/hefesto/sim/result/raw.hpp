// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The ngspice raw format, CSV export and waveform measurements as pure
// functions (openspec/changes/m9-lvs-simulation design.md D12, D18;
// capability simulation-analyses-and-results "Final vector retrieval",
// "Raw file format", "CSV export", "Raw file import", "Waveform
// measurements"; Q-SIM-17). ResultStore (result_store.hpp) adds the files.
// Added in M9 R2 (task 13.3).
//
// Owner: SIMRES.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/result/result_store.hpp"

namespace hefesto::sim {

/// Raw import cap (design D18, Q-SIM-25): 4 GiB, and never more than the
/// result budget once loaded.
inline constexpr std::uint64_t kMaxRawImportBytes = std::uint64_t{4} << 30;

/// The display name ngspice writes in raw files: `v(<node>)` for a voltage
/// vector named by its node, `i(<src>)` for `<src>#branch`, else the name.
[[nodiscard]] std::string display_name(const Vector& vector);

struct RawWriteOptions {
  bool binary = true;
  std::string title;    // the testbench title line without its leading `* `
  std::string date;     // C asctime layout without the newline; empty: now (local time)
  std::string command;  // `hefesto-<ver> ngspice-<ver>`; empty: no Command: line
};

/// The plots as one raw file, binary data little-endian (Q-SIM-17).
/// SIM-RESULT-004 when a plot is empty or its vectors differ in length.
[[nodiscard]] diag::Outcome<std::string> render_raw(const std::vector<Plot>& plots, const RawWriteOptions& options);

/// Parses a raw file of one or more plots, binary or ASCII. `name` prefixes
/// the diagnostics (`<name>:<line>: expected "No. Points:"`, `<name>: binary
/// data ends after point K of N`). A file whose data would take more than
/// `budget` bytes once loaded fails (SIM-RESULT-003) before anything is
/// allocated for it.
[[nodiscard]] diag::Outcome<std::vector<Plot>> parse_raw(std::string_view data, std::string_view name,
                                                         std::uint64_t budget = kDesktopResultBudget);

/// "CSV export": the plot's scale and `vectors` (empty: all), complex
/// vectors as re/im columns or, with `dbphase`, db/ph columns.
struct CsvOptions {
  bool dbphase = false;
  std::vector<std::string> vectors;  // names or display names; empty: every vector
};
[[nodiscard]] diag::Outcome<std::string> render_csv(const Plot& plot, const CsvOptions& options = {});

/// "Waveform measurements" (binary64). Arguments by function:
///   value_at x0 | min | max | avg [a b] | rms [a b] | cross level k dir |
///   delay level k dir level2 k2 dir2 (on `vector`, then on `target`, or
///   `vector` again) | risetime lo hi [lo_fraction hi_fraction];
/// dir is 1 rise, -1 fall, 0 either.
[[nodiscard]] diag::Outcome<double> measure(const Plot& plot, const WaveformMeasure& measure);

}  // namespace hefesto::sim
