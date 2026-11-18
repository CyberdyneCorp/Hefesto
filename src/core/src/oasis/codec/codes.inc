// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes OASIS-CODEC-* of the oasis area, included by
// src/core/src/oasis/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_OASIS_CODE("OASIS-CODEC-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/oasis.md with `hefesto-cli --diagnostic-codes=oasis`
// and describe the codes in its CODEC section.
//
// Owner: OASFMT (openspec/changes/m6-lef-def-oasis/design.md D1/D2).

HEFESTO_OASIS_CODE("OASIS-CODEC-001", error, "Not an OASIS file: the first 13 bytes are not the OASIS magic bytes; nothing is read.")
HEFESTO_OASIS_CODE("OASIS-CODEC-002", error, "Unexpected end of file: the stream ends before the END record is complete, or no END record is in the last 256 bytes.")
HEFESTO_OASIS_CODE("OASIS-CODEC-003", error, "Data after the END record; the decoded content is still used.")
HEFESTO_OASIS_CODE("OASIS-CODEC-004", error, "Invalid record type, or a START record missing or repeated; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-005", error, "Integer overflow: an integer does not fit in 64 bits, or a 32-bit field exceeds 2^32 - 1; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-006", error, "Invalid real: an unknown real type or a division by zero; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-007", warning, "Illegal character in an a-string or n-string, or an empty n-string; the bytes are used as they are.")
HEFESTO_OASIS_CODE("OASIS-CODEC-008", error, "Coordinate overflow: a delta outside 32 bits or a position, offset or length outside 64 bits; the element is dropped.")
HEFESTO_OASIS_CODE("OASIS-CODEC-009", error, "Invalid point list: a count of 0 drops the element, a type above 5 stops decoding.")
HEFESTO_OASIS_CODE("OASIS-CODEC-010", error, "Invalid repetition: a type above 11 or a zero grid; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-011", error, "Invalid interval type; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-012", warning, "Empty interval (lower bound above the upper bound) in a LAYERNAME record.")
HEFESTO_OASIS_CODE("OASIS-CODEC-013", error, "Unsupported OASIS version or invalid unit in the START record; the file is not read.")
HEFESTO_OASIS_CODE("OASIS-CODEC-014", warning, "END record not 256 bytes long, or an unknown validation scheme (validation is skipped).")
HEFESTO_OASIS_CODE("OASIS-CODEC-015", error, "OASIS validation failed: the stored signature differs from the computed one; the content is still used.")
HEFESTO_OASIS_CODE("OASIS-CODEC-016", error, "Name table error: implicit and explicit records mixed, or a reference number defined twice; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-017", warning, "Duplicate cell name in CELLNAME records; the later reference number is an alias.")
HEFESTO_OASIS_CODE("OASIS-CODEC-018", error, "Undefined reference number once the whole file has been read.")
HEFESTO_OASIS_CODE("OASIS-CODEC-019", error, "A cell is defined twice (two CELL records for one name).")
HEFESTO_OASIS_CODE("OASIS-CODEC-020", error, "A record relies on an undefined modal variable; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-021", error, "Invalid element (magnification or angle, polygon or path vertex count, CTRAPEZOID type); the element is dropped.")
HEFESTO_OASIS_CODE("OASIS-CODEC-022", warning, "Irregular record: TEXT info bit 7, RECTANGLE with S and H, odd type 0/1 polygon count, or a degenerate trapezoid (dropped).")
HEFESTO_OASIS_CODE("OASIS-CODEC-023", error, "CBLOCK error: unsupported compression type, size mismatch, damaged data, inflate cap, or a START, END or CBLOCK record inside; decoding stops.")
HEFESTO_OASIS_CODE("OASIS-CODEC-024", warning, "Strict mode violation; the decoded content is unchanged.")
HEFESTO_OASIS_CODE("OASIS-CODEC-025", warning, "Unknown standard property; it is kept as an ordinary property.")
HEFESTO_OASIS_CODE("OASIS-CODEC-026", error, "The encoder cannot write a value: out of range, not representable in the requested form, unresolved, or a call out of order.")
HEFESTO_OASIS_CODE("OASIS-CODEC-027", error, "CBLOCK compression failed (zlib) or the compression level is not 1 to 9.")
HEFESTO_OASIS_CODE("OASIS-CODEC-028", error, "Invalid PROPERTY value type (above 15); decoding stops.")
