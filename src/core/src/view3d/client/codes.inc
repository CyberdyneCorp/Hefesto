// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Diagnostic codes VIEW3D-CLIENT-* of the view3d area, included by
// src/core/src/view3d/codes.cpp (rows are sorted there). One line per code:
//   HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-NNN", error|warning|info|debug, "One-line summary.")
// Never renumber or reuse a code once released; regenerate the table of
// docs/diagnostics/codes/view3d.md with `hefesto-cli --diagnostic-codes=view3d`
// and describe the codes in its CLIENT section.
//
// Owner: WIND3D (openspec/changes/m10-3d-view/design.md D2/D3).
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-001", error, "A 3D window command was given the wrong arguments (Magic's `Usage: ...` line); nothing changes.")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-002", error, "A 3D window is already open (Magic's `Only one 3D window allowed.`).")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-003", error, "The 3D window cannot open: no cell to render, the cell does not exist, or (magic-compat ON) the toolkit path of a closed 3D window (user decision J5).")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-004", error, "A `zoom` value or factor is zero or negative (Magic's `Error: zoom values/factors must be positive and nonzero`).")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-005", error, "The command needs a current output style and there is none (Magic's `No CIF output style set!`; Q-MISC-73).")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-006", error, "No mask layer of the current output style has the name given to `render` or `see`.")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-007", error, "A command sent to the 3D window is not one of its commands (Magic's `Unknown command:`, followed by the command on standard output).")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-008", info, "Output of a 3D window command printed on standard output (`help`, the command echoed after `Unknown command:`).")
HEFESTO_VIEW3D_CODE("VIEW3D-CLIENT-009", error, "A 3D window command was run while no 3D window is open.")
