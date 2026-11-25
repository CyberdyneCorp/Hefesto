// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LEF-READ-* of the lef area, included by
// src/core/src/lef/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LEF_CODE("LEF-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lef.md with `hefesto-cli --diagnostic-codes=lef`
// and describe the codes in its READ section.
//
// Owner: LEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).

HEFESTO_LEF_CODE("LEF-READ-001", error, "LEF read error (Magic's LefError text): unknown layer, bad LAYER statement, layer reclassification, missing or misplaced END, malformed RECT, POLYGON, SIZE, ORIGIN or FOREIGN, improper DIRECTION, USE or SHAPE.")
HEFESTO_LEF_CODE("LEF-READ-002", warning, "LEF read warning (Magic's LefError text): unknown layer TYPE, duplicate NONDEFAULTRULE or macro renamed, macro without SIZE.")
HEFESTO_LEF_CODE("LEF-READ-003", info, "LEF read message (Magic's LefError text): unknown keyword ignored, NONDEFAULTRULE value without a rule or layer, unknown via in macro geometry.")
HEFESTO_LEF_CODE("LEF-READ-004", info, "lef read: `Reading LEF data from file <path>.` before the file is read.")
HEFESTO_LEF_CODE("LEF-READ-005", info, "lef read: `LEF read: Processed <n> lines.` after the file is read.")
HEFESTO_LEF_CODE("LEF-READ-006", error, "lef read: `Cannot open input file <path> (<reason>).`; nothing is read.")
