// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// sim::ResultStore: stored simulation results (openspec/changes/
// m9-lvs-simulation design.md D12, D18, "Area contracts" SIMRES; capability
// simulation-analyses-and-results "Result storage", "Raw file format", "CSV
// export", "Raw file import", "Waveform measurements"). Results live per job
// under the project's `sim/` folder; raw files are always little-endian.
//
// Owner: SIMRES.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/job.hpp"

namespace hefesto::sim {

/// Result memory per job (design D18, Q-SIM-16): 4 GiB on desktop.
inline constexpr std::uint64_t kDesktopResultBudget = std::uint64_t{4} << 30;

/// One plot: the scale vector first, then the others.
struct Plot {
  std::string name;  // e.g. "tran1"
  std::vector<Vector> vectors;
};

/// One measurement of "Waveform measurements": `function` is value_at, min,
/// max, avg, rms, cross, delay or risetime; `arguments` its numbers in the
/// order of the table.
struct WaveformMeasure {
  std::string function;
  std::string vector;
  std::vector<double> arguments;
  std::optional<std::string> part;  // re, im, db or ph for a complex vector
};

struct ResultStore {
  platform::FileSystem* files = nullptr;  // required by every member function
  platform::Path root;                    // the project's sim/ folder

  [[nodiscard]] diag::Status write_raw(const JobId& job, const Plot& plot) const;
  [[nodiscard]] diag::Outcome<Plot> read_raw(const platform::Path& path) const;
  [[nodiscard]] diag::Status export_csv(const Plot& plot, const platform::Path& path) const;
  [[nodiscard]] diag::Outcome<double> measure_waveform(const Plot& plot, const WaveformMeasure& measure) const;
};

}  // namespace hefesto::sim
