// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes SCHEM-FILE-* of the schem area (schematic-file-format), included by
// src/core/src/schem/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_SCHEM_CODE("SCHEM-FILE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/schem.md with `hefesto-cli --diagnostic-codes=schem`
// and describe the codes in its FORMAT section.
//
// Owner: FORMAT (openspec/changes/m8-schematic/design.md D3).

HEFESTO_SCHEM_CODE("SCHEM-FILE-001", warning, "A record has a missing or non-numeric field and is skipped.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-002", warning, "A graphic record has a missing or out-of-range layer and is skipped.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-003", warning, "The file ends inside a brace string; the field is read as absent.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-004", warning, "An embedded symbol block comes before any instance and is skipped.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-005", warning, "The file ends inside an embedded symbol block; the block is kept.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-006", error, "The file changed on disk since it was loaded; it is not overwritten.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-007", warning, "Embedded symbol blocks are nested deeper than 64 levels; the block is skipped.")
HEFESTO_SCHEM_CODE("SCHEM-FILE-008", warning, "An embedded symbol nests symbols 40 levels deep; the rest of its block is not read.")
