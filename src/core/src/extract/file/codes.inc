// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-FILE-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-FILE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its FILE section.
//
// Owner: EXTNODE (openspec/changes/m7-extraction/design.md D2/D3).

HEFESTO_EXTRACT_CODE("EXTRACT-FILE-001", error, "A `node`, `substrate` or `merge` record does not hold one area and perimeter pair per resistance class of the header; the `.ext` text is not produced.")
HEFESTO_EXTRACT_CODE("EXTRACT-FILE-002", error, "The `.ext` writer was finished without header records; the `.ext` text is not produced.")
HEFESTO_EXTRACT_CODE("EXTRACT-FILE-003", error, "An extraction output file cannot be opened, written or renamed over its target (`Cannot open output file ...`); the previous file is left unchanged.")
HEFESTO_EXTRACT_CODE("EXTRACT-FILE-004", warning, "The `extract path` directory does not exist (`Path \"...\" does not exist;  attempting to create it.`, `...  saving locally.`).")
