// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes GDS-WRITE-* of the gds area, included by
// src/core/src/gds/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_GDS_CODE("GDS-WRITE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/gds.md with `hefesto-cli --diagnostic-codes=gds`
// and describe the codes in its GDSW section.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Ranges:
// 001-019 the writer core, 020-039 the merged-polygon and label emitters,
// 040 on verbatim copy, full dump, addendum and abstract views.

HEFESTO_GDS_CODE("GDS-WRITE-001", info, "GDS write progress: `Generating output for cell <name>` for each cell the write reaches.")
HEFESTO_GDS_CODE("GDS-WRITE-002", error, "No CIF/GDS output style set: nothing is written and no file is opened (Q-MASK-219).")
HEFESTO_GDS_CODE("GDS-WRITE-003", error, "Failure to read entire subtree of the cell: a cell under the root cannot be read and `gds undefined` is disallow.")
HEFESTO_GDS_CODE("GDS-WRITE-005", error, "The output style's units are not a whole number of nanometers or angstroms (Q-MASK-225).")
HEFESTO_GDS_CODE("GDS-WRITE-007", warning, "Two cells map to the same structure name; the later one is renamed (Q-MASK-209).")
HEFESTO_GDS_CODE("GDS-WRITE-008", error, "A cell is an abstract view and `gds abstract` is disallow; the write fails (Q-MASK-226).")
HEFESTO_GDS_CODE("GDS-WRITE-009", warning, "An abstract view is written to GDS (`gds abstract allow`).")
HEFESTO_GDS_CODE("GDS-WRITE-010", error, "The GDS output file cannot be opened or written; a failed write reports that the file may be incomplete.")
HEFESTO_GDS_CODE("GDS-WRITE-011", error, "The written cell has the default name (UNNAMED).")
HEFESTO_GDS_CODE("GDS-WRITE-012", info, "`<n> problems occurred.  See feedback entries.` after a write whose mask generation reported problems.")
HEFESTO_GDS_CODE("GDS-WRITE-013", error, "The GDS write was cancelled; no output file is left.")
HEFESTO_GDS_CODE("GDS-WRITE-014", error, "The cell hierarchy under the written cell is deeper than the writer's limit.")
HEFESTO_GDS_CODE("GDS-WRITE-015", error, "A read-only vendor cell with GDS content cannot be copied yet (verbatim copy and full dump arrive with task 10.7).")
HEFESTO_GDS_CODE("GDS-WRITE-004", error, "A GDS output value does not fit its integer record field (Q-MASK-222); the write fails.")
HEFESTO_GDS_CODE("GDS-WRITE-006", warning, "A name or string was changed for GDS output (character table, unprintable character, string-limit truncation).")
HEFESTO_GDS_CODE("GDS-WRITE-020", warning, "Port labels of a type without a `labels <type> port` layer are skipped (magic-compat ON) or written on the port-text layer (OFF) (Q-MASK-206).")
