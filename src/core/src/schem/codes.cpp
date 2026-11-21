// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "schem" area (see diag/area_codes.hpp), assembled
// from one fragment per subdirectory (openspec/changes/m8-schematic/design.md
// D3):
//
//   schem/file/codes.inc   SCHEM-FILE-*   FORMAT
//   schem/sym/codes.inc    SCHEM-SYM-*    SYMLIB
//   schem/lib/codes.inc    SCHEM-LIB-*    SYMLIB
//   schem/tcl/codes.inc    SCHEM-TCL-*    SYMLIB
//   schem/net/codes.inc    SCHEM-NET-*    NETLIST
//   schem/edit/codes.inc   SCHEM-EDIT-*   EDIT
//   schem/cmd/codes.inc    SCHEM-CMD-*    CMD
//   schem/sim/codes.inc    SCHEM-SIM-*    SIMINT
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_SCHEM_CODE("SCHEM-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`.
//
// Owner: SCAFFOLD (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_SCHEM_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "schem/file/codes.inc"
#include "schem/sym/codes.inc"
#include "schem/lib/codes.inc"
#include "schem/tcl/codes.inc"
#include "schem/net/codes.inc"
#include "schem/edit/codes.inc"
#include "schem/cmd/codes.inc"
#include "schem/sim/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_SCHEM_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a SCHEM- code is declared twice");

}  // namespace

std::span<const CodeInfo> schem() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
