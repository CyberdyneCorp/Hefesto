// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes CIF-CMD-* of the cif area, included by
// src/core/src/cif/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_CIF_CODE("CIF-CMD-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/cif.md with `hefesto-cli --diagnostic-codes=cif`
// and describe the codes in its CIF section.
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_CIF_CODE("CIF-CMD-001", warning, "`cif unfracture yes` (or `no`) with magic-compat on: the third word is not applied, Magic reads the value from the fourth (Q-MASK-413).")
HEFESTO_CIF_CODE("CIF-CMD-002", info, "`<n> problems occurred.  See feedback entries.` after a `cif see`, `paint`, `coverage` or preview whose mask generation reported problems.")
HEFESTO_CIF_CODE("CIF-CMD-003", error, "No CIF output style is set: the `cif` option has nothing to generate.")
