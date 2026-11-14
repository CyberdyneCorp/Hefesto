// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes GDS-READ-* of the gds area, included by
// src/core/src/gds/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_GDS_CODE("GDS-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/gds.md with `hefesto-cli --diagnostic-codes=gds`
// and describe the codes in its GDSR section.
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_GDS_CODE("GDS-READ-001", error, "Unexpected record type in input: the GDS record found is not the one the grammar expects.")
HEFESTO_GDS_CODE("GDS-READ-002", error, "Unexpected EOF: the GDS stream ends before the record the grammar expects.")
HEFESTO_GDS_CODE("GDS-READ-003", error, "Incompatible scale factor for a read-only (`gds readonly yes`) read; nothing is read.")
HEFESTO_GDS_CODE("GDS-READ-004", info, "`Library written using GDS-II Release <version>`.")
HEFESTO_GDS_CODE("GDS-READ-005", error, "Unknown BGNSTR timestamp format; the cell timestamp is set to zero.")
HEFESTO_GDS_CODE("GDS-READ-006", error, "GDS read error (Magic's CalmaReadError text, prefixed with the cell and byte position).")
HEFESTO_GDS_CODE("GDS-READ-007", warning, "`gds warning limit`: the 100th read error; later ones are not reported.")
HEFESTO_GDS_CODE("GDS-READ-008", info, "GDS read progress (`Reading \"<name>\".`, the library name, saved flatten cells, magnified copies, cells already in memory).")
HEFESTO_GDS_CODE("GDS-READ-009", error, "No cifinput style: nothing in the \"cifinput\" section of the technology file; nothing is read.")
HEFESTO_GDS_CODE("GDS-READ-010", error, "The GDS file cannot be opened or read.")
HEFESTO_GDS_CODE("GDS-READ-011", warning, "A GDS read resource cap was reached (look-ahead or magnification depth, array copies, scale range); the element is read without it.")
HEFESTO_GDS_CODE("GDS-READ-012", warning, "magic-compat OFF read behavior note (round path ends, negative path widths, discarded structure geometry).")
