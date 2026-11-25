// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LEF-TECH-* of the lef area, included by
// src/core/src/lef/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LEF_CODE("LEF-TECH-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lef.md with `hefesto-cli --diagnostic-codes=lef`
// and describe the codes in its TECH section.
//
// Owner: LEFTECH (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).

HEFESTO_LEF_CODE("LEF-TECH-001", error, "Unknown or ambiguous keyword in the tech lef section; the line is ignored.")
HEFESTO_LEF_CODE("LEF-TECH-002", error, "A lef section line names no LEF layer; the line is ignored.")
HEFESTO_LEF_CODE("LEF-TECH-003", error, "A lef section line names no magic user type; the line is ignored.")
HEFESTO_LEF_CODE("LEF-TECH-004", error, "Two magic types on a lef section line other than obstruction; the line is ignored.")
HEFESTO_LEF_CODE("LEF-TECH-005", error, "More than two magic types on a lef section line (magic-compat OFF); the line is ignored.")
HEFESTO_LEF_CODE("LEF-TECH-006", error, "A contact type is defined with a keyword other than cut, contact, obstruction or layer.")
HEFESTO_LEF_CODE("LEF-TECH-007", error, "A non-contact type is defined as a cut or contact.")
HEFESTO_LEF_CODE("LEF-TECH-008", error, "A LEF name is already bound to a magic type; the name keeps its record.")
