// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes MASK-OPER-* of the mask area, included by
// src/core/src/mask/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_MASK_CODE("MASK-OPER-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/mask.md with `hefesto-cli --diagnostic-codes=mask`
// and describe the codes in its OPS section.
//
// Owner: OPS (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_MASK_CODE("MASK-OPER-001", warning, "Mask generation: a bloat-min or bloat-max tile was inverted by shrink.")
HEFESTO_MASK_CODE("MASK-OPER-002", warning, "Mask generation: no room for contact cuts in a contact strip.")
HEFESTO_MASK_CODE("MASK-OPER-003", warning, "Mask generation: a shortened contact strip has no room for cuts.")
HEFESTO_MASK_CODE("MASK-OPER-004", warning, "Mask generation: no room for contact cuts in a contact region.")
