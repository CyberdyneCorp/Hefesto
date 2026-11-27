// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "plot" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m10-plot-route/design.md
// D3):
//
//   plot/tech/codes.inc    PLOT-TECH-*    PLOTTECH
//   plot/params/codes.inc  PLOT-PARAM-*   PLOTCMD
//   plot/cmd/codes.inc     PLOT-CMD-*     PLOTCMD
//   plot/io/codes.inc      PLOT-IO-*      PLOTCMD
//   plot/ps/codes.inc      PLOT-PS-*      PS
//   plot/pnm/codes.inc     PLOT-PNM-*     PNM
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_PLOT_CODE("PLOT-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/plot.md with `hefesto-cli --diagnostic-codes=plot`.
//
// Owner: SCAFFOLD (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_PLOT_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "plot/tech/codes.inc"
#include "plot/params/codes.inc"
#include "plot/cmd/codes.inc"
#include "plot/io/codes.inc"
#include "plot/ps/codes.inc"
#include "plot/pnm/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_PLOT_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a PLOT- code is declared twice");

}  // namespace

std::span<const CodeInfo> plot() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
