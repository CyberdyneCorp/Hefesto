// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes LVS-CMP-* of the lvs area (lvs-comparison-algorithm), included by
// src/core/src/lvs/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_LVS_CODE("LVS-CMP-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/lvs.md with `hefesto-cli --diagnostic-codes=lvs`
// and describe the codes in its LVSALG section.
//
// Owner: LVSALG (openspec/changes/m9-lvs-simulation/design.md D3).
HEFESTO_LVS_CODE("LVS-CMP-001", error, "A compared circuit has no devices or no nets (`Circuit <c> contains no devices.`, `... no nets.`); the comparison stops.")
HEFESTO_LVS_CODE("LVS-CMP-002", warning, "The comparison was cancelled between refinement rounds; the partition is kept, so reports can still be written, and the result is marked cancelled.")
HEFESTO_LVS_CODE("LVS-CMP-003", warning, "Refinement stopped at its round limit (twice the vertex count) without converging; the partition is kept (a Hefesto guard against a cycling partition).")
