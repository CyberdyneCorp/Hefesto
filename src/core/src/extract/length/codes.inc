// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-LEN-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-LEN-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its LEN section.
//
// Owner: EXTCMD (openspec/changes/m7-extraction/design.md D2/D3).
HEFESTO_EXTRACT_CODE("EXTRACT-LEN-001", warning, "A driver of `extract length` names no label of the root cell or of its instances (`Can't find terminal \"<name>\"`); it gets no `distance` record.")
