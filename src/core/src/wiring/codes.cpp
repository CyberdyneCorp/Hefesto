// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "wiring" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix WIRE-), then regenerate
// docs/diagnostics/codes/wire.md with `hefesto-cli --diagnostic-codes=wiring`.
// Constants: hefesto/wiring/diag_codes.hpp.
//
// Owner: WIRE (openspec/changes/m2-interactive-editing/design.md D1/D2).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kCodes = {
    CodeInfo{"WIRE-CMD-001", error, "Bad `wire` option or arguments (the help list or the option's syntax follows)."},
    CodeInfo{"WIRE-CMD-002", error, "A layer name given to `wire` does not exist or is ambiguous."},
    CodeInfo{"WIRE-CMD-003", error,
             "No wiring material: none selected yet, none visible under the cursor, or no usable cursor."},
    CodeInfo{"WIRE-CMD-004", error, "The box (or the edit cell) needed to start a leg or place a contact is missing."},
    CodeInfo{"WIRE-CMD-005", error, "No contact can be placed: same layer, no wiring contact, or no routing layer."},
    CodeInfo{"WIRE-CMD-006", warning,
             "`wire increment type` or `wire decrement type` found several routing layers; the first in search "
             "order is used."},
    CodeInfo{"WIRE-FILE-001", error, "The `wire segment` coordinate file cannot be found or opened."},
    CodeInfo{"WIRE-FILE-002", error, "A line of the `wire segment` coordinate file lacks a coordinate."},
    CodeInfo{"WIRE-PATH-001", warning, "A `wire segment` path reverses direction or repeats a point; it is split there."},
    CodeInfo{"WIRE-PATH-002", warning, "A `wire segment` bend exceeds the mitre limit; the route is truncated there."},
    CodeInfo{"WIRE-REPORT-001", info, "A report or note `wire` prints while it runs (Magic's TxPrintf text)."},
    CodeInfo{"WIRE-REPORT-002", warning, "A warning `wire` prints while it still runs (Magic's TxError text)."},
    CodeInfo{"WIRE-TECH-001", warning, "A line of the `wiring` technology section has an error and was ignored."},
};

}  // namespace

std::span<const CodeInfo> wiring() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
