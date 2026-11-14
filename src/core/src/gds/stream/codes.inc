// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes GDS-STREAM-* of the gds area, included by
// src/core/src/gds/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_GDS_CODE("GDS-STREAM-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/gds.md with `hefesto-cli --diagnostic-codes=gds`
// and describe the codes in its GDSR section.
//
// Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_GDS_CODE("GDS-STREAM-001", error, "The GDS stream ends inside a record's data bytes.")
HEFESTO_GDS_CODE("GDS-STREAM-002", error, "The gzip-compressed GDS stream is corrupt or cut off.")
HEFESTO_GDS_CODE("GDS-STREAM-003", error, "A seek moved past the end of the (decompressed) GDS stream.")
HEFESTO_GDS_CODE("GDS-STREAM-004", error, "A GDS record payload exceeds 65531 bytes and cannot be written.")
HEFESTO_GDS_CODE("GDS-STREAM-005", error, "Gzip compression of the GDS output failed or the level is not 1 to 9.")
