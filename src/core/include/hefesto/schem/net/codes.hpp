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

}  // namespace hefesto::schem::net_codes
