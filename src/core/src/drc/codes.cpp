// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "drc" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m3-drc/design.md D1):
//
//   drc/check/codes.inc     DRC-CHECK-*, DRC-MASK-* CHECK, DRCCIF (M5)
//   drc/commands/codes.inc  DRC-CMD-*              COMMANDS
//   drc/engine/codes.inc    DRC-ENG-*              ENGINE
//   drc/rules/codes.inc     DRC-LINT-*, DRC-TECH-* RULES
//
// The rows are sorted at compile time (sorted_codes), as DRC-MASK-* sits
// between the topics of other fragments.
// A fragment line reads
//   HEFESTO_DRC_CODE("DRC-TECH-001", warning, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/drc.md with `hefesto-cli --diagnostic-codes=drc`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_DRC_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "drc/check/codes.inc"
#include "drc/commands/codes.inc"
#include "drc/engine/codes.inc"
#include "drc/rules/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_DRC_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a DRC- code is declared twice");

}  // namespace

std::span<const CodeInfo> drc() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
