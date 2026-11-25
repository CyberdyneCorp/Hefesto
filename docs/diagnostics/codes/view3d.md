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

Codes of cross-sections (design D10, P2). Owner: INSPECT (M10 round 5:
SECTION, task 11.1). `view3d::section` returns its diagnostics in
`CrossSection::diagnostics`; the caller (the desktop's section panel)
reports them.

- `VIEW3D-SECTION-001` (native error): the cut line is not axis-aligned,
  or both its ends are the same point. The section is empty (no
  intervals, no layers listed). The desktop draws cut lines with two
  clicks snapped to an axis, so it only reaches this for a zero-length
  line.
- `VIEW3D-SECTION-002` (native warning): in layout-layer mode, layers the
  line crosses have no physical height (no extract style, or no µm per
  internal unit because there is no `cifoutput` scale); they are left out
  of the section and named. In mask-layer mode the same case is
  `VIEW3D-STACK-001`, which the section emits naming only the mask layers
  the line crosses.

Positions and heights are µm in both magic-compat modes (Q-MISC-72); a
line outside the model gives an empty section without a diagnostic.

The renderer's section clipping plane and the board's substrate slab
(`render::DrawParams::section_plane`, `render::DrawParams::substrate`,
`view3d::substrate_slab`; RENDER3D and SECTION, round 5) report nothing:
a non-finite plane position clips nothing, and a slab with an empty area,
non-finite heights or no layer index left is not drawn.

## EXPORT

Codes of `export3d` and the glTF and STL writers (design D14, P2). Owner:
EXPORT.

`export3d <file> [-format gltf|glb|stl] [-merged] [-zscale k]
[-visible|-all]` is a Hefesto command (not Magic's; listed by `help` only
with magic-compat off). It exports the open 3D window's current model, as
the window builds it (its stack and cut box), in physical µm without the
view's z exaggeration; `-zscale k` multiplies the heights and k is
recorded in the glTF `asset.extras.hefesto.zscale`. The format follows the
extension (`.gltf`, `.glb`, `.stl`, any case) unless `-format` names one.
`.gltf` writes the JSON and, when there is geometry, `<stem>.bin` beside it
(an export with no layer writes the JSON only); STL writes one file per
exported layer, `<stem>_<layer>.stl` in the same directory (characters other
than letters, digits and `_-.+` become `_`; a name equal to an earlier one
ignoring case gets `_2`, `_3`, ... before `.stl`), or with `-merged` one
file. The result is the list of files written; each is replaced atomically.
A layer is exported when it is visible (`-visible`, the default; `-all`
takes every layer), drawn (a solid or stippled style) and has a physical
height and geometry; a type in two display styles is exported once. With
magic-compat OFF the temporary mask layers are never exported, with `-all`
as well. The desktop's
synthetic substrate slab is never exported. The writers and the file layout
are described in `src/core/include/hefesto/view3d/export/export.hpp` and
`command.hpp`.

- `VIEW3D-EXPORT-001` (error): `export3d: no 3D window is open`. Nothing is
  written.
- `VIEW3D-EXPORT-002` (error): the usage line `Usage: export3d file [-format
  gltf|glb|stl] [-merged] [-zscale k] [-visible|-all]` for no file, two
  files, an unknown option, an unknown `-format` value or a `-zscale` value
  that is not a positive number; `export3d: cannot tell the format of
  "<file>"; ...` for a file whose extension is not `.gltf`, `.glb` or `.stl`
  without `-format`. Nothing is written.
- `VIEW3D-EXPORT-003` (native warning): `No physical height for layer(s):
  <names>; they are left out of the export (...)` names the layout-layer
  layers with geometry whose height has no source (no extract style) or no
  µm scale (no output style). Mask layers without a `render` height are
  reported with `VIEW3D-STACK-001` instead (Q-MISC-72).
- `VIEW3D-EXPORT-004` (error): `export3d: the 3D window has no model (cell
  "<name>" not found)`. Nothing is written.
- `VIEW3D-EXPORT-005` (error): `export3d: the model is too large for the
  format (GLB over 4 GiB)` or `(STL with <n> triangles (at most
  4294967295))`: GLB stores its lengths and STL its triangle count in 32
  bits. Checked before writing; nothing is written.

Errors of the file system (a directory that does not exist, no permission)
are reported with their own codes, and the files written before the error
stay.

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
| `VIEW3D-CLIENT-003` | error | The 3D window cannot open: no cell to render, or the cell does not exist (a closed window's toolkit path reopens in both modes, Q-MISC-80). |
| `VIEW3D-CLIENT-004` | error | A `zoom` value or factor is zero or negative (Magic's `Error: zoom values/factors must be positive and nonzero`). |
| `VIEW3D-CLIENT-005` | error | The command needs a current output style and there is none (Magic's `No CIF output style set!`; Q-MISC-73). |
| `VIEW3D-CLIENT-006` | error | No mask layer of the current output style has the name given to `render` or `see`. |
| `VIEW3D-CLIENT-007` | error | A command sent to the 3D window is not one of its commands (Magic's `Unknown command:`, followed by the command on standard output). |
| `VIEW3D-CLIENT-008` | info | Output of a 3D window command printed on standard output (`help`, the command echoed after `Unknown command:`). |
| `VIEW3D-CLIENT-009` | error | A 3D window command was run while no 3D window is open. |
| `VIEW3D-EXPORT-001` | error | `export3d` was run while no 3D window is open; nothing is written. |
| `VIEW3D-EXPORT-002` | error | `export3d` was given the wrong arguments (no file, an unknown option or format, or a z scale that is not a positive number); nothing is written. |
| `VIEW3D-EXPORT-003` | warning | Layout-layer layers with geometry have no physical height (no extract-style height, or no µm scale without an output style), so they are left out of the export (names the layers). |
| `VIEW3D-EXPORT-004` | error | The 3D window has no model to export (its cell no longer exists); nothing is written. |
| `VIEW3D-EXPORT-005` | error | The model is too large for the format (a GLB over 4 GiB, or an STL with more than 4294967295 triangles, sizes the format stores in 32 bits); nothing is written. |
| `VIEW3D-SECTION-001` | error | The cross-section's cut line is not axis-aligned or has zero length; the section is empty. |
| `VIEW3D-SECTION-002` | warning | Layers crossed by the cut line have no physical height in layout-layer mode (no extract style, or no µm per internal unit); they are left out of the cross-section (names the layers). |
| `VIEW3D-STACK-001` | warning | Mask layers of the output style have no `render` height, so they have no physical height and are left out of µm-based output (names the layers; Q-MISC-72). |
| `VIEW3D-STACK-002` | error | A `render` statement's style is neither a display style number nor a long name (Magic's `Error:  Bad render style for CIF layer.`); the statement is ignored. |
<!-- END DIAGNOSTIC CODES -->
