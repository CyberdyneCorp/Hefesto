// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-HIER-* (src/core/src/lvs/hier/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSHIER").
//
// Owner: LVSHIER.

#pragma once

#include <string_view>

namespace hefesto::lvs::hier_codes {

inline constexpr std::string_view kDepthLimit = "LVS-HIER-001";
inline constexpr std::string_view kNoTop = "LVS-HIER-002";
inline constexpr std::string_view kBadDepthSetting = "LVS-HIER-003";
inline constexpr std::string_view kCancelled = "LVS-HIER-004";
inline constexpr std::string_view kFlattenRounds = "LVS-HIER-005";
inline constexpr std::string_view kDeviceLimit = "LVS-HIER-006";

}  // namespace hefesto::lvs::hier_codes
