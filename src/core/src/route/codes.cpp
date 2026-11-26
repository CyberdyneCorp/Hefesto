// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "route" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m10-plot-route/design.md
// D3):
//
//   route/tech/codes.inc    ROUTE-TECH-*    MZTECH
//   route/maze/codes.inc    ROUTE-MZ-*      MZ
//   route/iroute/codes.inc  ROUTE-IROUTE-*  IROUTE
//   route/diag/codes.inc    ROUTE-DIAG-*    IROUTE
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_ROUTE_CODE("ROUTE-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/route.md with `hefesto-cli --diagnostic-codes=route`.
//
// Owner: SCAFFOLD (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_ROUTE_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "route/tech/codes.inc"
#include "route/maze/codes.inc"
#include "route/iroute/codes.inc"
#include "route/diag/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_ROUTE_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a ROUTE- code is declared twice");

}  // namespace

std::span<const CodeInfo> route() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
