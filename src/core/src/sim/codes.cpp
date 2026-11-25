// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "sim" area (see diag/area_codes.hpp), assembled
// from one fragment per subdirectory (openspec/changes/m9-lvs-simulation/design.md D3):
//
//   sim/engine/codes.inc     SIM-ENGINE-*    SIMENG
//   sim/deck/codes.inc       SIM-DECK-*      SIMDECK
//   sim/result/codes.inc     SIM-RESULT-*    SIMRES
//   sim/probe/codes.inc      SIM-PROBE-*     SIMPROBE
//   sim/cmd/codes.inc        SIM-CMD-*       SIMCMD
//   sim/runset/codes.inc     SIM-RUNSET-*    SIMADV
//   sim/meas/codes.inc       SIM-MEAS-*      SIMMEAS
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_SIM_CODE("SIM-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/sim.md with `hefesto-cli --diagnostic-codes=sim`.
//
// Owner: SCAFFOLD (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_SIM_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "sim/engine/codes.inc"
#include "sim/deck/codes.inc"
#include "sim/result/codes.inc"
#include "sim/probe/codes.inc"
#include "sim/cmd/codes.inc"
#include "sim/runset/codes.inc"
#include "sim/meas/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_SIM_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a SIM- code is declared twice");

}  // namespace

std::span<const CodeInfo> sim() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
