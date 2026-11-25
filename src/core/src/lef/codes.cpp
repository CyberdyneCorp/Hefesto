// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "lef" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
// D1):
//
//   lef/lex/codes.inc           LEF-LEX-*         LEFR
//   lef/tech/codes.inc          LEF-TECH-*        LEFTECH
//   lef/via/codes.inc           LEF-VIA-*         LEFVIA
//   lef/read/codes.inc          LEF-READ-*        LEFR
//   lef/write/codes.inc         LEF-WRITE-*       LEFW
//   lef/command/codes.inc       LEF-CMD-*         LEFR, LEFW
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_LEF_CODE("LEF-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/lef.md with `hefesto-cli --diagnostic-codes=lef`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_LEF_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "lef/lex/codes.inc"
#include "lef/tech/codes.inc"
#include "lef/via/codes.inc"
#include "lef/read/codes.inc"
#include "lef/write/codes.inc"
#include "lef/command/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_LEF_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a LEF- code is declared twice");

}  // namespace

std::span<const CodeInfo> lef() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
