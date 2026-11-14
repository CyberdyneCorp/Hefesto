// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes MASK-INPUT-* of the mask area, included by
// src/core/src/mask/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_MASK_CODE("MASK-INPUT-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/mask.md with `hefesto-cli --diagnostic-codes=mask`
// and describe the codes in its INPUT section.
//
// Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_MASK_CODE("MASK-INPUT-001", error, "Error in a `cifinput` section line (Magic's message); the statement is skipped or its value reset.")
HEFESTO_MASK_CODE("MASK-INPUT-002", warning, "Input style name that is a strict prefix of an earlier one (Q-MASK-302); native only.")
HEFESTO_MASK_CODE("MASK-INPUT-003", info, "The loaded input style's scale (`Input style <name>: scaleFactor=<S>, multiplier=<M>`).")
HEFESTO_MASK_CODE("MASK-INPUT-004", error, "`cif istyle`: the name is a prefix of more than one input style and equals none.")
HEFESTO_MASK_CODE("MASK-INPUT-005", error, "`cif istyle`: no input style starts with the name.")
HEFESTO_MASK_CODE("MASK-INPUT-006", warning, "Decimal `scalefactor` with a unit keyword, where the magic-compat settings differ (Q-MASK-300); native only.")
HEFESTO_MASK_CODE("MASK-INPUT-007", warning, "`boundary` or `mask-hints` in a `layer` rule or a `copyup` templayer (Q-MASK-307); native only.")
HEFESTO_MASK_CODE("MASK-INPUT-008", warning, "`labels` under a templayer ignored (Q-MASK-311: magic-compat OFF, or an index that is no layout type); native only.")
HEFESTO_MASK_CODE("MASK-INPUT-009", warning, "Input scale normalisation truncated the scale factor or a distance, as Magic does; magic-compat OFF only.")
HEFESTO_MASK_CODE("MASK-INPUT-010", warning, "Read geometry off the internal grid: the grid was subdivided (`Input off lambda grid by <r>/<d>; grid redefined.`).")
HEFESTO_MASK_CODE("MASK-INPUT-011", warning, "Read geometry off the internal grid: snapped (`Input off lambda grid by <r>/<d>; snapped to grid.`).")
HEFESTO_MASK_CODE("MASK-INPUT-012", warning, "The read units were subdivided mid-read (`CIF style <name>: units rescaled by factor of <n> / <d>`).")
HEFESTO_MASK_CODE("MASK-INPUT-013", warning, "A boundary templayer redefined a cell's FIXED_BBOX (`Warning:  Cell <name> boundary was redefined.`).")
HEFESTO_MASK_CODE("MASK-INPUT-014", info, "Input `grow`/`shrink` while the output style has grow-euclidean (Q-MASK-306); native only, once per read.")
HEFESTO_MASK_CODE("MASK-INPUT-016", error, "Read engine used out of order by a format reader (a cell inside a cell, an end or a call without a cell).")
HEFESTO_MASK_CODE("MASK-INPUT-017", warning, "Copyup planes not copied into an array of more than 2^20 elements (resource cap); native only.")
HEFESTO_MASK_CODE("MASK-INPUT-015", info, "Input `tagged` rule run with a scale factor other than 1 (Q-MASK-310); native only, once per read.")
