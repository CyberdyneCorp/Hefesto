// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-NODE-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-NODE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its NODE section.
//
// Owner: EXTNODE (openspec/changes/m7-extraction/design.md D2/D3).

HEFESTO_EXTRACT_CODE("EXTRACT-NODE-001", warning, "Two port labels with different names are on one node (`Warning:  Ports \"a\" and \"b\" are electrically shorted.`); suppressed per Q-EXTRACT-22.")
HEFESTO_EXTRACT_CODE("EXTRACT-NODE-002", info, "The unique-naming pass starts on a cell (`Processing <cell>`).")
HEFESTO_EXTRACT_CODE("EXTRACT-NODE-003", warning, "The unique-naming pass left non-global duplicate labels as feedback (`<cell>: <n> warnings`).")
HEFESTO_EXTRACT_CODE("EXTRACT-NODE-004", error, "The unique-naming pass was called without an extraction style or technology; no label is renamed.")
