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
// Owner: EXTSUB (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2);
// EXTRACT-STYLE-003..006 (the 3D height subset, reported by HeightView):
// M10 TECH (openspec/changes/m10-3d-view/design.md D5); EXTRACT-STYLE-007..020
// (the full reader of load_styles): EXTTECH (openspec/changes/m7-extraction/design.md D7).
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-001", error, "An extract-section line has the wrong number of arguments, or a `style`, `device` or `antenna` line is malformed (Magic's `Malformed line for keyword ...` text; Q-MISC-5); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-002", error, "A `device` line names an unknown device class (`Illegal device.  Legal devices are:`); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-003", error, "A `height` line of the selected extraction style has a non-numeric height or thickness (`Layer height ... must be numeric`); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-004", error, "A `units` line names neither microns nor lambda (`Units must be microns or lambda.`); lambda is used.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-005", error, "A `planeorder` or `noplaneordering` line is rejected or out of order (unknown plane, index out of range, one after the other); Magic's text.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-006", warning, "The plane orders of the extraction style are not a permutation of 0..k (`Warning: Extraction Style ...`); every plane gets its declaration order.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-007", error, "A numeric argument of an extract-section rule is not a number (`Capacitance value ... must be a number`, `Resistance value ...`, `Contact resistivity ... must be an integer value`, `Device resistivity ... must be numeric`, `Cannot parse cap scale value`); Magic's value is used.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-008", error, "A capacitance rule names an invalid type or plane combination (overlap of a type with itself or on one plane, a repeated overlap pair, a shield on a shielded plane, space inside an edge, sideoverlap plane errors, fringing on one plane, space in overlap types); Magic's text.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-009", error, "A `defaultareacap`, `defaultperimeter`, `defaultoverlap` or `defaultsideoverlap` line comes before any `planeorder` line (`Cannot parse ... line without plane ordering!`); the line has no effect.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-010", error, "A `device` or `fet` line has a bad parameter name, too few subcircuit terminals, overlapping drain and source types or a non-global fet substrate node; Magic's text.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-011", warning, "A device parameter limit is not a number (`Non-numeric limit ... for parameter ...`); the limit parsing stops there.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-012", error, "An `antenna` line has an unknown calculation type, a non-numeric ratio, or no calculation type and no default (Q-MISC-3: stored as none); Magic's text.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-013", warning, "A `model` line names an unknown antenna model (`Unknown antenna model ...`); no flag is set for it.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-014", warning, "A `resist` value is neither an integer nor `None` (`Resist argument must be integer or \"None\".`, `Unparseable resist argument`, `Resist argument truncated to integer value.`).")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-015", error, "A `step` value is not positive (`Hierarchical interaction step size must be > 0`); Magic aborts the technology load, the style keeps its step.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-016", warning, "The step size is zero after unit conversion (`Warning:  zero step size!  Resetting to default.`); 100 is used.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-017", info, "A `contact` line gives size, border or spacing values, which are ignored (`Contact size value ignored (using GDS generation rules).`).")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-018", info, "User types no extract line mentions and that connect to nothing are non-electrical (`The following types are not handled by extraction and will be treated as non-electrical types:`).")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-019", error, "An extract-section line names an unknown or ambiguous plane (`Unrecognized plane name ...`, `Ambiguous plane name ...`); the line stops there.")
HEFESTO_EXTRACT_CODE("EXTRACT-STYLE-020", error, "An extract-section line starts with an unknown keyword (`Illegal keyword.  Legal keywords are:`); the line is ignored.")
