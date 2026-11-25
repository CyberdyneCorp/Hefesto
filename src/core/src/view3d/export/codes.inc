// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes VIEW3D-EXPORT-* of the view3d area, included by
// src/core/src/view3d/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_VIEW3D_CODE("VIEW3D-EXPORT-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/view3d.md with `hefesto-cli --diagnostic-codes=view3d`
// and describe the codes in its EXPORT section.
//
// Owner: EXPORT (P2) (openspec/changes/m10-3d-view/design.md D2/D3).
HEFESTO_VIEW3D_CODE("VIEW3D-EXPORT-001", error, "`export3d` was run while no 3D window is open; nothing is written.")
HEFESTO_VIEW3D_CODE("VIEW3D-EXPORT-002", error, "`export3d` was given the wrong arguments (no file, an unknown option or format, or a z scale that is not a positive number); nothing is written.")
HEFESTO_VIEW3D_CODE("VIEW3D-EXPORT-003", warning, "Layout-layer layers with geometry have no physical height (no extract-style height, or no µm scale without an output style), so they are left out of the export (names the layers).")
HEFESTO_VIEW3D_CODE("VIEW3D-EXPORT-004", error, "The 3D window has no model to export (its cell no longer exists); nothing is written.")
HEFESTO_VIEW3D_CODE("VIEW3D-EXPORT-005", error, "The model is too large for the format (a GLB over 4 GiB, or an STL with more than 4294967295 triangles, sizes the format stores in 32 bits); nothing is written.")
