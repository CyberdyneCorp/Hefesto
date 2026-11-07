# Wiring Tool

## Purpose

Defines Magic's wiring interface: the `wiring` technology section (contact definitions used for layer switches), the `wire` command and all its subcommands (type selection, legs, contacts/switching, increment/decrement, show, values, width, and `segment` for arbitrary-angle centerline wires), and the exact geometry each operation paints. The interaction surface (mouse buttons of the wiring tool) is behavioral; painted geometry is exact.

## Requirements

### Requirement: wiring technology section
The `wiring` section SHALL be parsed as:

```ebnf
wiring_section = "wiring" , NL , { wiring_stmt , NL } , "end" ;
wiring_stmt    = scalefactor | contact7 | contact9 ;
scalefactor    = "scalefactor" , int ;
contact7       = "contact" , ctype , size , layer1 , surround1 , layer2 , surround2 ;
contact9       = "contact" , ctype , size , layer1 , surround1 , extend1 , layer2 , surround2 , extend2 ;
size, surround1, surround2, extend1, extend2 = int ;
```

`size` is the contact cut size, `surroundN` the overlap of layer N around the cut on all sides, `extendN` an additional overlap of layer N on the two sides along/across the wire direction (see "Contact placement"). `scalefactor` (default 1) divides all contact dimensions when used, rounding up. Errors (line ignored): `"scalefactor" line must have exactly 2 arguments.`, `"scalefactor" argument must be an integer.`, `Unknown wiring keyword: %s.  Line ignored.`, `"contact" lines must have exactly 7 or 9 arguments.`, non-integer size/surround/extend (`Contact size must be an integer.` etc.), unknown type names. Contacts are kept in REVERSE file order (each new line is prepended), which determines search order. Dimensions scale with the internal grid (multiply, then truncating divide).

Example (scmos): `contact m2contact 4 metal1 0 metal2 0`.

Compatibility: exact
(Magic ref: wiring/wireTech.c:40-170, scmos/scmos.tech.in wiring section)

#### Scenario: Seven-field contact
- **GIVEN** `contact m2contact 4 metal1 0 metal2 0`
- **WHEN** parsed
- **THEN** a contact m2contact of size 4 between metal1 (surround 0) and metal2 (surround 0) with extends 0 is defined

### Requirement: wire command dispatch
`wire` SHALL accept a first argument matched case-insensitively as a unique prefix of: `decrement`, `help`, `horizontal`, `increment`, `leg`, `show`, `switch`, `type`, `values`, `vertical`, `width`, `segment`. With no argument, or an invalid one (`"%s" isn't a valid wire option.`), it prints the help list (`Wiring commands have the form ":wire option", where option is one of:` followed by each entry indented 4 spaces). Wrong argument forms print `Wrong arguments.  The correct syntax is` and the option's usage. Commands that read the cursor position (`type`, `leg`, `horizontal`, `vertical` without explicit coordinates) are recorded in the command log as `wire <option> at|to <x>i <y>i` with the edit-cell coordinates of the cursor so that replay does not depend on the pointer.

Compatibility: behavioral
(Magic ref: commands/CmdTZ.c:1689-1740, commands/CmdTZ.c:2040-2060)

#### Scenario: Help on bad option
- **WHEN** `wire bogus` is executed
- **THEN** `"bogus" isn't a valid wire option.` is reported followed by the help list

### Requirement: Wire type selection
`wire type` SHALL set the current wiring layer and width:
- `wire type layer [width]`: layer by name (`Layer name "%s" doesn't exist.` / `... is ambiguous.`); width given, or the larger of the current width and the layer's DRC minimum width.
- `wire type` (cursor) or `wire type at x y`: look at the visible, non-space types under the point (1×1 area at the point); if none: `There's no material visible underneath the cursor.`. Starting from the type after the current one, cycle upward through type numbers (wrapping to the first user type) to the first type present, so repeated invocations cycle through overlapping layers. Select the maximal rectangular chunk of that type containing the point; width = min(chunk width, chunk height). Set the box to a width×width square: for odd width, the 1×1 point area expanded by `width/2` (floor); for even width, lower-left at `(px - width/2, py - width/2)`; then shift it inside the chunk if it sticks out on any side. Select the material under that square, and print `Using %s wires %d units wide.`.
Any type change resets the last-leg direction.

