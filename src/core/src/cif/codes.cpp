// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "cif" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m5-mask-gds-cif/design.md
// D1):
//
//   cif/read/codes.inc         CIF-READ-*      CIF
//   cif/write/codes.inc        CIF-WRITE-*     CIF
//   cif/command/codes.inc      CIF-CMD-*       CIF
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_CIF_CODE("CIF-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/cif.md with `hefesto-cli --diagnostic-codes=cif`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_CIF_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "cif/read/codes.inc"
#include "cif/write/codes.inc"
#include "cif/command/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_CIF_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a CIF- code is declared twice");

}  // namespace

std::span<const CodeInfo> cif() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
