// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "extract" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m7-extraction/design.md D2):
//
//   extract/tech/codes.inc      EXTRACT-STYLE-*     EXTTECH
//   extract/node/codes.inc      EXTRACT-NODE-*      EXTNODE
//   extract/file/codes.inc      EXTRACT-FILE-*      EXTNODE
//   extract/cap/codes.inc       EXTRACT-CAP-*       EXTCAP
//   extract/device/codes.inc    EXTRACT-DEV-*       EXTDEV
//   extract/hier/codes.inc      EXTRACT-HIER-*      EXTHIER
//   extract/command/codes.inc   EXTRACT-CMD-*       EXTCMD
//   extract/length/codes.inc    EXTRACT-LEN-*       EXTCMD
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_EXTRACT_CODE("EXTRACT-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_EXTRACT_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "extract/tech/codes.inc"
#include "extract/node/codes.inc"
#include "extract/file/codes.inc"
#include "extract/cap/codes.inc"
#include "extract/device/codes.inc"
#include "extract/hier/codes.inc"
#include "extract/command/codes.inc"
#include "extract/length/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_EXTRACT_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a EXTRACT- code is declared twice");

}  // namespace

std::span<const CodeInfo> extract() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
