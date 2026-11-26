// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Property checks of a matched device pair (openspec/changes/m9-lvs-simulation
// design.md D8, "Area contracts" LVSPROP; capability lvs-property-matching).
// Values are binary64 parsed by the locale-independent SI parser of
// lvs/net and printed through std::to_chars, so the checks and the report
// text are the same on every platform.
//
// M9 R2 replaces the scaffold's check_properties(const Matching&, ...) with
// a per-pair check the comparison (lvs/cmp) calls for every class that
// holds one device of each circuit ("When properties are checked"). The
// setup commands (lvs/setup, task 7) fill the inputs: the properties of
// interest with their merge rules and pins (Cell::properties), the
// permutable pins (Cell::permutes) and the global topology mode
// (PropertySettings::strict_topology).
//
// Owner: LVSPROP.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"

namespace hefesto::lvs {

/// One entry of "Property error results".
struct PropertyError {
  std::string device1;
  std::string device2;
  std::string property;
  std::string detail;  // the report line, or "property-check-failed" (Q-LVS-10)
};

/// The Q-LVS-10 detail of a check that could not compare.
inline constexpr std::string_view kPropertyCheckFailed = "property-check-failed";

/// What a comparison remembers of its property checks ("Match
/// verification"): none failed, some failed, or a check could not compare.
enum class PropertyState : std::uint8_t { none, errors, failure };

struct PropertySettings {
  bool netgen_compat = true;      // design D5 (Q-LVS-7)
  bool strict_topology = false;   // `property topology strict`
  bool case_insensitive = true;   // the session's name matching (string values, keys)
};

/// A net key per pin that is equal for two pins exactly when their nets
/// correspond (e.g. the net class of the partition); kNoPinKey for a pin
/// on no net. Used for pin-associated properties and permuted no-connects.
inline constexpr std::uint32_t kNoPinKey = 0xFFFF'FFFFU;

/// One side of a device pair.
struct DeviceSide {
  std::string_view name;                 // instance name; printed without a leading '/'
  const Cell* klass = nullptr;           // the device class: properties of interest, permutes
  const Instance* instance = nullptr;    // the property records and pin states
  std::span<const std::uint32_t> pin_keys;  // one per pin, or empty
};

enum class PropertyOutcome : std::uint8_t { match, mismatch, failure };

/// The result of one pair: the outcome, the report lines ("<a> vs. <b>:"
/// and the mismatch lines of "Tolerance rules", "Missing properties" and
/// "Series/parallel network reconciliation"), the console-only lines and one
/// PropertyError per mismatch.
struct PairCheck {
  PropertyOutcome outcome = PropertyOutcome::match;
  std::vector<std::string> report;
  std::vector<std::string> console;
  std::vector<PropertyError> errors;
};

[[nodiscard]] PairCheck check_device_pair(const DeviceSide& first, const DeviceSide& second,
                                          const PropertySettings& settings);

/// Q-LVS-7 (Resolution: compat), the one branch point of the relative
/// tolerance of double and value properties: netgen-compat ON is netgen's
/// 2·|a−b|/(a+b), with its NaN, infinite and negative outcomes; OFF is
/// |a−b| / max(|a|, |b|), 0 when both are zero.
[[nodiscard]] double relative_delta(double a, double b, bool netgen_compat) noexcept;

/// Whether a and b match under tolerance t: ON matches unless the ratio is
/// greater than t (so a NaN ratio, 0/0, is a match, as the spec states);
/// OFF matches when the ratio is at most t.
[[nodiscard]] bool within_relative_tolerance(double a, double b, double t, bool netgen_compat) noexcept;

/// printf's %g and %.3g, locale-independent ("Platform-independent property
/// arithmetic").
[[nodiscard]] std::string format_g(double value, int precision = 6);

}  // namespace hefesto::lvs
