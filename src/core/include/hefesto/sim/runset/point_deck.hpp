// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Point decks (capability simulation-sweeps "Point decks"; design D14,
// Q-SIMADV-9 (b), D-22): the deck of each point is the prepared base deck
// with only the corner, parameters, temperature, supply and stimuli, Monte
// Carlo settings and title changed, in that order. Only top-level lines
// change; `.subckt` bodies and `.control` blocks are left as they are (the
// Monte Carlo seed removal and the mismatch parameters excepted). Two points
// with equal level values get identical decks apart from the title line.
//
// Owner: SIMADV (M9 R3, task 15.1).

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/sim/runset/corners.hpp"
#include "hefesto/sim/runset/montecarlo.hpp"

namespace hefesto::sim {

/// The changes of one point.
struct PointChanges {
  std::string job_id;                                 // the title becomes `<title> [<job id>]`
  std::optional<CornerApplication> corner;            // step 1
  /// Section variables of the corner table, to find the `.lib` line each
  /// word belongs to (a line whose word is one of a variable's words).
  std::vector<SectionVariable> variables;
  std::vector<std::pair<std::string, double>> params;  // step 2, `param:<name>` levels
  std::optional<double> temperature;                   // step 3, a `temp` level
  std::optional<double> supply;                        // step 4, a `supply` level (factor k)
  std::vector<std::string> supply_sources;             // the stimuli marked `supply`
  std::vector<std::pair<std::string, double>> sources;  // step 4, `source:<name>` levels
  /// Step 5: the Monte Carlo sample (nullopt: no Monte Carlo dimension).
  std::optional<McApplication> monte_carlo;
  bool nominal = false;                                // the nominal sample
  std::vector<StatParameter> stat_parameters;
  McMode mode = McMode::process;
  std::string dut_subckt;  // where mismatch parameters go; empty: a raw or schematic deck
};

/// The point deck of `base` (its first line is the title). Refusals:
/// SIM-RUNSET-005 `Parameter <name> is defined more than once at top level`,
/// SIM-RUNSET-004 for a `source:` stimulus that is not a top-level V or I
/// line with a DC value, SIM-RUNSET-011 and the warnings of
/// strip_seed_settings, SIM-RUNSET-017 `Mismatch parameter <name> needs a
/// layout or netlist DUT`.
[[nodiscard]] diag::Outcome<std::vector<std::string>> point_deck(const std::vector<std::string>& base,
                                                                const PointChanges& changes,
                                                                std::vector<diag::Diagnostic>& warnings);

/// Q-SIMADV-9 (b): where a top-level `.param` would be inserted, and the
/// index after the last `.lib`/`.include` model line (0 when none).
[[nodiscard]] std::size_t after_last_model_line(const std::vector<std::string>& lines);

}  // namespace hefesto::sim
