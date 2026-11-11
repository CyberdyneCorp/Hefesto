// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "gui" area (see diag/area_codes.hpp). Add a
// code here (sorted, prefix GUI-), then regenerate
// docs/diagnostics/codes/gui.md with `hefesto-cli --diagnostic-codes=gui`.
// The constants are in hefesto/gui/diag_codes.hpp.
//
// Owner: GUIMGR (openspec/changes/m4-scripting/design.md D1/D2, tasks.md section 9).

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kCodes{
    CodeInfo{"GUI-PROMPT-001", info, "A file prompt reports what it read (`Top-level cells read from GDS file: L`, `Cells read from GDS file: L`)."},
    CodeInfo{"GUI-REPORT-001", info, "`drc_save_report` or `drc_load_report` progress (`loading C`, `done with F`, `Reading F`, `Done.`)."},
    CodeInfo{"GUI-REPORT-002", error, "The DRC report file cannot be opened or written (Tcl's `couldn't open \"F\"` error); nothing is written or read."},
    CodeInfo{"GUI-TOOLBAR-001", warning, "A statement of the toolbar presets file `<tech>_toolbars.tcl` is not a `set <name> {<layers>}` line; it is ignored and never evaluated (Q-SCRIPT-11). Hefesto only."},
    CodeInfo{"GUI-TOOLBAR-002", error, "A toolbar preset cannot be loaded: the presets file `<tech>_toolbars.tcl` is missing or does not set the preset (Q-SCRIPT-28)."},
    CodeInfo{"GUI-USAGE-001", error, "A GUI manager command was given the wrong number or form of arguments (Tcl's `wrong # args` error of the procedure)."},
    CodeInfo{"GUI-WRAP-001", error, "A wrapper window cannot be opened or closed (no such frame, or the layout window cannot be created)."},
    CodeInfo{"GUI-WRAP-002", info, "A wrapper hook script (`Opts(callback)`, `Winopts(F,callback)`) failed; as in Magic the error is caught and the wrapper opens. Hefesto only."},
};

}  // namespace

std::span<const CodeInfo> gui() noexcept {
  return kCodes;
}

}  // namespace hefesto::diag::area_codes
