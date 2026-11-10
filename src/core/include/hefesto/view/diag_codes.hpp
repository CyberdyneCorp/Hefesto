// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the view area (table: src/core/src/view/codes.cpp,
// document: docs/diagnostics/codes/view.md).
//
// Owner: VIEW (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <string_view>

namespace hefesto::view::codes {

inline constexpr std::string_view kBox = "VIEW-BOX-001";
inline constexpr std::string_view kClient = "VIEW-CLIENT-001";
inline constexpr std::string_view kColorRange = "VIEW-COLOR-001";
inline constexpr std::string_view kNoColorWindow = "VIEW-COLOR-002";
inline constexpr std::string_view kColorFile = "VIEW-COLOR-003";
inline constexpr std::string_view kCoordinate = "VIEW-COORD-001";
inline constexpr std::string_view kExpand = "VIEW-EXPAND-001";
inline constexpr std::string_view kInstance = "VIEW-EXPAND-002";
inline constexpr std::string_view kFeedback = "VIEW-FEEDBACK-001";
inline constexpr std::string_view kFeedbackRange = "VIEW-FEEDBACK-002";
inline constexpr std::string_view kFeedbackFile = "VIEW-FEEDBACK-003";
inline constexpr std::string_view kGrid = "VIEW-GRID-001";
inline constexpr std::string_view kReport = "VIEW-REPORT-001";
inline constexpr std::string_view kErrorReport = "VIEW-REPORT-002";
inline constexpr std::string_view kReset = "VIEW-RESET-001";
inline constexpr std::string_view kMinimumScale = "VIEW-SCALE-001";
inline constexpr std::string_view kScroll = "VIEW-SCROLL-001";
inline constexpr std::string_view kSee = "VIEW-SEE-001";
inline constexpr std::string_view kUsage = "VIEW-USAGE-001";
inline constexpr std::string_view kWatchPlane = "VIEW-WATCH-001";
inline constexpr std::string_view kWindowLimit = "VIEW-WINDOW-001";
inline constexpr std::string_view kWindowUnknown = "VIEW-WINDOW-002";
inline constexpr std::string_view kNoWindow = "VIEW-WINDOW-003";
inline constexpr std::string_view kZoom = "VIEW-ZOOM-001";

}  // namespace hefesto::view::codes
