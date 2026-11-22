// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "ui" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix UI-), then regenerate
// docs/diagnostics/codes/ui.md with `hefesto-cli --diagnostic-codes=ui`.
// Constants: hefesto/ui/diag_codes.hpp.
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D1/D2).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kCodes{
    CodeInfo{"UI-CLIENT-001", error, "A window type (client) name is unknown, ambiguous or hidden; the valid window types are listed."},
    CodeInfo{"UI-CLIENT-002", error, "The command needs an optional module that is not available yet (`netlist` until M12, `wind3d` until M10, `rsim`)."},
    CodeInfo{"UI-DISPATCH-001", error, "The command word is an ambiguous abbreviation (`That command abbreviation is ambiguous.`); nothing was run."},
    CodeInfo{"UI-HISTORY-001", error, "A command-history expansion (`!...`, `^old^new`) names no matching event or word."},
    CodeInfo{"UI-INTERRUPT-001", warning, "The command was interrupted (`[Interrupted]`)."},
    CodeInfo{"UI-KEY-001", error, "A key or button has no macro in the current table (`Unknown macro or short command: 'K'`)."},
    CodeInfo{"UI-LOG-001", error, "`logcommands` cannot start or change the command log (already open, cannot be created, or none open)."},
    CodeInfo{"UI-MACRO-001", error, "A macro key name cannot be parsed (`Unrecognized macro name K`)."},
    CodeInfo{"UI-MACRO-002", error, "`macro` cannot find the macro table of the window or name it was given; the known tables are listed."},
    CodeInfo{"UI-PARSE-001", warning, "A command line has an unmatched quote; parsing continues as if it were closed at the end."},
    CodeInfo{"UI-REPORT-001", info, "A dispatch, macro or tool command prints a note (Magic's TxPrintf text)."},
    CodeInfo{"UI-SOURCE-001", error, "`source` cannot read the script file (`Couldn't read from FILE.`)."},
    CodeInfo{"UI-STACK-001", error, "A box or edit stack command found no stack, an empty stack or no cell to push; the script error is raised."},
    CodeInfo{"UI-SUSPEND-001", error, "`resumeall` was called without a matching `suspendall` (`resume called without suspend`)."},
    CodeInfo{"UI-TOOL-001", error, "A tool cannot be registered: ten tools exist already (`no space in button handler`)."},
    CodeInfo{"UI-USAGE-001", error, "A dispatch, macro, tool or script-helper command was given the wrong number or form of arguments; the usage is printed."},
};

}  // namespace

std::span<const CodeInfo> ui() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
