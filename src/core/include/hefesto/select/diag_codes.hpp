// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the select area (prefix SELECT-). The table
// with severities and summaries is src/core/src/select/codes.cpp, documented
// in docs/diagnostics/codes/select.md (`hefesto-cli --diagnostic-codes=select`).
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::select::codes {

// Command arguments (cmd-selection)
inline constexpr std::string_view kUsage = "SELECT-CMD-001";
inline constexpr std::string_view kBadOption = "SELECT-CMD-002";
inline constexpr std::string_view kBadLayer = "SELECT-CMD-003";
inline constexpr std::string_view kBadLayerSpec = "SELECT-CMD-004";
inline constexpr std::string_view kUnknownStyle = "SELECT-CMD-005";
// What a selection command needs
inline constexpr std::string_view kNoBox = "SELECT-CONTEXT-001";
inline constexpr std::string_view kNoPointer = "SELECT-CONTEXT-002";
inline constexpr std::string_view kNotInEditCell = "SELECT-CONTEXT-003";
inline constexpr std::string_view kNotEditable = "SELECT-CONTEXT-004";
inline constexpr std::string_view kEmpty = "SELECT-CONTEXT-005";
// Finding material and cells
inline constexpr std::string_view kNoPaint = "SELECT-FIND-001";
inline constexpr std::string_view kNoSuchUse = "SELECT-FIND-002";
inline constexpr std::string_view kNoShortPath = "SELECT-FIND-003";
// Identify
inline constexpr std::string_view kIdIllegal = "SELECT-ID-001";
inline constexpr std::string_view kIdNotUnique = "SELECT-ID-002";
inline constexpr std::string_view kIdNoSubcell = "SELECT-ID-003";
inline constexpr std::string_view kIdNotChild = "SELECT-ID-004";
// Transforms
inline constexpr std::string_view kBadAngle = "SELECT-XFORM-001";

}  // namespace hefesto::select::codes
