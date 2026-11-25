// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "oasis" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
// D1):
//
//   oasis/codec/codes.inc       OASIS-CODEC-*     OASFMT
//   oasis/read/codes.inc        OASIS-READ-*      OASR
//   oasis/write/codes.inc       OASIS-WRITE-*     OASW
//   oasis/command/codes.inc     OASIS-CMD-*       OASCMD
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_OASIS_CODE("OASIS-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/oasis.md with `hefesto-cli --diagnostic-codes=oasis`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_OASIS_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "oasis/codec/codes.inc"
#include "oasis/read/codes.inc"
#include "oasis/write/codes.inc"
#include "oasis/command/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_OASIS_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a OASIS- code is declared twice");

}  // namespace

std::span<const CodeInfo> oasis() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
