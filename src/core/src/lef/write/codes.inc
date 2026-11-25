// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LEF-WRITE-* of the lef area, included by
// src/core/src/lef/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LEF_CODE("LEF-WRITE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lef.md with `hefesto-cli --diagnostic-codes=lef`
// and describe the codes in its WRITE section.
//
// Owner: LEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2).
HEFESTO_LEF_CODE("LEF-WRITE-001", info, "LEF write progress: `Generating LEF output <file> for cell <name>:` and the `Diagnostic:` lines.")
HEFESTO_LEF_CODE("LEF-WRITE-002", error, "LEF write: the output file cannot be opened; nothing is written.")
HEFESTO_LEF_CODE("LEF-WRITE-003", error, "lef writeall: a cell of the subtree cannot be read; nothing is written.")
HEFESTO_LEF_CODE("LEF-WRITE-004", warning, "LEF write: one port index is used by labels with different texts; one PIN is written.")
HEFESTO_LEF_CODE("LEF-WRITE-005", warning, "LEF write: a port has no geometry on a LEF layer; no PIN is written (Q-LEFDEF-46).")
HEFESTO_LEF_CODE("LEF-WRITE-006", warning, "LEF write: the -hide setback is more than half the macro boundary; nothing is hidden (Q-LEFDEF-47).")
HEFESTO_LEF_CODE("LEF-WRITE-007", error, "LEF write: OBS_BBOX is not four integers; it is ignored.")
HEFESTO_LEF_CODE("LEF-WRITE-008", warning, "LEF write: a written name contains a space, `;`, `#` or newline; it is written verbatim (Q-LEFDEF-35).")
HEFESTO_LEF_CODE("LEF-WRITE-009", error, "lef write, lef writeall: no cell is selected.")
HEFESTO_LEF_CODE("LEF-WRITE-010", info, "lef write: a def write option is ignored.")
HEFESTO_LEF_CODE("LEF-WRITE-011", error, "LEF write: no technology is loaded; nothing is written.")
