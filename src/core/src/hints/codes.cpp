// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "hints" area (see diag/area_codes.hpp). Add a
// code here (sorted, prefix HINTS-), then regenerate
// docs/diagnostics/codes/hints.md with `hefesto-cli --diagnostic-codes=hints`.
// Until the first code lands the table is empty (std::array of no codes).
//
// Owner: HINTS (openspec/changes/m3-drc/design.md D1/D2).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array<CodeInfo, 0> kCodes{};

}  // namespace

std::span<const CodeInfo> hints() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
