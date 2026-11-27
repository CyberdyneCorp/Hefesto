// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Monte Carlo (capability simulation-monte-carlo "Monte Carlo modes",
// "Monte Carlo dimension", "Sample seeds", "Failed samples and early
// stopping", "User statistical parameters", "Sample count limit"). Hefesto
// models no variation itself: a sample is a deck whose section words and
// `.param` switches select the PDK's statistical models, loaded with the
// sample's own seed into a freshly reset circuit (design D9, D14).
//
// Owner: SIMADV (M9 R3, task 15.3).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/runset/corners.hpp"

namespace hefesto::sim {

enum class McMode : std::uint8_t { process, mismatch, both };

[[nodiscard]] std::string_view mc_mode_name(McMode mode) noexcept;

/// "Monte Carlo dimension".
struct MonteCarloSpec {
  McMode mode = McMode::process;
  std::uint32_t runs = 100;       // N, 1..100 000
  std::uint32_t base_seed = 1;    // B, 1..2 147 483 647
  bool nominal = true;
  std::optional<double> stop;     // early-stop half-width target h
  bool exclude_failed = false;    // failedSamples = exclude
};

/// "Sample count limit" (Q-SIM-25): lowerable setting, default 100 000.
inline constexpr std::uint32_t kDefaultMaxSamples = 100'000;

/// SIM-RUNSET-010 `sim mc: runs must be between 1 and <limit>` or the seed
/// range; `max_samples` is the (possibly lowered) limit.
[[nodiscard]] diag::Status check_monte_carlo(const MonteCarloSpec& spec, std::uint32_t max_samples = kDefaultMaxSamples);

/// One level of the Monte Carlo dimension: the nominal sample or sample i.
struct McLevel {
  bool nominal = false;
  std::uint32_t index = 0;  // the sample index i (0 for the nominal level)
  [[nodiscard]] std::string label() const;  // `nom` or the index
  [[nodiscard]] std::uint32_t seed(std::uint32_t base_seed) const;
};

[[nodiscard]] std::vector<McLevel> mc_levels(const MonteCarloSpec& spec);

/// What a sample changes in its point's deck (simulation-sweeps "Point
/// decks", step 5): section words replaced, and `.param` lines placed
/// directly after the last model line.
struct McApplication {
  std::vector<std::pair<std::string, std::string>> words;
  std::vector<std::string> params_after_models;  // whole lines
};

/// The bundled mode table of "Monte Carlo modes" for `table`'s PDK at
/// process corner `corner`. Refusals (SIM-RUNSET-009): `<PDK> does not
/// define Monte Carlo mode <mode>`, `Monte Carlo mode <mode> is not defined
/// at corner <c>` (Q-SIMADV-8 (a)).
[[nodiscard]] diag::Outcome<McApplication> mc_application(const CornerTable& table, McMode mode,
                                                          std::string_view corner);

/// "User statistical parameters".
struct StatParameter {
  enum class Distribution : std::uint8_t { agauss, gauss, aunif, unif, limit };
  std::string name;
  Distribution distribution = Distribution::agauss;
  std::vector<double> arguments;  // nom abs sigma | nom rel sigma | nom abs | nom rel | nom abs
  bool mismatch = false;
};

/// The `.param` line of `p` for a sample, or `.param <name>=<nom>` for the
/// nominal sample and for a parameter the mode does not vary. SIM-RUNSET-010
/// for a wrong argument count.
[[nodiscard]] diag::Outcome<std::string> stat_parameter_line(const StatParameter& p, McMode mode, bool nominal);

/// "Sample seeds": removes every top-level `.option(s)` `seed=` assignment and
/// every `setseed` / `set rndseed` line of `.control` blocks, each with the
/// warning SIM-RUNSET-012 `Removed seed setting on line <n>; Monte Carlo
/// seeds are set by Hefesto` (1-based line numbers of `lines`); refuses
/// `stat_param_as_func` and `statlocal` (SIM-RUNSET-011 `<variable> is not
/// supported in Monte Carlo runs`).
[[nodiscard]] diag::Outcome<std::vector<std::string>> strip_seed_settings(const std::vector<std::string>& lines,
                                                                         std::vector<diag::Diagnostic>& warnings);

/// sg13g2 `mismatch`: SIM-RUNSET-014 `Monte Carlo mismatch: instance <name>
/// has mm_ok=0; it will not vary` for each subcircuit instance of a PDK
/// device (`sg13_`/`rppd`/`rsil`/`rhigh`/`cap_`) whose `mm_ok` is absent or 0.
[[nodiscard]] std::vector<diag::Diagnostic> mm_ok_warnings(const std::vector<std::string>& dut_lines);

/// The 95 % Wilson half-width of yield k/m (simulation-measurements-and-
/// statistics "Yield"); nullopt for m = 0.
[[nodiscard]] std::optional<double> wilson_half_width(std::size_t passes, std::size_t counted);

/// One sample's outcome for early stopping, by sample index: nullopt while
/// not final; else whether it passed, and whether it failed (state `failed`).
struct SampleOutcome {
  bool final = false;
  bool pass = false;
  bool failed = false;
};

/// "Failed samples and early stopping": the first checkpoint m = 50, 100,
/// ... (m <= samples.size()) at which samples 0..m-1 are all final and the
/// half-width is <= h; nullopt otherwise. Failed samples count as failing
/// unless `exclude_failed`.
[[nodiscard]] std::optional<std::size_t> early_stop(const std::vector<SampleOutcome>& samples, double h,
                                                    bool exclude_failed);

}  // namespace hefesto::sim
