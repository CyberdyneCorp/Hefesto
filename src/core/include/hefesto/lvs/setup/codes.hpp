// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-SETUP-* (src/core/src/lvs/setup/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSSETUP").
//
// Owner: LVSSETUP.

#pragma once

#include <string_view>

namespace hefesto::lvs::setup_codes {

inline constexpr std::string_view kLineIgnored = "LVS-SETUP-001";
inline constexpr std::string_view kMissingFile = "LVS-SETUP-002";
inline constexpr std::string_view kUnreadable = "LVS-SETUP-003";
inline constexpr std::string_view kStepBudget = "LVS-SETUP-004";
inline constexpr std::string_view kCancelled = "LVS-SETUP-005";
inline constexpr std::string_view kCommandError = "LVS-SETUP-006";
inline constexpr std::string_view kTooLarge = "LVS-SETUP-007";
inline constexpr std::string_view kBadContext = "LVS-SETUP-008";

}  // namespace hefesto::lvs::setup_codes
