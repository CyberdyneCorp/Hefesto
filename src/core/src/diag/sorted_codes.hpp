// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: compile-time assembly of an area code table from several
// workstream fragments (openspec/changes/m4-scripting/design.md D1). The
// script and pcell areas include one codes.inc per subdirectory whose topics
// interleave (SCRIPT-PDK-* and SCRIPT-TRUST-* from script/pdk/,
// SCRIPT-SANDBOX-* and SCRIPT-TCL-* from script/tcl/), so no include order
// keeps the table sorted; sorted_codes() sorts it while compiling instead.
// A fragment list ends with an end marker row (an array needs one element
// even when every fragment is empty), which sorted_codes() drops.
//
// Owner: INTEGRATION.

#pragma once

#include <array>
#include <cstddef>

#include "hefesto/diag/codes.hpp"

namespace hefesto::diag::area_codes {

/// The first N - 1 rows of `rows` (the last is the end marker), sorted by
/// code. Insertion sort: tables are small and this runs at compile time.
template <std::size_t N>
[[nodiscard]] constexpr std::array<CodeInfo, N - 1> sorted_codes(const CodeInfo (&rows)[N]) {
  std::array<CodeInfo, N - 1> out{};
  for (std::size_t i = 0; i + 1 < N; ++i) {
    std::size_t j = i;
    while (j > 0 && rows[i].code < out[j - 1].code) {
      out[j] = out[j - 1];
      --j;
    }
    out[j] = rows[i];
  }
  return out;
}

/// True when no two rows share a code (checked by a static_assert in each
/// assembling codes.cpp).
template <std::size_t N>
[[nodiscard]] constexpr bool unique_codes(const std::array<CodeInfo, N>& sorted) {
  for (std::size_t i = 1; i < N; ++i) {
    if (sorted[i - 1].code == sorted[i].code) {
      return false;
    }
  }
  return true;
}

}  // namespace hefesto::diag::area_codes
