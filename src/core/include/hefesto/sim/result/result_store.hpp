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
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
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
/// Added in the M9 R3 fix pass: result memory per job on iPadOS (512 MiB;
/// scenario "Default result budget", Q-SIM-16).
inline constexpr std::uint64_t kIpadResultBudget = std::uint64_t{512} << 20;

/// Added in the M9 R3 fix pass: the default result budget of a platform.
[[nodiscard]] constexpr std::uint64_t default_result_budget(bool ipados) noexcept {
  return ipados ? kIpadResultBudget : kDesktopResultBudget;
}

/// Added in the M9 R3 fix pass: the default result budget of this build
/// (iPadOS builds define TARGET_OS_IOS through TargetConditionals.h).
#if defined(__APPLE__) && TARGET_OS_IOS
inline constexpr std::uint64_t kPlatformResultBudget = default_result_budget(true);
#else
inline constexpr std::uint64_t kPlatformResultBudget = default_result_budget(false);
#endif

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
  /// Added in M9 R2: `delay`'s target vector (nullopt: `vector`).
  std::optional<std::string> target;
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
