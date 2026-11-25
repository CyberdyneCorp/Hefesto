// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "lvs" area (see diag/area_codes.hpp), assembled
// from one fragment per subdirectory (openspec/changes/m9-lvs-simulation/design.md D3):
//
//   lvs/net/codes.inc        LVS-NET-*       LVSREAD
//   lvs/read/codes.inc       LVS-READ-*      LVSREAD
//   lvs/setup/codes.inc      LVS-SETUP-*     LVSSETUP
//   lvs/cmp/codes.inc        LVS-CMP-*       LVSALG
//   lvs/reduce/codes.inc     LVS-REDUCE-*    LVSRED
//   lvs/prop/codes.inc       LVS-PROP-*      LVSPROP
//   lvs/hier/codes.inc       LVS-HIER-*      LVSHIER
//   lvs/report/codes.inc     LVS-REPORT-*    LVSREP
//   lvs/cmd/codes.inc        LVS-CMD-*       LVSCMD
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_LVS_CODE("LVS-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`.
//
// Owner: SCAFFOLD (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_LVS_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "lvs/net/codes.inc"
#include "lvs/read/codes.inc"
#include "lvs/setup/codes.inc"
#include "lvs/cmp/codes.inc"
#include "lvs/reduce/codes.inc"
#include "lvs/prop/codes.inc"
#include "lvs/hier/codes.inc"
#include "lvs/report/codes.inc"
#include "lvs/cmd/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_LVS_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a LVS- code is declared twice");

}  // namespace

std::span<const CodeInfo> lvs() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
