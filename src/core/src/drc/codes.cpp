// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "drc" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m3-drc/design.md D1):
//
//   drc/check/codes.inc     DRC-CHECK-*            CHECK
//   drc/commands/codes.inc  DRC-CMD-*              COMMANDS
//   drc/engine/codes.inc    DRC-ENG-*              ENGINE
//   drc/rules/codes.inc     DRC-LINT-*, DRC-TECH-* RULES
//
// The fragments are included in this order, which is the sorted order of
// their topics, so the table stays sorted when each fragment is sorted.
// A fragment line reads
//   HEFESTO_DRC_CODE("DRC-TECH-001", warning, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/drc.md with `hefesto-cli --diagnostic-codes=drc`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include <iterator>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_DRC_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kTable[] = {
#include "drc/check/codes.inc"
#include "drc/commands/codes.inc"
#include "drc/engine/codes.inc"
#include "drc/rules/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); not part of the table
};

#undef HEFESTO_DRC_CODE

}  // namespace

std::span<const CodeInfo> drc() noexcept {
  return std::span<const CodeInfo>{kTable, std::size(kTable) - 1};
}

}  // namespace hefesto::diag::area_codes
