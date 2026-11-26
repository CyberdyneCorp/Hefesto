// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-REDUCE-* (src/core/src/lvs/reduce/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSRED").
//
// Owner: LVSRED.

#pragma once

#include <string_view>

namespace hefesto::lvs::reduce_codes {

inline constexpr std::string_view kMissingPin = "LVS-REDUCE-001";

}  // namespace hefesto::lvs::reduce_codes
