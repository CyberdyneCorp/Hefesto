// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes GDS-CMD-* of the gds area, included by
// src/core/src/gds/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_GDS_CODE("GDS-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/gds.md with `hefesto-cli --diagnostic-codes=gds`
// and describe the codes in its GDSW section.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_GDS_CODE("GDS-CMD-001", error, "Wrong number of arguments, or an unrecognized argument, in a `gds` command.")
HEFESTO_GDS_CODE("GDS-CMD-002", error, "`gds compress` with a value outside 0 to 9; the setting is kept.")
HEFESTO_GDS_CODE("GDS-CMD-003", error, "`gds contacts yes`: the option is accepted and does nothing (Magic's unimplemented function).")
