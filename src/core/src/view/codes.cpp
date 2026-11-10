// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "view" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix VIEW-), then regenerate
// docs/diagnostics/codes/view.md with `hefesto-cli --diagnostic-codes=view`.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kViewCodes = {
    CodeInfo{"VIEW-BOX-001", error, "A view command needs the box and there is none, or the box is in another cell than the window's root."},
    CodeInfo{"VIEW-CLIENT-001", error, "`specialopen` names a window client that is not available yet (`netlist` arrives with netlist-window)."},
    CodeInfo{"VIEW-COLOR-001", error, "A color window command names a color index outside the color map (`The colormap only has values from 0 to N (decimal).`)."},
    CodeInfo{"VIEW-COLOR-002", error, "A color window command was run with no color window open or pointed at."},
    CodeInfo{"VIEW-COLOR-003", error, "The color window's `load` or `save` could not read or write the color map file."},
    CodeInfo{"VIEW-COORD-001", error, "A view command argument is not a valid coordinate, integer or fraction."},
    CodeInfo{"VIEW-EXPAND-001", warning, "An instance under the box cannot be expanded because its cell is unavailable."},
    CodeInfo{"VIEW-EXPAND-002", error, "`expand instance` / `unexpand instance` names an instance path that does not exist under the window's root."},
    CodeInfo{"VIEW-FEEDBACK-001", error, "`feedback` was given an unknown option, an unknown display style, or a style index below 1."},
    CodeInfo{"VIEW-FEEDBACK-002", warning, "`feedback find N` names an area that does not exist; area 1 is shown instead."},
    CodeInfo{"VIEW-FEEDBACK-003", error, "`feedback save` cannot write its file."},
    CodeInfo{"VIEW-GRID-001", error, "A `grid` spacing is not greater than zero, or `grid multiple` is outside 1..255."},
    CodeInfo{"VIEW-REPORT-001", info, "A view or window command prints a note (Magic's TxPrintf text) and returns no result."},
    CodeInfo{"VIEW-RESET-001", info, "`reset` does not apply: the host window system manages the display."},
    CodeInfo{"VIEW-SCALE-001", warning, "The requested view needs a scale below the minimum; the minimum scale is used (`At minimum scale!`)."},
    CodeInfo{"VIEW-SCROLL-001", error, "A `scroll` amount is not a fraction, or its units are not `w` or `l`."},
    CodeInfo{"VIEW-SEE-001", error, "A `see` layer list names an unknown layer."},
    CodeInfo{"VIEW-USAGE-001", error, "A view or window command was given the wrong number or form of arguments; the usage is printed."},
    CodeInfo{"VIEW-WATCH-001", error, "`*watch` names an unknown plane; the legal plane names are listed."},
    CodeInfo{"VIEW-WINDOW-001", error, "No more windows can be opened (`Can't have more than 31 windows.`)."},
    CodeInfo{"VIEW-WINDOW-002", error, "The named or numbered window does not exist."},
    CodeInfo{"VIEW-WINDOW-003", error, "The command needs a window and none is open or pointed at (`Point to a window first`)."},
    CodeInfo{"VIEW-ZOOM-001", error, "The `zoom` factor is not between 0 and 20."},
};

}  // namespace

std::span<const CodeInfo> view() noexcept {
  return kViewCodes;
}

}  // namespace hefesto::diag::area_codes
