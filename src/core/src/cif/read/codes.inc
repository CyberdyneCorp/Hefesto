// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes CIF-READ-* of the cif area, included by
// src/core/src/cif/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_CIF_CODE("CIF-READ-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/cif.md with `hefesto-cli --diagnostic-codes=cif`
// and describe the codes in its CIF section.
//
// Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D1/D2).
HEFESTO_CIF_CODE("CIF-READ-001", error, "CIF read error (Magic's CIFReadError text, with its line prefix).")
HEFESTO_CIF_CODE("CIF-READ-002", warning, "CIF read warning (Magic's CIFReadWarning text, with its line prefix).")
HEFESTO_CIF_CODE("CIF-READ-003", warning, "The CIF read error or warning limit was reached; later ones are not printed.")
HEFESTO_CIF_CODE("CIF-READ-004", warning, "A CIF wire path reverses direction or repeats a point; the wire is split there.")
HEFESTO_CIF_CODE("CIF-READ-005", error, "A CIF wire junction exceeds the mitre limit; the rest of the route is dropped.")
HEFESTO_CIF_CODE("CIF-READ-006", error, "The CIF file defines or calls more symbols than the read allows; the rest are ignored.")
