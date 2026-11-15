// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes CIF-WRITE-* of the cif area, included by
// src/core/src/cif/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_CIF_CODE("CIF-WRITE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/cif.md with `hefesto-cli --diagnostic-codes=cif`
// and describe the codes in its CIF section.
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_CIF_CODE("CIF-WRITE-001", error, "No CIF output style is set: nothing is written.")
HEFESTO_CIF_CODE("CIF-WRITE-002", error, "Failure to read in entire subtree of the cell: a cell under the root cannot be read; the file is left empty.")
HEFESTO_CIF_CODE("CIF-WRITE-003", error, "The current CIF output style has a reducer of 0 and can only be used for writing Calma output.")
HEFESTO_CIF_CODE("CIF-WRITE-004", error, "The CIF output file cannot be opened or written; a failed write reports that the file may be incomplete.")
HEFESTO_CIF_CODE("CIF-WRITE-005", info, "`<n> problems occurred.  See feedback entries.` after a write whose mask generation reported problems.")
HEFESTO_CIF_CODE("CIF-WRITE-006", warning, "A label's text holds whitespace or `;`: written verbatim (magic-compat ON) or with each such character replaced by `_` (OFF, Q-MASK-322).")
HEFESTO_CIF_CODE("CIF-WRITE-007", error, "The CIF write was cancelled; no output file is left.")
HEFESTO_CIF_CODE("CIF-WRITE-008", error, "The cell hierarchy under the written cell is deeper than the writer's limit.")
