// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes MASK-GEN-* of the mask area, included by
// src/core/src/mask/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_MASK_CODE("MASK-GEN-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/mask.md with `hefesto-cli --diagnostic-codes=mask`
// and describe the codes in its GEN section.
//
// Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_MASK_CODE("MASK-GEN-001", error, "A CIF layer name does not exist in the current output style.")
HEFESTO_MASK_CODE("MASK-GEN-002", error, "No CIF output style is set.")
HEFESTO_MASK_CODE("MASK-GEN-003", warning, "Mask generation: a parent and its child disagree on a layer's geometry.")
HEFESTO_MASK_CODE("MASK-GEN-004", warning, "Mask generation: the hierarchy is deeper than the generators descend.")
