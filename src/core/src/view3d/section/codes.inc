// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes VIEW3D-SECTION-* of the view3d area, included by
// src/core/src/view3d/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_VIEW3D_CODE("VIEW3D-SECTION-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/view3d.md with `hefesto-cli --diagnostic-codes=view3d`
// and describe the codes in its SECTION section.
//
// Owner: INSPECT (P2) (openspec/changes/m10-3d-view/design.md D2/D3).
HEFESTO_VIEW3D_CODE("VIEW3D-SECTION-001", error, "The cross-section's cut line is not axis-aligned or has zero length; the section is empty.")
HEFESTO_VIEW3D_CODE("VIEW3D-SECTION-002", warning, "Layers crossed by the cut line have no physical height in layout-layer mode (no extract style, or no µm per internal unit); they are left out of the cross-section (names the layers).")
