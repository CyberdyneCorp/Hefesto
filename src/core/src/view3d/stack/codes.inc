// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes VIEW3D-STACK-* of the view3d area, included by
// src/core/src/view3d/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_VIEW3D_CODE("VIEW3D-STACK-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/view3d.md with `hefesto-cli --diagnostic-codes=view3d`
// and describe the codes in its STACK section.
//
// Owner: TECH (openspec/changes/m10-3d-view/design.md D2/D3).
HEFESTO_VIEW3D_CODE("VIEW3D-STACK-001", warning, "Mask layers of the output style have no `render` height, so they have no physical height and are left out of µm-based output (names the layers; Q-MISC-72).")
HEFESTO_VIEW3D_CODE("VIEW3D-STACK-002", error, "A `render` statement's style is neither a display style number nor a long name (Magic's `Error:  Bad render style for CIF layer.`); the statement is ignored.")
