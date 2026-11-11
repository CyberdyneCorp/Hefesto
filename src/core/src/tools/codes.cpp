// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "tools" area (see diag/area_codes.hpp). Add a
// code here (sorted, prefix TOOLS-), then regenerate
// docs/diagnostics/codes/tools.md with `hefesto-cli --diagnostic-codes=tools`.
// The constants are in hefesto/tools/diag_codes.hpp.
//
// Owner: TOOLS (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 8).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kCodes{
    CodeInfo{"TOOLS-MAZE-001", error, "`mazeroute` is not available yet: it arrives with the maze router `iroute` (router-maze-iroute, M10)."},
    CodeInfo{"TOOLS-RSP-001", info, "`readspice` progress (`Annotating port orders from F`, `Annotating cell C`, `Cell C port order was modified.`, `Cell C in netlist has not been loaded.`)."},
    CodeInfo{"TOOLS-RSP-002", warning, "`readspice` cannot read the netlist file or any of its extension candidates (`Error:  Can't read netlist file F`); the result is 1."},
    CodeInfo{"TOOLS-RSP-003", warning, "A `.PININFO` entry names a pin that the subcircuit line did not list (`.PININFO error:  Pin P not found.`)."},
    CodeInfo{"TOOLS-RSP-004", warning, "`readspice` could not walk the ports of a cell (`Error:  C port numbering failed.`); the remaining ports keep their indices."},
    CodeInfo{"TOOLS-STRIP-001", info, "The reference-library stripper cannot open or rewrite a `.mag` file (`Error: can't open file F`, printed on stdout); the next file is processed."},
    CodeInfo{"TOOLS-USAGE-001", error, "A tools command was given the wrong number of arguments (Tcl's `wrong # args` error of the procedure)."},
};

}  // namespace

std::span<const CodeInfo> tools() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
