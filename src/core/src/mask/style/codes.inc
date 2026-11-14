// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes MASK-STYLE-* of the mask area, included by
// src/core/src/mask/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_MASK_CODE("MASK-STYLE-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/mask.md with `hefesto-cli --diagnostic-codes=mask`
// and describe the codes in its STYLES section.
//
// Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D1/D2).

HEFESTO_MASK_CODE("MASK-STYLE-001", error, "Error in a `cifoutput` section line (Magic's message); the statement is skipped or its value reset.")
HEFESTO_MASK_CODE("MASK-STYLE-002", warning, "Warning for a `cifoutput` section line (Magic's message); the statement still applies.")
HEFESTO_MASK_CODE("MASK-STYLE-003", warning, "Output style name that Magic's prefix test would drop; registered at the end of the list (Q-MASK-10); native only.")
HEFESTO_MASK_CODE("MASK-STYLE-004", warning, "Non-zero `gridlimit` in a style whose expander is not 10: applied in mask units (Q-MASK-8); native only.")
HEFESTO_MASK_CODE("MASK-STYLE-005", error, "`cif ostyle`: the name is a prefix of more than one output style.")
HEFESTO_MASK_CODE("MASK-STYLE-006", error, "`cif ostyle`: no output style has the name or starts with it.")
