// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes EXTRACT-CAP-* of the extract area, included by
// src/core/src/extract/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_EXTRACT_CODE("EXTRACT-CAP-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/extract.md with `hefesto-cli --diagnostic-codes=extract`
// and describe the codes in its CAP section.
//
// Owner: EXTCAP (openspec/changes/m7-extraction/design.md D2/D3).
HEFESTO_EXTRACT_CODE("EXTRACT-CAP-001", warning, "Two port labels of different names are on one node (`Warning:  Ports \"a\" and \"b\" are electrically shorted.`, ExtBasic.c:815-838); suppressed per Q-EXTRACT-22: with magic-compat ON whenever lumped resistance is on, with it OFF only when extresist runs in the same pass.")
HEFESTO_EXTRACT_CODE("EXTRACT-CAP-002", error, "A capacitance entry point was called without a cell, an extraction style or a technology; nothing is computed.")
