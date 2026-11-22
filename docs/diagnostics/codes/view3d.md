# Diagnostic codes: view3d area

Codes `VIEW3D-<TOPIC>-<NNN>` emitted by `src/core/src/view3d/` (the 3D view:
layer stack, mesh model, camera, the `wind3d` client, picking,
cross-sections, export and overlays; capabilities view-3d and
hefesto-3d-visualization; `openspec/changes/m10-3d-view/design.md` D2/D3).
Conventions and the renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/view3d/codes.cpp` (owner:
SCAFFOLD) from one fragment per workstream, each owned by that workstream.
The rows are sorted while compiling (`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/view3d/stack/codes.inc` | `VIEW3D-STACK-*` | TECH |
| `src/core/src/view3d/mesh/codes.inc` | `VIEW3D-MESH-*` | MESH |
| `src/core/src/view3d/camera/codes.inc` | `VIEW3D-CAMERA-*` | CAMERA |
| `src/core/src/view3d/client/codes.inc` | `VIEW3D-CLIENT-*` | WIND3D |
| `src/core/src/view3d/pick/codes.inc` | `VIEW3D-PICK-*` | INSPECT |
| `src/core/src/view3d/section/codes.inc` | `VIEW3D-SECTION-*` | INSPECT (P2) |
| `src/core/src/view3d/export/codes.inc` | `VIEW3D-EXPORT-*` | EXPORT (P2) |
| `src/core/src/view3d/overlay/codes.inc` | `VIEW3D-OVERLAY-*` | OVERLAY (P3) |

The extract-style height subset that the layout-layer stack reads
(`src/core/src/extract/tech/`, design D5) reports with the `EXTRACT-STYLE-*`
codes of [extract.md](extract.md), since it is part of the one
`extract-tech-section` reader.

The scaffold (tasks.md 2.2, 2.3) declared no code: the area-contract stubs
answer neutral values (an empty mesh, Magic's zero view) and no command
reaches them until their workstream lands them. TECH (tasks 3.1-3.4) adds
the STACK codes below, WIND3D (tasks 8.1-8.4) the CLIENT codes.

## STACK

Codes of the layer stack (`view3d::LayerStack`, design D5). Owner: TECH.

- `VIEW3D-STACK-001` (native warning) names the non-temporary mask layers
  of the output style with no `render` statement, which have no physical
  height (Q-MISC-72): `No render height for mask layer(s): <names>; they
  are left out ...`. `view3d::missing_height_warning` builds it;
  cross-sections and export from mask-layer mode emit it (gf180mcuD has no
  `render` lines, so every mask layer is named).
- `VIEW3D-STACK-002` is Magic's `Error:  Bad render style for CIF layer.`
  for a `render` statement whose style is neither a display style number
  nor an exact long name. Magic reports it while loading the `cifoutput`
  section; Hefesto loads the display styles after that section (and a
  headless session has none), so the reference is kept as written
  (`mask::RenderAttrs`) and `view3d::build_mask_stack` resolves it,
  reports each `render` line with a bad reference once per build (once
  per line, as Magic, also when the line names several layers) and
  ignores that statement, so the layer keeps its earlier `render` values
  as in Magic. The other `render` errors (argument count, `Syntax: render
  <layer> <style> <height> <thick>`, `Unknown layer name.`) are reported
  at load with `MASK-STYLE-001` ([mask.md](mask.md)). Two differences
  from Magic await a decision (m10-3d-view tasks.md 3.4, integration note
  (a)): Magic checks the style first, so a line with a bad style and bad
  numbers or an unknown layer prints only this error, where Hefesto
  prints the load-time error and never this one; and the error comes
  again on each stack build rather than once per tech load.

## MESH

Codes of the mesh model and its builders (design D6). Owner: MESH.

## CAMERA

Codes of the view state, camera, presets and gestures (design D7). Owner:
CAMERA.

The camera has no codes: its operations cannot fail. Inputs Magic would
divide by (an empty cell bounding box, a zero or negative scalefactor, a
non-positive zoom factor, a zero-size drawable) leave the view unchanged or
fall back to a finite value instead; the commands that take such values
report their own usage errors (WIND3D).

## CLIENT

Codes of the `wind3d` client and its commands (design D9). Owner: WIND3D.
Magic's own messages (usage lines, "Only one 3D window allowed.", "No CIF
output style set!") keep Magic's text; the code names the condition. As in
Magic, every one of them is printed and none raises a Tcl error (the window
command returns normally).

- `VIEW3D-CLIENT-001`: each command's usage line, as W3Dmain.c prints it
  (`Usage: view angle_x angle_y angle_z absolute|relative`, `Usage: level
  [n]`, `Usage: cutbox [none|box|llx lly urx ur]`, ...). A value that is not
  numeric is no usage error: as in Magic, the command then changes nothing
  and prints nothing.
- `VIEW3D-CLIENT-002`: `Only one 3D window allowed.`
- `VIEW3D-CLIENT-003`: W3Dcreate's refusals, after the one-window check and
  in its order: `Ambiguous directive:  Put cursor box in one of the
  windows.` (no cell named, no box, not exactly one layout window); `Cells
  cannot be created in the 3D window.` (the cell does not exist, or cannot
  be read). A closed window's toolkit path reopens in both modes
  (Q-MISC-80: Magic deletes the path on close, MISC-3D-12).
- `VIEW3D-CLIENT-004`: `Error: zoom values/factors must be positive and
  nonzero`.
- `VIEW3D-CLIENT-005`: `No CIF output style set!` from `render` with a
  layer name, `see` in mask-layer mode and `cif` in layout-layer mode when
  there is no current output style (Q-MISC-73 amendment; Magic dereferences
  the missing style, or for `cif` silently keeps the mode).
- `VIEW3D-CLIENT-006`: `Unknown CIF layer "NAME"` (`render`) and CIFNameToMask's
  `CIF name "NAME" doesn't exist in style "STYLE".` followed by `The valid CIF
  layer names are: A, B.` (`see` in mask-layer mode).
- `VIEW3D-CLIENT-007` and `-008`: a word sent to the 3D window
  (`.magic3d WORD ...`) that is none of its commands: `Unknown command:` on
  standard error with no newline, then the words and the window point on
  standard output (` 'WORD' 'ARG' at (20, 20)`), as WindSendCommand and
  windPrintCommand print them. The window takes no window-client commands
  (W3Dcreate clears WIND_COMMANDS), so a layout command is unknown there.
  `-008` also carries the `help` summary.
- `VIEW3D-CLIENT-009`: a `wind3d` command run with no 3D window open
  (`send wind3d zoom`); Magic cannot reach the table then.

## PICK

Codes of the pick mapping, the 3D/2D selection link and net highlighting
(design D10). Owner: INSPECT.

Picking has no codes (M10 round 3): the background, an id past the model
and a stale target (its tile edited away, or its cell deleted, since the
mesh was built) resolve or select nothing instead of failing, and the
highlight sets of an empty selection or a failed connectivity search are
empty.

## SECTION

Codes of cross-sections (design D10, P2). Owner: INSPECT.

## EXPORT

Codes of `export3d` and the glTF and STL writers (design D14, P2). Owner:
EXPORT.

## OVERLAY

Codes of the overlay datasets and color maps (design D16, P3). Owner:
OVERLAY.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=view3d
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=view3d`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `VIEW3D-CLIENT-001` | error | A 3D window command was given the wrong arguments (Magic's `Usage: ...` line); nothing changes. |
| `VIEW3D-CLIENT-002` | error | A 3D window is already open (Magic's `Only one 3D window allowed.`). |
| `VIEW3D-CLIENT-003` | error | The 3D window cannot open: no cell to render, the cell does not exist, or (magic-compat ON) the toolkit path of a closed 3D window (user decision J5). |
| `VIEW3D-CLIENT-004` | error | A `zoom` value or factor is zero or negative (Magic's `Error: zoom values/factors must be positive and nonzero`). |
| `VIEW3D-CLIENT-005` | error | The command needs a current output style and there is none (Magic's `No CIF output style set!`; Q-MISC-73). |
| `VIEW3D-CLIENT-006` | error | No mask layer of the current output style has the name given to `render` or `see`. |
| `VIEW3D-CLIENT-007` | error | A command sent to the 3D window is not one of its commands (Magic's `Unknown command:`, followed by the command on standard output). |
| `VIEW3D-CLIENT-008` | info | Output of a 3D window command printed on standard output (`help`, the command echoed after `Unknown command:`). |
| `VIEW3D-CLIENT-009` | error | A 3D window command was run while no 3D window is open. |
| `VIEW3D-STACK-001` | warning | Mask layers of the output style have no `render` height, so they have no physical height and are left out of µm-based output (names the layers; Q-MISC-72). |
| `VIEW3D-STACK-002` | error | A `render` statement's style is neither a display style number nor a long name (Magic's `Error:  Bad render style for CIF layer.`); the statement is ignored. |
<!-- END DIAGNOSTIC CODES -->
