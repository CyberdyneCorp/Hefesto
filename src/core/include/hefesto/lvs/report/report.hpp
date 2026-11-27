// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LVS reports (openspec/changes/m9-lvs-simulation design.md D16, "Area
// contracts" LVSREP; capability lvs-reports): `comp.out` in the two-column
// layout, `comp.json` and the structured list result. Sections whose order
// would come from hash order are written in a deterministic order.
//
// M9 R3 adds (design "As landed (LVSHIER, LVSREP, LVSCMD; M9 R3)"): the
// console text, the JSON file name (Q-LVS-13) and the classification names
// of "Result classification".
//
// Owner: LVSREP.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/hier/run.hpp"

namespace hefesto::lvs {

[[nodiscard]] diag::Outcome<std::string> write_comp_out(const LvsResult& result);
[[nodiscard]] diag::Outcome<std::string> write_json(const LvsResult& result);
/// The `-list` result ("Structured list result").
[[nodiscard]] diag::Outcome<cmd::Value> to_list(const LvsResult& result);

/// What the console shows of a run written with logging on: the
/// console-only text before the final result (echo is off), then all of
/// the final result ("Log and console routing", `lvs` logging sequence).
[[nodiscard]] std::string console_text(const LvsResult& result, bool logging);

/// The JSON file name for the log name `log` (Q-LVS-13): the text from the
/// last `.` of the file name on is replaced by `.json`; a name without one
/// gets `.json` appended.
[[nodiscard]] std::string json_file_name(std::string_view log);

/// The overall status of "Result classification": `clean`, `mismatch`,
/// `error` (input error) or `cancelled`.
enum class OverallStatus : std::uint8_t { clean, mismatch, error, cancelled };
[[nodiscard]] OverallStatus overall_status(const LvsResult& result) noexcept;
[[nodiscard]] std::string_view overall_status_name(OverallStatus status) noexcept;

/// The per-pair status names of "Result classification" (`match`,
/// `match-property-errors`, ..., `mismatch`).
[[nodiscard]] std::string_view pair_status_name(PairStatus status) noexcept;

}  // namespace hefesto::lvs
