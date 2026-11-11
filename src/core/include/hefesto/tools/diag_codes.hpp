// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the tools area (prefix TOOLS-). The table
// with severities and summaries is src/core/src/tools/codes.cpp, documented
// in docs/diagnostics/codes/tools.md (`hefesto-cli --diagnostic-codes=tools`).
//
// Owner: TOOLS (openspec/changes/m4-scripting/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::tools::codes {

// mazeroute arrives with router-maze-iroute (M10; Q-SCRIPT-30).
inline constexpr std::string_view kMazeRoute = "TOOLS-MAZE-001";
// readspice (capability readspice).
inline constexpr std::string_view kReadSpiceNote = "TOOLS-RSP-001";
inline constexpr std::string_view kReadSpiceNoFile = "TOOLS-RSP-002";
inline constexpr std::string_view kReadSpicePinInfo = "TOOLS-RSP-003";
inline constexpr std::string_view kReadSpiceNumbering = "TOOLS-RSP-004";
// strip_reflibs (scripting-auxiliary-tools "Reference-library stripper").
inline constexpr std::string_view kStripOpen = "TOOLS-STRIP-001";
// Wrong number of arguments of a tools command (a Tcl procedure's error).
inline constexpr std::string_view kUsage = "TOOLS-USAGE-001";

}  // namespace hefesto::tools::codes
