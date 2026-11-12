// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the gui area (prefix GUI-). The table with
// severities and summaries is src/core/src/gui/codes.cpp, documented in
// docs/diagnostics/codes/gui.md (`hefesto-cli --diagnostic-codes=gui`).
//
// Owner: GUIMGR (openspec/changes/m4-scripting/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::gui::codes {

// File prompts (ui-gui-managers "File prompts").
inline constexpr std::string_view kPromptNote = "GUI-PROMPT-001";
// DRC report file (ui-gui-managers "DRC manager and DRC report file").
inline constexpr std::string_view kReportNote = "GUI-REPORT-001";
inline constexpr std::string_view kReportOpen = "GUI-REPORT-002";
// Toolbar presets file (ui-gui-managers "Layer toolbar", "Toolbar order presets file").
inline constexpr std::string_view kTkError = "GUI-TK-001";
inline constexpr std::string_view kToolbarStatement = "GUI-TOOLBAR-001";
inline constexpr std::string_view kToolbarPreset = "GUI-TOOLBAR-002";
// Wrong number of arguments of a GUI procedure (Tcl's `wrong # args`).
inline constexpr std::string_view kUsage = "GUI-USAGE-001";
// Wrapper windows (ui-gui-managers "Wrapper window construction", hook points).
inline constexpr std::string_view kWrapper = "GUI-WRAP-001";
inline constexpr std::string_view kWrapperHook = "GUI-WRAP-002";

}  // namespace hefesto::gui::codes
