// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-READ-* (src/core/src/lvs/read/codes.inc,
// docs/diagnostics/codes/lvs.md "LVSREAD").
//
// Owner: LVSREAD.

#pragma once

#include <string_view>

namespace hefesto::lvs::read_codes {

inline constexpr std::string_view kFileNotFound = "LVS-READ-001";
inline constexpr std::string_view kUnknownType = "LVS-READ-002";
inline constexpr std::string_view kBadFormat = "LVS-READ-003";
inline constexpr std::string_view kFileNumber = "LVS-READ-004";
inline constexpr std::string_view kIgnoredCard = "LVS-READ-005";
inline constexpr std::string_view kTooManyWarnings = "LVS-READ-006";
inline constexpr std::string_view kBinary = "LVS-READ-007";
inline constexpr std::string_view kSummary = "LVS-READ-008";
inline constexpr std::string_view kMissingEnds = "LVS-READ-009";
inline constexpr std::string_view kStrayEnds = "LVS-READ-010";
inline constexpr std::string_view kDuplicateCell = "LVS-READ-011";
inline constexpr std::string_view kBadlyFormed = "LVS-READ-012";
inline constexpr std::string_view kNotParameter = "LVS-READ-013";
inline constexpr std::string_view kSelfInstance = "LVS-READ-014";
inline constexpr std::string_view kIgnoredCall = "LVS-READ-015";
inline constexpr std::string_view kUndefinedCell = "LVS-READ-016";
inline constexpr std::string_view kParameterMismatch = "LVS-READ-017";
inline constexpr std::string_view kCaseSensitive = "LVS-READ-018";
inline constexpr std::string_view kIncludeDepth = "LVS-READ-019";
inline constexpr std::string_view kLineLength = "LVS-READ-020";
inline constexpr std::string_view kNameLength = "LVS-READ-021";
inline constexpr std::string_view kBadLimit = "LVS-READ-022";
inline constexpr std::string_view kIncludeUnreadable = "LVS-READ-023";
inline constexpr std::string_view kPathUnreadable = "LVS-READ-024";
inline constexpr std::string_view kIncludeCount = "LVS-READ-025";

}  // namespace hefesto::lvs::read_codes
