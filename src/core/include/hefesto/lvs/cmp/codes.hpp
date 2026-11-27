// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-CMP-* (src/core/src/lvs/cmp/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSALG").
//
// Owner: LVSALG.

#pragma once

#include <string_view>

namespace hefesto::lvs::cmp_codes {

inline constexpr std::string_view kEmptyCircuit = "LVS-CMP-001";
inline constexpr std::string_view kCancelled = "LVS-CMP-002";
inline constexpr std::string_view kRoundLimit = "LVS-CMP-003";

}  // namespace hefesto::lvs::cmp_codes
