// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "antenna" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m7-extraction/design.md D2):
//
//   antenna/codes.inc           ANTENNA-CHECK-*     ANTENNA
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_ANTENNA_CODE("ANTENNA-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/antenna.md with `hefesto-cli --diagnostic-codes=antenna`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_ANTENNA_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "antenna/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_ANTENNA_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a ANTENNA- code is declared twice");

}  // namespace

std::span<const CodeInfo> antenna() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
