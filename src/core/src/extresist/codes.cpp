// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "extresist" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m7-extraction/design.md D2):
//
//   extresist/network/codes.inc EXTRESIST-NET-*     RESNET
//   extresist/command/codes.inc EXTRESIST-CMD-*     RESCMD
//   extresist/output/codes.inc  EXTRESIST-OUT-*     RESCMD
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_EXTRESIST_CODE("EXTRESIST-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/extresist.md with `hefesto-cli --diagnostic-codes=extresist`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_EXTRESIST_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "extresist/network/codes.inc"
#include "extresist/command/codes.inc"
#include "extresist/output/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_EXTRESIST_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a EXTRESIST- code is declared twice");

}  // namespace

std::span<const CodeInfo> extresist() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
