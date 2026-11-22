// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "netlist" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m7-extraction/design.md D2):
//
//   netlist/spice/codes.inc     NETLIST-SPICE-*     SPICE
//   netlist/sim/codes.inc       NETLIST-SIM-*       SIM
//   netlist/extcheck/codes.inc  NETLIST-CHECK-*     SIM
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_NETLIST_CODE("NETLIST-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/netlist.md with `hefesto-cli --diagnostic-codes=netlist`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_NETLIST_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "netlist/spice/codes.inc"
#include "netlist/sim/codes.inc"
#include "netlist/extcheck/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_NETLIST_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a NETLIST- code is declared twice");

}  // namespace

std::span<const CodeInfo> netlist() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
