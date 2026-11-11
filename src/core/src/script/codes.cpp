// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "script" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m4-scripting/design.md
// D1):
//
//   script/binding/codes.inc  SCRIPT-BIND-*                     BINDING
//   script/pdk/codes.inc      SCRIPT-PDK-*, SCRIPT-TRUST-*      PDKINT
//   script/tcl/codes.inc      SCRIPT-SANDBOX-*, SCRIPT-TCL-*    TCL
//
// The topics of pdk/ and tcl/ interleave, so the rows are sorted while
// compiling (diag/sorted_codes.hpp); each fragment may list its codes in
// any order. A fragment line reads
//   HEFESTO_SCRIPT_CODE("SCRIPT-TCL-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/script.md with `hefesto-cli --diagnostic-codes=script`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_SCRIPT_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "script/binding/codes.inc"
#include "script/pdk/codes.inc"
#include "script/tcl/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_SCRIPT_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a SCRIPT- code is declared twice");

}  // namespace

std::span<const CodeInfo> script() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
