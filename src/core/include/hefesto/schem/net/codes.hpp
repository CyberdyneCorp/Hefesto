// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-NET-* (src/core/src/schem/net/codes.inc,
// docs/diagnostics/codes/schem.md "NETLIST").
//
// Owner: NETLIST.

#pragma once

#include <string_view>

namespace hefesto::schem::net_codes {

inline constexpr std::string_view kShort = "SCHEM-NET-001";
inline constexpr std::string_view kBusSyntax = "SCHEM-NET-002";
inline constexpr std::string_view kUnconnectedPin = "SCHEM-NET-003";
inline constexpr std::string_view kExprMinus = "SCHEM-NET-004";
inline constexpr std::string_view kTokenPrefix = "SCHEM-NET-005";
inline constexpr std::string_view kSizeLimit = "SCHEM-NET-006";
inline constexpr std::string_view kDuplicatePortNumber = "SCHEM-NET-007";
inline constexpr std::string_view kPinMismatch = "SCHEM-NET-008";
inline constexpr std::string_view kSameCellName = "SCHEM-NET-009";
inline constexpr std::string_view kExtraSubstring = "SCHEM-NET-010";
inline constexpr std::string_view kTemplateLf = "SCHEM-NET-011";
inline constexpr std::string_view kMissingSchematic = "SCHEM-NET-012";
inline constexpr std::string_view kNoName = "SCHEM-NET-013";
inline constexpr std::string_view kNoType = "SCHEM-NET-014";
inline constexpr std::string_view kAllPinsUnconnected = "SCHEM-NET-015";
inline constexpr std::string_view kOverlapping = "SCHEM-NET-016";
inline constexpr std::string_view kFlatStopped = "SCHEM-NET-017";
inline constexpr std::string_view kInterrupted = "SCHEM-NET-018";

}  // namespace hefesto::schem::net_codes
