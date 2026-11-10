// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic code constants of the wiring area (prefix WIRE-). The table
// with severities and summaries is src/core/src/wiring/codes.cpp, documented
// in docs/diagnostics/codes/wire.md (`hefesto-cli --diagnostic-codes=wiring`).
//
// Owner: WIRE (openspec/changes/m2-interactive-editing/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::wiring::codes {

// `wire` / `trackwire` (wiring-tool)
inline constexpr std::string_view kUsage = "WIRE-CMD-001";
inline constexpr std::string_view kBadLayer = "WIRE-CMD-002";
inline constexpr std::string_view kNoMaterial = "WIRE-CMD-003";
inline constexpr std::string_view kNoBox = "WIRE-CMD-004";
inline constexpr std::string_view kNoContact = "WIRE-CMD-005";
inline constexpr std::string_view kAmbiguousRouting = "WIRE-CMD-006";
// `wire segment` file form and path conversion
inline constexpr std::string_view kFileOpen = "WIRE-FILE-001";
inline constexpr std::string_view kFileCoordinate = "WIRE-FILE-002";
inline constexpr std::string_view kPathReversal = "WIRE-PATH-001";
inline constexpr std::string_view kPathMitre = "WIRE-PATH-002";
// Command output that is not the result (Magic's TxPrintf / TxError text)
inline constexpr std::string_view kReport = "WIRE-REPORT-001";
inline constexpr std::string_view kWarning = "WIRE-REPORT-002";
// The `wiring` technology section
inline constexpr std::string_view kTechLine = "WIRE-TECH-001";

}  // namespace hefesto::wiring::codes
