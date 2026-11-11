// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "pcell" area (see diag/area_codes.hpp), assembled
// from one fragment per subdirectory (openspec/changes/m4-scripting/
// design.md D1), both owned by PCELL:
//
//   pcell/framework/codes.inc  the gencell framework (e.g. PCELL-GEN-*, PCELL-DIALOG-*)
//   pcell/native/codes.inc     the native generators (e.g. PCELL-NATIVE-*)
//
// The rows are sorted while compiling (diag/sorted_codes.hpp). A fragment
// line reads
//   HEFESTO_PCELL_CODE("PCELL-GEN-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/pcell.md with `hefesto-cli --diagnostic-codes=pcell`.
//
// Owner: INTEGRATION (this file); the fragments are owned by PCELL.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_PCELL_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "pcell/framework/codes.inc"
#include "pcell/native/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_PCELL_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a PCELL- code is declared twice");

}  // namespace

std::span<const CodeInfo> pcell() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
