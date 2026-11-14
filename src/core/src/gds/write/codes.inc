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
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_GDS_CODE("GDS-WRITE-004", error, "A GDS output value does not fit its integer record field (Q-MASK-222); the write fails.")
HEFESTO_GDS_CODE("GDS-WRITE-006", warning, "A name or string was changed for GDS output (character table, unprintable character, string-limit truncation).")
