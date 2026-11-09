// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the "app" area (table: src/core/src/app/codes.cpp,
// document: docs/diagnostics/codes/app.md).
//
// Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2/D3).

#pragma once

#include <string_view>

namespace hefesto::app::codes {

inline constexpr std::string_view kDisplayUnknown = "APP-DISPLAY-001";
inline constexpr std::string_view kFileLoading = "APP-FILE-001";
inline constexpr std::string_view kFileUnsupported = "APP-FILE-002";
inline constexpr std::string_view kOptionUnknown = "APP-OPTION-001";
inline constexpr std::string_view kOptionBadName = "APP-OPTION-002";
inline constexpr std::string_view kOptionAmbiguous = "APP-OPTION-003";
inline constexpr std::string_view kOptionMissing = "APP-OPTION-004";
inline constexpr std::string_view kPinMissing = "APP-PIN-001";
inline constexpr std::string_view kProjectFormat = "APP-PROJECT-001";
inline constexpr std::string_view kProjectIo = "APP-PROJECT-002";
inline constexpr std::string_view kRecoverUnavailable = "APP-RECOVER-001";
inline constexpr std::string_view kSettingsFile = "APP-SETTINGS-001";
inline constexpr std::string_view kStartupFile = "APP-STARTUP-001";
inline constexpr std::string_view kStartupNote = "APP-STARTUP-002";
inline constexpr std::string_view kTechFailed = "APP-TECH-001";
inline constexpr std::string_view kTechUsing = "APP-TECH-002";

}  // namespace hefesto::app::codes
