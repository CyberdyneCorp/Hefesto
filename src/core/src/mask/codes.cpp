// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "mask" area (see diag/area_codes.hpp), assembled
// from one fragment per workstream (openspec/changes/m5-mask-gds-cif/design.md
// D1):
//
//   mask/plane/codes.inc       MASK-OPER-*     OPS
//   mask/style/codes.inc       MASK-STYLE-*    STYLES
//   mask/input/codes.inc       MASK-INPUT-*    INPUT
//   mask/ops/codes.inc         MASK-OPER-*     OPS
//   mask/gen/codes.inc         MASK-GEN-*      GEN
//
// The rows are sorted while compiling (diag/sorted_codes.hpp); each fragment
// may list its codes in any order. A fragment line reads
//   HEFESTO_MASK_CODE("MASK-TOPIC-001", error, "One-line summary.")
// Never renumber or reuse a code once released. After a change regenerate
// docs/diagnostics/codes/mask.md with `hefesto-cli --diagnostic-codes=mask`.
//
// Owner: INTEGRATION (this file); each fragment is owned by its workstream.

#include "diag/area_codes.hpp"
#include "diag/sorted_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

#define HEFESTO_MASK_CODE(code, severity, summary) CodeInfo{code, severity, summary},

constexpr CodeInfo kRows[] = {
#include "mask/plane/codes.inc"
#include "mask/style/codes.inc"
#include "mask/input/codes.inc"
#include "mask/ops/codes.inc"
#include "mask/gen/codes.inc"
    CodeInfo{"", error, ""},  // end marker (an array needs one element); dropped by sorted_codes
};

#undef HEFESTO_MASK_CODE

constexpr auto kTable = sorted_codes(kRows);
static_assert(unique_codes(kTable), "a MASK- code is declared twice");

}  // namespace

std::span<const CodeInfo> mask() noexcept {
  return kTable;
}

}  // namespace hefesto::diag::area_codes
