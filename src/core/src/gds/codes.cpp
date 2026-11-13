// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "gds" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m5-mask-gds-cif/design.md
// D1):
//
//   gds/stream/codes.inc       GDS-STREAM-*    GDSR
//   gds/read/codes.inc         GDS-READ-*      GDSR
//   gds/write/codes.inc        GDS-WRITE-*     GDSW
//   gds/command/codes.inc      GDS-CMD-*       GDSW
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_GDS_CODE("GDS-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/gds.md with `hefesto-cli --diagnostic-codes=gds`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_GDS_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "gds/stream/codes.inc"
#include "gds/read/codes.inc"
#include "gds/write/codes.inc"
#include "gds/command/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_GDS_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a GDS- code is declared twice");

}  // namespace

std::span<const CodeInfo> gds() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
