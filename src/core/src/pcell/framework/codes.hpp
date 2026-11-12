// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The PCELL-* codes of pcell/framework (codes.inc) as constants.
//
// Owner: PCELL (openspec/changes/m4-scripting/design.md D1/D2).

#pragma once

#include <string_view>

namespace hefesto::pcell::codes {

/// A Tcl list or dictionary value is malformed (Tcl's own message).
inline constexpr std::string_view kListSyntax = "PCELL-LIST-001";
/// A framework procedure raised a script error (toolkit.tcl `error`, a
/// failing engine command or device procedure outside `catch`).
inline constexpr std::string_view kScriptError = "PCELL-GEN-001";
/// A device's _check or _draw failed inside the framework's `catch`; the
/// message is printed on stderr and generation continues.
inline constexpr std::string_view kDeviceMessage = "PCELL-GEN-002";
/// The project has no script interpreter (the framework calls PDK code).
inline constexpr std::string_view kNoInterpreter = "PCELL-GEN-003";
/// netlist_to_layout progress and skip messages (toolkit.tcl `puts`).
inline constexpr std::string_view kNetlistReport = "PCELL-NETLIST-001";

}  // namespace hefesto::pcell::codes
