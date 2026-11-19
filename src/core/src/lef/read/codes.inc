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
// Owner: LEFR (openspec/changes/m6-lef-def-oasis/design.md D1/D2).

HEFESTO_LEF_CODE("LEF-READ-001", error, "LEF read error (Magic's LefError text): unknown layer, bad LAYER statement, layer reclassification, missing or misplaced END.")
HEFESTO_LEF_CODE("LEF-READ-002", warning, "LEF read warning (Magic's LefError text): unknown layer TYPE, duplicate NONDEFAULTRULE renamed.")
HEFESTO_LEF_CODE("LEF-READ-003", info, "LEF read message (Magic's LefError text): unknown keyword ignored, NONDEFAULTRULE value without a rule or layer.")
