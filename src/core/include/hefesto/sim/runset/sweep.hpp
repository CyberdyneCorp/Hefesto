// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sweep variables, sweep point generation and design-of-experiments blocks
// (capability simulation-sweeps "Sweep variables", "Sweep point
// generation", "Design of experiments"). Every level is computed directly
// from its index in binary64; level counts are checked against the run-set
// point limit before anything is allocated (design D18).
//
// Owner: SIMADV (M9 R3, task 15.1).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/deck/deck.hpp"

namespace hefesto::sim {

/// "Sweep variables": param:<name>, temp, supply, source:<name>.
struct SweepVariableRef {
  enum class Kind : std::uint8_t { parameter, temperature, supply, source };
  Kind kind = Kind::parameter;
  std::string name;  // the parameter or stimulus name; empty for temp and supply
  /// The variable as written (`param:wn`); also the dimension name.
  [[nodiscard]] std::string text() const;
  /// Two references name the same variable (parameters case-insensitively).
  [[nodiscard]] bool same_as(const SweepVariableRef& other) const;
};

/// Parses a sweep variable; `corner` and malformed names are refused with
/// SIM-RUNSET-004.
[[nodiscard]] diag::Outcome<SweepVariableRef> parse_sweep_variable(std::string_view text);

/// A `source:` variable must name a DC stimulus of the testbench:
/// SIM-RUNSET-004 `Sweep source:<name>: only dc stimuli can be swept; use a
/// parameter` for pulse, sin and pwl, or `... not a stimulus`.
[[nodiscard]] diag::Status check_source_variable(const SweepVariableRef& variable,
                                                 const std::vector<Stimulus>& stimuli);

/// One sweep dimension ("Sweep point generation").
struct SweepDefinition {
  enum class Mode : std::uint8_t { lin_points, lin_step, dec, oct, list };
  std::string variable;
  Mode mode = Mode::list;
  double start = 0.0;
  double stop = 0.0;
  double step = 0.0;          // lin_step
  std::uint32_t count = 0;    // lin_points: points; dec, oct: points per decade or octave
  std::vector<double> values;  // list
};

/// The levels of `sweep`, at most `max_levels` (else SIM-RUNSET-001 before
/// allocation); violations are SIM-RUNSET-003 naming the dimension and the
/// parameter, for example `Sweep param:wn: dec start must be > 0`.
[[nodiscard]] diag::Outcome<std::vector<double>> sweep_levels(const SweepDefinition& sweep, std::size_t max_levels);

/// A level as written into decks: C `%.12g`.
[[nodiscard]] std::string format_level(double value);

/// One factor of a DOE block.
struct DoeFactor {
  std::string variable;
  double lo = 0.0;
  double hi = 1.0;
  bool log = false;
  std::uint32_t levels = 2;  // `full` only
};

struct DoeBlock {
  enum class Method : std::uint8_t { full, oat, lhs, random };
  Method method = Method::full;
  std::vector<DoeFactor> factors;
  std::uint32_t points = 0;  // lhs (n >= 2) and random (n >= 1)
  std::uint64_t seed = 1;    // "The DOE seed SHALL default to 1"
};

/// A DOE factor as `sim doe` writes it, `<var>:<lo>:<hi>[:log][:<levels>]`;
/// SIM-RUNSET-006 with the command's usage line when it is malformed
/// ("Sweep command options", scenario "Unknown factor syntax").
[[nodiscard]] diag::Outcome<DoeFactor> parse_doe_factor(std::string_view text);

/// The rows of `block` (one factor value per column, factor order), at most
/// `max_rows` rows (SIM-RUNSET-001 before allocation); invalid blocks are
/// SIM-RUNSET-006.
[[nodiscard]] diag::Outcome<std::vector<std::vector<double>>> doe_rows(const DoeBlock& block, std::size_t max_rows);

}  // namespace hefesto::sim
