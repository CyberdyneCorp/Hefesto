// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes of the "display" area (see diag/area_codes.hpp). Add a code
// here (sorted, prefix DISPLAY-), then regenerate
// docs/diagnostics/codes/display.md with `hefesto-cli --diagnostic-codes=display`.

#include <array>

#include "diag/area_codes.hpp"

namespace hefesto::diag::area_codes {

namespace {

using enum Severity;

// Keep sorted by code. Never renumber or reuse a code once released.
constexpr std::array kDisplayCodes = {
    CodeInfo{"DISPLAY-CMAP-001", error, "A color map file (`T.C.M.cmap` or `.cmap1`) cannot be found or read."},
    CodeInfo{"DISPLAY-CMAP-002", error, "A color map line is not `red green blue maxIndex [name]`; the load fails."},
    CodeInfo{"DISPLAY-CMAP-003", error, "Color map entries are out of order (a maxIndex below the next index to fill)."},
    CodeInfo{"DISPLAY-CMAP-004", error, "The color map ends before every index is filled."},
    CodeInfo{"DISPLAY-CMAP-005", error, "The color map file cannot be written."},
    CodeInfo{"DISPLAY-DSTYLE-001", error, "No display-style file `T.D.dstyle` (or `.dstyle5`) was found."},
    CodeInfo{"DISPLAY-DSTYLE-002", warning, "The dstyle `version` line has no number; version 6 is assumed."},
    CodeInfo{"DISPLAY-DSTYLE-003", error, "Dstyle sections are out of order; the file is rejected."},
    CodeInfo{"DISPLAY-DSTYLE-004", warning, "An integer in section-header position (an unexpected empty line); the line is read as data."},
    CodeInfo{"DISPLAY-DSTYLE-005", error, "Unknown dstyle section name; the file is rejected."},
    CodeInfo{"DISPLAY-DSTYLE-006", error, "A dstyle style or stipple line has a format error; the file is rejected."},
    CodeInfo{"DISPLAY-DSTYLE-007", warning, "The dstyle file does not define exactly 52 internal styles (rejects the file with magic-compat off)."},
    CodeInfo{"DISPLAY-DSTYLE-008", error, "A required dstyle section (display_styles, layout_styles, pale_styles) is missing."},
    CodeInfo{"DISPLAY-DSTYLE-009", error, "The numbers of pale and layout styles differ; the file is rejected."},
    CodeInfo{"DISPLAY-FONT-001", error, "An outline font file cannot be found or read."},
    CodeInfo{"DISPLAY-FONT-002", error, "An outline font file has no glyph dictionary (`CharProcs begin`)."},
    CodeInfo{"DISPLAY-GLYPH-001", error, "A cursor glyph file cannot be found or read."},
    CodeInfo{"DISPLAY-GLYPH-002", error, "A glyph file has a format error (no size line, a white-space cell or a short row)."},
    CodeInfo{"DISPLAY-GLYPH-003", error, "A glyph file ends before its last glyph is complete."},
    CodeInfo{"DISPLAY-GLYPH-004", warning, "A glyph file has extra lines after its last glyph; they are ignored."},
    CodeInfo{"DISPLAY-GLYPH-005", error, "A glyph file's size line declares more pixels (count x width x height) than the 4 Mi limit; nothing is allocated."},
    CodeInfo{"DISPLAY-STYLE-001", warning, "A technology styles reference matches no layout style; the reference is skipped."},
    CodeInfo{"DISPLAY-STYLE-002", error, "The technology names no styletype family, so no display styles can be loaded."},
};

}  // namespace

std::span<const CodeInfo> display() noexcept {
  return kDisplayCodes;
}

}  // namespace hefesto::diag::area_codes
