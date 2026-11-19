// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes OASIS-READ-* of the oasis area, included by
// src/core/src/oasis/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_OASIS_CODE("OASIS-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/oasis.md with `hefesto-cli --diagnostic-codes=oasis`
// and describe the codes in its READ section.
//
// Owner: OASR (openspec/changes/m6-lef-def-oasis/design.md D1/D2).
HEFESTO_OASIS_CODE("OASIS-READ-001", error, "No cifinput style: nothing in the \"cifinput\" section of the technology file; nothing is read.")
HEFESTO_OASIS_CODE("OASIS-READ-002", error, "The OASIS file cannot be opened or read.")
HEFESTO_OASIS_CODE("OASIS-READ-003", error, "Decoding stopped (a fatal oasis-format error); only the cells complete before it are read.")
HEFESTO_OASIS_CODE("OASIS-READ-004", error, "Coordinate overflow: an element instance has a coordinate, width or displacement outside 32 bits; it is dropped.")
HEFESTO_OASIS_CODE("OASIS-READ-005", warning, "XNAME, XELEMENT or XGEOMETRY not supported: the record is skipped (reported once per kind and cell).")
HEFESTO_OASIS_CODE("OASIS-READ-006", error, "An element does not fit its GDS-equivalent record (a polygon or path above 8191 points, a string above 65530 bytes); it is dropped or cut.")
HEFESTO_OASIS_CODE("OASIS-READ-007", error, "A read resource cap was reached (the size of the GDS-equivalent stream); the rest of the file is not read.")