Compatibility: exact
(Magic ref: wiring/wireOps.c:90-240, commands/CmdTZ.c:1870-1915)

#### Scenario: Pick from a wire
- **GIVEN** a metal1 rectangle (0,0)-(100,4) and the cursor at (50,2)
- **WHEN** `wire type` is executed
- **THEN** the wire type is metal1, width 4, the box becomes (48,0)-(52,4), and `Using metal1 wires 4 units wide.` is printed

### Requirement: Wire legs
`wire leg`, `wire horizontal`, `wire vertical` (optionally `to x y` in edit-cell coordinates, else the cursor point, which must be over the edit cell) SHALL paint a new leg starting from the box (which must be in the edit cell's root) with width w and half-width `h = floor(w/2)`:
- Direction for `leg`: `dx` = distance from the point to the box in x (0 if within the x-span), `dy` likewise; horizontal if `dx > dy`, else vertical.
- Horizontal: if the box height differs from w, the box is first recentered to a w×w square at its center (`mid = floor((lo+hi)/2)`, `lo = mid - h`). If `px > box.xtop`: leg x-span `[box.xbot, px + h]`, direction east; if `px < box.xbot`: `[px - h, box.xtop]`, west; otherwise nothing is painted. Leg y-span: `ybot = py - h` clamped to `[box.ybot, box.ytop - w]`, height w.
- Vertical: symmetric.
- The leg is painted with the current type (respecting the edit cell's paint rules), labels adjusted, DRC re-checked; the selection becomes the connected chunk of that type under the leg; the box becomes a w×w square at the tip of the leg (extended back to the selected chunk if shorter). Without a wiring type: `Can't add a new wire leg:  no wiring material selected.` (see Q-LEGACY-51; window handling Q-LEGACY-52); without a box: `No box!  Before wiring a leg, you must set the box` / `    to indicate where the leg starts.`.

Compatibility: exact
(Magic ref: wiring/wireOps.c:240-420, commands/CmdTZ.c:1800-1850, commands/CmdTZ.c:1950-1975)

#### Scenario: Horizontal leg to the right
- **GIVEN** wire type metal1 width 4 and the box (48,0)-(52,4)
- **WHEN** `wire horizontal to 80 2` is executed
- **THEN** metal1 is painted over (48,0)-(82,4) and the box becomes (78,0)-(82,4)

### Requirement: Contact placement (wire switch, increment, decrement)
`wire switch [layer width]` (or `switch` using the type under the cursor, as `wire type`) SHALL change the wiring layer and place a contact at the end of the last leg (marked by the box): if the new type equals the old: `The new wiring layer is the same as the old one, so` / `    there's no need for a contact.`. The contact is the first wiring contact (in list order) connecting old→new as (layer1, layer2) — "up" — or (layer2, layer1) — "down"; none: `The technology file doesn't define a contact` / `    between "%s" and "%s".`. With all dimensions divided by scalefactor rounding up, `oldOverlap` = surround of the old layer, `newOverlap` = surround of the new layer:
1. `T = size + 2*oldOverlap`; if `T < newWidth - 2*newOverlap` then `T = newWidth - 2*newOverlap` (where newWidth is the new wire width).
2. Start from the box; in each axis where the box is smaller than T, grow it to T centered (`lo -= (T - extent)/2` truncating, `hi = lo + T`); then along the last leg direction, keep only the T-long part at the leg's end.
3. Paint the contact type over that area shrunk by `oldOverlap`; layer1 over the contact area expanded by surround1 (if nonzero); layer2 expanded by surround2 (if nonzero); extensions: layer N expanded by extendN along one axis and surroundN along the other, the axis depending on the leg direction and up/down sense.
4. Select the contact and surrounds, set the box, re-check DRC.
`wire increment layer|type` / `wire decrement layer|type` switch to the unique layer reachable through a contact whose layer1 (increment) or layer2 (decrement) is the current type (width = max(current width, DRC minimum width of the new layer)); multiple candidates: `Ambiguous directive---multiple routing types`; none: `No routing layer defined`. `wire increment|decrement width [n]` changes the width by n (default 1).

Compatibility: exact
(Magic ref: wiring/wireOps.c:560-860, commands/CmdTZ.c:1740-1800, commands/CmdTZ.c:1850-1870)

#### Scenario: Contact at end of an east leg
- **GIVEN** wire type metal1 width 4, last leg east with box (78,0)-(82,4), wiring contact `m2contact 4 metal1 0 metal2 0`
- **WHEN** `wire switch metal2 4` is executed
- **THEN** m2contact is painted over (78,0)-(82,4) and the wire type becomes metal2 width 4

### Requirement: wire show, values and width
`wire show` SHALL compute the leg that `wire leg` would paint from the box to the cursor and display it as the selection only (no layout change, not undoable); it silently does nothing when the box, cursor or wire type is missing. `wire values` returns the two-element list {layer, width}, with the width formatted in the current display units. `wire width` returns the width, formatted the same way; `wire width value` sets the width keeping the type. Informative: Magic's non-Tcl build prints `Wire layer %s, width %s` and `Wire width is %s` instead.

Compatibility: behavioral
(Magic ref: wiring/wireOps.c:420-560, commands/CmdTZ.c:1915-1950, commands/CmdTZ.c:1975-1995)

#### Scenario: Width query
- **GIVEN** the wire width is 4 and display units are internal units
- **WHEN** `wire width` is executed
- **THEN** the result is `4`

### Requirement: wire segment
`wire segment layer width x1 y1 x2 y2 ... [-noendcap]` or `wire segment layer width filename [-noendcap]` SHALL paint a centerline path of the given width on every plane of the layer in the edit cell, without using the current wire state:
- At least two points (argument count after the optional `-noendcap` must be even and >= 8, or exactly 5 for the file form); otherwise `Usage: wire segment layer width x1 x2 y1 y2` / ` (or)  wire segment layer width filename` (see Q-LEGACY-53). Non-numeric width: `Route segment width must be a numeric value`. No edit cell: `No cell being edited`.
- The file form reads one `x y` pair per line (whitespace separated); a missing coordinate reports `Bad coordinate pair at %s line %d` and aborts; unopenable file `No such file or error opening %s`.
- Coordinates are parsed at twice the internal resolution so that centerlines on half-grid points are exact; the path is converted to polygons with the same wire-path algorithm used for CIF/GDS paths (capability `gds-read`): each segment is offset by half the width on both sides, mitred joints (mitre limited to 4× width), and, unless `-noendcap`, each end extended by half the width; resulting vertices are rounded to the internal grid.
- The cell is marked modified and DRC is re-checked over the bounding box of the points expanded by `round(width × 1.414214)` half-units.

Compatibility: exact
(Magic ref: commands/CmdTZ.c:1995-2040, cif/CIFrdpt.c:300-700)

#### Scenario: Horizontal segment with end caps
- **GIVEN** layer metal1
- **WHEN** `wire segment metal1 4 0 0 20 0` is executed
- **THEN** metal1 is painted over (-2,-2)-(22,2)

#### Scenario: No end caps
- **WHEN** `wire segment metal1 4 0 0 20 0 -noendcap` is executed
- **THEN** metal1 is painted over (0,-2)-(20,2)

### Requirement: Wiring tool buttons
When the wiring tool is the active tool, the pointer bindings SHALL provide this workflow: button 1 starts a wire (`wire type` at the cursor) and enters a tracking mode in which pointer motion continuously previews the next leg (`wire show`); while tracking, button 1 commits a leg (`wire leg`) and continues, button 2 commits a leg and then places a contact (`wire switch`) ending tracking, button 3 cancels tracking (clearing the preview); control-button 1 starts tracking with the current wire type/width (box set to a width×width square at the cursor); shift-button 1/3 = `wire increment type` / `wire decrement type` then `wire show`; shift-button 2 = `wire switch`; wheel up/down = `wire increment width` / `wire decrement width` then `wire show`. The wiring state (type, width, last direction) participates in undo/redo. These bindings are defined by the scripting layer and are behavioral; Hefesto MAY map them to touch gestures.

Compatibility: behavioral
(Magic ref: tcltk/tools.tcl:545-640, wiring/wireUndo.c:80-170)

#### Scenario: Undo restores wiring state
- **GIVEN** `wire type metal1 4` followed by `wire type metal2 6`
- **WHEN** undo is executed
- **THEN** the wiring layer and width return to metal1 and 4
