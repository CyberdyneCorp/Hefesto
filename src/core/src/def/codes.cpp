// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "def" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
// D1):
//
//   def/read/codes.inc          DEF-READ-*        DEFR
//   def/write/codes.inc         DEF-WRITE-*       DEFW
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_DEF_CODE("DEF-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/def.md with `hefesto-cli --diagnostic-codes=def`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_DEF_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "def/read/codes.inc"
#include "def/write/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_DEF_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a DEF- code is declared twice");

}  // namespace

std::span<const CodeInfo> def() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
