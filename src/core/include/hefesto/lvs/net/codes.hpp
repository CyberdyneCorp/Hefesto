// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-NET-* (src/core/src/lvs/net/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSREAD").
//
// Owner: LVSREAD.

#pragma once

#include <string_view>

namespace hefesto::lvs::net_codes {

inline constexpr std::string_view kDeviceLimit = "LVS-NET-001";

}  // namespace hefesto::lvs::net_codes
