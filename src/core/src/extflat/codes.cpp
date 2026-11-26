// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "extflat" area (see diag/area_codes.hpp), assembled
// from the fragment extflat/codes.inc (EXTFLAT-READ-*, EXTSUB;
// openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1). The rows are sorted while
// compiling (diag/sorted_codes.hpp). A fragment line reads
//   HEFESTO_EXTFLAT_CODE("EXTFLAT-READ-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/extflat.md with `hefesto-cli --diagnostic-codes=extflat`.
//
// Owner: INTEGRATION (this file); the fragment is owned by EXTSUB.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_EXTFLAT_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "extflat/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_EXTFLAT_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "an EXTFLAT- code is declared twice");

}  // namespace

std::span<const CodeInfo> extflat() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
