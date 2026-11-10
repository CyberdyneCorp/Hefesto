// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the ui area (prefix UI-). The table with
// severities and summaries is src/core/src/ui/codes.cpp, documented in
// docs/diagnostics/codes/ui.md (`hefesto-cli --diagnostic-codes=ui`).
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::ui::codes {

inline constexpr std::string_view kClient = "UI-CLIENT-001";
inline constexpr std::string_view kModuleUnavailable = "UI-CLIENT-002";
inline constexpr std::string_view kAmbiguous = "UI-DISPATCH-001";
inline constexpr std::string_view kHistory = "UI-HISTORY-001";
inline constexpr std::string_view kInterrupted = "UI-INTERRUPT-001";
inline constexpr std::string_view kUnknownKey = "UI-KEY-001";
inline constexpr std::string_view kLog = "UI-LOG-001";
inline constexpr std::string_view kMacroName = "UI-MACRO-001";
inline constexpr std::string_view kMacroTable = "UI-MACRO-002";
inline constexpr std::string_view kUnmatchedQuote = "UI-PARSE-001";
inline constexpr std::string_view kReport = "UI-REPORT-001";
inline constexpr std::string_view kSource = "UI-SOURCE-001";
inline constexpr std::string_view kStack = "UI-STACK-001";
inline constexpr std::string_view kSuspend = "UI-SUSPEND-001";
inline constexpr std::string_view kToolSpace = "UI-TOOL-001";
inline constexpr std::string_view kUsage = "UI-USAGE-001";

}  // namespace hefesto::ui::codes
