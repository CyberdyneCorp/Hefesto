// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-STYLE-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its STYLE section.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D1/D2);
// EXTRACT-STYLE-003..006 (the 3D height subset, reported by HeightView):
// M10 TECH (openspec/changes/m10-3d-view/design.md D5).
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-001", error, "A `style` or `device` line of the selected extraction style is malformed (Magic's `Malformed line for keyword ...` text); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-002", error, "A `device` line names an unknown device class (`Illegal device.  Legal devices are:`); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-003", error, "A `height` line of the selected extraction style has a non-numeric height or thickness (`Layer height ... must be numeric`); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-004", error, "A `units` line names neither microns nor lambda (`Units must be microns or lambda.`); lambda is used.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-005", error, "A `planeorder` or `noplaneordering` line is rejected or out of order (unknown plane, index out of range, one after the other); Magic's text.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-006", warning, "The plane orders of the extraction style are not a permutation of 0..k (`Warning: Extraction Style ...`); every plane gets its declaration order.")
