// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "connect" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix CONNECT-), then regenerate
// docs/diagnostics/codes/connect.md with `hefesto-cli --diagnostic-codes=connect`.
// Constants: hefesto/connect/diag_codes.hpp.
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D1/D2).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array<CodeInfo, 0> kCodes{};

}  // namespace

std::span<const CodeInfo> connect() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
