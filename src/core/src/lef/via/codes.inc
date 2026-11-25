// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LEF-VIA-* of the lef area, included by
// src/core/src/lef/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LEF_CODE("LEF-VIA-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lef.md with `hefesto-cli --diagnostic-codes=lef`
// and describe the codes in its VIA section.
//
// Owner: LEFVIA (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).

HEFESTO_LEF_CODE("LEF-VIA-001", warning, "A LEF VIA redefines a via or layer name (Cut type redefined).")
HEFESTO_LEF_CODE("LEF-VIA-002", warning, "A DEF VIAS definition redefines a via or layer name (Composite via redefined).")
HEFESTO_LEF_CODE("LEF-VIA-003", warning, "Cut growing: the via cut is not the output style's cut size for its type.")
HEFESTO_LEF_CODE("LEF-VIA-004", warning, "Cut growing: the via cut's size; the magic contact size is used.")
HEFESTO_LEF_CODE("LEF-VIA-005", warning, "The middle LAYERS entry of a generated via is not a contact; the layers are reordered.")
HEFESTO_LEF_CODE("LEF-VIA-006", error, "No LAYERS entry of a generated via is a contact.")
HEFESTO_LEF_CODE("LEF-VIA-007", warning, "A generated via omits a parameter; the VIARULE value or 0 is used.")
HEFESTO_LEF_CODE("LEF-VIA-008", warning, "A via PATTERN does not parse; every cut is kept.")
HEFESTO_LEF_CODE("LEF-VIA-009", warning, "DEF write: a via's cut is wider than its area.")
HEFESTO_LEF_CODE("LEF-VIA-010", warning, "DEF write: a via's cut height check (inverted in Magic) fired.")
HEFESTO_LEF_CODE("LEF-VIA-011", warning, "DEF write: a via layer has no LEF name; its RECT is skipped.")
HEFESTO_LEF_CODE("LEF-VIA-012", error, "DEF write: a via's cut array is above 1,000,000 cuts (Q-LEFDEF-150); one cut the size of the via area is written.")
