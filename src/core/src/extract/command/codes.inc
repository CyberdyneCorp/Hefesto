// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-CMD-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its CMD section.
//
// Owner: EXTCMD (openspec/changes/m7-extraction/design.md D2/D3).
HEFESTO_EXTRACT_CODE("EXTRACT-CMD-001", error, "extract_cell was called without an extraction style or a technology; the cell is not extracted.")
HEFESTO_EXTRACT_CODE("EXTRACT-CMD-002", info, "A cell's extraction starts (`Extracting <cell> into <file>:`), or its counts follow it (`<cell>: N errors M warnings`).")
HEFESTO_EXTRACT_CODE("EXTRACT-CMD-003", warning, "The totals of a run (`Total of N errors (check feedback entries).`, `Total of M warnings.`).")
HEFESTO_EXTRACT_CODE("EXTRACT-CMD-004", warning, "A run cannot start: the root is unnamed, nothing is selected, the subtree cannot be read, no style is set, or there is nothing to extract.")
HEFESTO_EXTRACT_CODE("EXTRACT-CMD-005", warning, "A cell's `.ext` file cannot be opened for writing (`Cannot open output file.`); the cell is skipped.")
HEFESTO_EXTRACT_CODE("EXTRACT-CMD-006", error, "A cell's `.ext` file cannot be produced or committed; the previous file is left unchanged.")
