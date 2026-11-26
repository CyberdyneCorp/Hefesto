// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes DEF-READ-* of the def area, included by
// src/core/src/def/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_DEF_CODE("DEF-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/def.md with `hefesto-cli --diagnostic-codes=def`
// and describe the codes in its READ section.
//
// Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).

HEFESTO_DEF_CODE("DEF-READ-001", error, "DEF read error (Magic's LefError DEF_ERROR text): unknown layer or via in a route, bad coordinates, unknown pin class, a section END missing.")
HEFESTO_DEF_CODE("DEF-READ-002", warning, "DEF read warning (Magic's LefError DEF_WARNING text): section count mismatch, technology name mismatch, converted instance name, unplaced component.")
HEFESTO_DEF_CODE("DEF-READ-003", info, "DEF read message (Magic's LefError DEF_INFO text): unknown keyword or property ignored.")
HEFESTO_DEF_CODE("DEF-READ-004", info, "DEF read report line: `Reading DEF data from file ...`, `Processed N ... total.`, `DEF read: Processed N lines.`, `DEF read: Design has no contents.`.")
HEFESTO_DEF_CODE("DEF-READ-005", error, "The DEF file cannot be opened: `Cannot open input file <name> (<reason>).`.")
HEFESTO_DEF_CODE("DEF-READ-006", warning, "Annotation: `Label \"<net>\" did not land on any existing net geometry.`; the label goes on the last route rectangle.")
HEFESTO_DEF_CODE("DEF-READ-007", error, "Hefesto-only DEF read error where Magic crashes or reads an undefined value: RECT without a reference point, pin without placement, a generated via above 65536 cuts or 4096 rows or columns.")
HEFESTO_DEF_CODE("DEF-READ-008", warning, "Hefesto-only DEF read warning: duplicate instance renamed, repeated UNITS replaced, unknown NONDEFAULTRULE layer, placement blockage not painted.")
