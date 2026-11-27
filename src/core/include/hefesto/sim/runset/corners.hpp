// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Corner tables (capability simulation-corners "Corner definitions in the
// PDK descriptor", "Bundled corner tables", "User-defined corners and corner
// sets", "Corner dimension"; simulation-netlist-preparation "Corners" for
// the naming rule). The bundled tables are data here until the model
// descriptor of task 13.2 carries them (ModelDescriptor::corners holds the
// names only; INTEGRATION open item).
//
// Owner: SIMADV (M9 R3, task 15.2).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::sim {

/// One section variable of a descriptor: the `.lib <file> <word>` line whose
/// word a corner selects.
struct SectionVariable {
  std::string name;                // `proc`, `mos`, `res`, ...
  std::string file;                // the library file's basename
  std::vector<std::string> words;  // every section word the file defines for it
  bool always_used = true;         // "Bundled corner tables": emitted only when the DUT uses the file
};

enum class CornerKind : std::uint8_t { process, mismatch, statistical };

struct CornerDefinition {
  std::string name;
  /// Variable -> section word; a variable left out (or `-`) takes the
  /// default corner's word.
  std::vector<std::pair<std::string, std::string>> words;
  std::vector<std::pair<std::string, std::string>> params;  // `.param` assignments
  CornerKind kind = CornerKind::process;
  /// User corners only ("User-defined corners and corner sets").
  std::optional<double> temperature;
  std::optional<double> supply;
  /// The statistical counterpart for Monte Carlo `process` and `both`
  /// (simulation-monte-carlo "Monte Carlo modes"); empty: none.
  std::string statistical;
};

struct CornerTuple {
  std::string corner;
  double temperature = 27.0;
  double supply = 1.0;
};

/// A corner set: the cartesian product corners x temperatures x supplies,
/// or an explicit tuple list (user sets).
struct CornerSetDefinition {
  std::string name;
  std::vector<std::string> corners;
  std::vector<double> temperatures;
  std::vector<double> supplies;
  std::vector<CornerTuple> tuples;  // non-empty: used instead of the product
};

struct CornerTable {
  std::string pdk;
  std::vector<SectionVariable> variables;
  std::vector<CornerDefinition> corners;
  std::vector<CornerSetDefinition> sets;
  std::string default_corner;
  std::string default_set;

  [[nodiscard]] const CornerDefinition* corner(std::string_view name) const;
  [[nodiscard]] const SectionVariable* variable(std::string_view name) const;
  [[nodiscard]] const CornerSetDefinition* set(std::string_view name) const;
  /// The corner's word for every variable, in variable order (`-` and
  /// missing variables take the default corner's word).
  [[nodiscard]] std::vector<std::pair<std::string, std::string>> words_of(const CornerDefinition& corner) const;
};

/// The bundled table of sky130A/B, gf180mcuC/D or sg13g2 (also
/// `ihp-sg13g2`); SIM-RUNSET-007 for another PDK.
[[nodiscard]] diag::Outcome<CornerTable> bundled_corner_table(std::string_view pdk);

/// simulation-netlist-preparation "Corners": `<section>_<temp>_<supply>`,
/// the integer temperature prefixed `m` when negative, the scaled supply in
/// `%g` with `.` replaced by `v`.
[[nodiscard]] std::string corner_name(std::string_view section, double temperature, double supply_volts);

/// One level of the corner dimension.
struct CornerLevel {
  std::string name;    // `ss_125_1v62`
  std::string corner;  // `ss`
  double temperature = 27.0;
  double supply = 1.0;  // the supply scale factor
};

/// "Corner dimension": the levels of `set` in order (corner outermost, then
/// temperature, then supply), named with the first supply stimulus'
/// `nominal_supply` volts. Two tuples with one name are refused with
/// `Duplicate corner <name> in corner set <set>`.
[[nodiscard]] diag::Outcome<std::vector<CornerLevel>> expand_corner_set(const CornerTable& table, std::string_view set,
                                                                        double nominal_supply);

/// "User-defined corners and corner sets": the PDK table with the user's
/// corners and sets added; a user corner shadows the PDK corner of its name
/// with the warning SIM-RUNSET-013 `Testbench corner <name> overrides the PDK
/// corner`, and every section word must belong to its variable.
struct UserCorners {
  std::vector<CornerDefinition> corners;
  std::vector<CornerSetDefinition> sets;
};
[[nodiscard]] diag::Outcome<CornerTable> merge_user_corners(const CornerTable& pdk, const UserCorners& user,
                                                            std::vector<diag::Diagnostic>& warnings);

/// `sim corners define`'s check: SIM-RUNSET-008 `sim corners: <var> has no
/// section <word>` for an unknown variable or word.
[[nodiscard]] diag::Status check_corner_words(const CornerTable& table, const CornerDefinition& corner);

/// The section names a library file defines (`.lib <name>` ... `.endl`).
[[nodiscard]] std::vector<std::string> library_sections(std::string_view text);

/// "Corner definitions in the PDK descriptor": every word of every corner
/// must be a section of its variable's file (`sections` maps a file's
/// basename to its sections); else `Corner <name>: section <word> not found
/// in <file>`.
[[nodiscard]] diag::Status check_corner_sections(const CornerTable& table,
                                                 const std::map<std::string, std::vector<std::string>>& sections);

/// What a corner level changes in a point deck (simulation-sweeps "Point
/// decks", step 1).
struct CornerApplication {
  std::vector<std::pair<std::string, std::string>> words;   // variable -> section word
  std::vector<std::pair<std::string, std::string>> params;  // `.param` assignments
  std::optional<double> temperature;
  std::optional<double> supply;
};

[[nodiscard]] diag::Outcome<CornerApplication> corner_application(const CornerTable& table, const CornerLevel& level);

}  // namespace hefesto::sim
