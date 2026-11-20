// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes OASIS-WRITE-* of the oasis area, included by
// src/core/src/oasis/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_OASIS_CODE("OASIS-WRITE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/oasis.md with `hefesto-cli --diagnostic-codes=oasis`
// and describe the codes in its WRITE section.
//
// Owner: OASW (openspec/changes/m6-lef-def-oasis/design.md D1/D2).
HEFESTO_OASIS_CODE("OASIS-WRITE-001", error, "The OASIS output file cannot be created or written; nothing is written.")
HEFESTO_OASIS_CODE("OASIS-WRITE-002", warning, "Font labels were written as OASIS TEXT without font, size, rotation or justification (once per write, with the count; Q-OASIS-32).")
HEFESTO_OASIS_CODE("OASIS-WRITE-003", warning, "A cell name is already used in the OASIS file; the later cell is written as XXXXX<n> (Q-OASIS-61).")
HEFESTO_OASIS_CODE("OASIS-WRITE-004", error, "A path of odd width in a read-only GDS cell cannot be written to OASIS; the write fails (Q-OASIS-63).")
HEFESTO_OASIS_CODE("OASIS-WRITE-005", warning, "An element of a read-only cell is dropped or changed for OASIS (degenerate boundary, single-point path, negative width, unknown path type, NODE).")
HEFESTO_OASIS_CODE("OASIS-WRITE-006", warning, "A read-only OASIS cell carries XELEMENT or XGEOMETRY records whose payloads may refer to XNAMEs that are not written (Q-OASIS-40).")
HEFESTO_OASIS_CODE("OASIS-WRITE-007", error, "A read-only OASIS cell cannot be copied from its source file (undecodable file, different unit); it is generated from its layout.")
HEFESTO_OASIS_CODE("OASIS-WRITE-008", error, "An output layer with a negative datatype gets no LAYERNAME record.")
HEFESTO_OASIS_CODE("OASIS-WRITE-009", error, "An element cannot be encoded in OASIS (a value out of range); it is not written and the write fails.")
HEFESTO_OASIS_CODE("OASIS-WRITE-010", error, "A read-only GDS array has inexact steps and more instances than an explicit repetition may list; it is not written and the write fails.")
HEFESTO_OASIS_CODE("OASIS-WRITE-011", error, "The OASIS write was cancelled; no output file is left.")
HEFESTO_OASIS_CODE("OASIS-WRITE-012", error, "The GDS element stream being translated is malformed or truncated; the cells before it are written.")
