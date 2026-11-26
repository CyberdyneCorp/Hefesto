// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes PLOT-TECH-* of the plot area, included by
// src/core/src/plot/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_PLOT_CODE("PLOT-TECH-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/plot.md with `hefesto-cli --diagnostic-codes=plot`
// and describe the codes in its PLOTTECH section.
//
// Owner: PLOTTECH (openspec/changes/m10-plot-route/design.md D3).
HEFESTO_PLOT_CODE("PLOT-TECH-001", warning, "A `style` line of the technology plot section has the wrong number of fields or names an unknown plot style, or a line comes before any `style`; after the last two, the lines up to the next `style` are skipped.")
HEFESTO_PLOT_CODE("PLOT-TECH-002", warning, "A `postscript` line of the technology plot section has neither 9, 5 nor 3 fields; the line is ignored.")
HEFESTO_PLOT_CODE("PLOT-TECH-003", warning, "A numeric field of a `postscript` line of the technology plot section is not a number (Q-MISC-50); the line is ignored.")
HEFESTO_PLOT_CODE("PLOT-TECH-004", warning, "A `map` line of the technology plot section names no type (Q-MISC-50); the line is ignored.")
HEFESTO_PLOT_CODE("PLOT-TECH-005", warning, "A PNM colour map or display-style file of the technology plot section cannot be opened or has a format error; `plot pnm` refuses until the next technology load.")
HEFESTO_PLOT_CODE("PLOT-TECH-006", warning, "A `draw` line of the technology plot section names an unknown layer or display style; the line is ignored.")
