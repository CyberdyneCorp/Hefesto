// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-CMD-* (src/core/src/lvs/cmd/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSCMD").
//
// Owner: LVSCMD.

#pragma once

#include <string_view>

namespace hefesto::lvs::cmd_codes {

inline constexpr std::string_view kOutput = "LVS-CMD-001";
inline constexpr std::string_view kUsage = "LVS-CMD-002";
inline constexpr std::string_view kNetlist = "LVS-CMD-003";
inline constexpr std::string_view kSameNetlist = "LVS-CMD-004";
inline constexpr std::string_view kNoCell = "LVS-CMD-005";
inline constexpr std::string_view kSetup = "LVS-CMD-006";    // retired (M9 R3): LVS-SETUP-* instead
inline constexpr std::string_view kWrite = "LVS-CMD-007";
inline constexpr std::string_view kBusy = "LVS-CMD-008";
inline constexpr std::string_view kNoSetup = "LVS-CMD-009";  // retired (M9 R3): LVS-SETUP-002 instead
inline constexpr std::string_view kNoFlattenLimit = "LVS-CMD-010";
inline constexpr std::string_view kNoCounterpart = "LVS-CMD-011";

}  // namespace hefesto::lvs::cmd_codes
