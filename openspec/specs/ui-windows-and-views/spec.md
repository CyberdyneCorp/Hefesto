# UI Windows and Views

## Purpose
Defines windows and what is drawn in them:
- the window model (ids, limits, stacking order, chrome flags)
- the window commands `openwindow`, `closewindow`, `specialopen`, `over`, `underneath`, `grow`, `windowborder`, `windowcaption`, `windowscrollbars`, `windowpositions`, `windownames`
- the view commands `view`, `xview`, `zoom`, `scroll`, `center`, `findbox`
- the refresh commands `redraw`, `reset`, `updatedisplay`
- captions, the crosshair, per-window expansion, the `see` command and `grid`
- the redisplay rules for layout windows: paint styles, edit vs non-edit (pale) display, labels and ports, subcell outlines and names, the grid, the box, highlights and DRC error display

Style and file formats are in `display-style-files`. Feedback and highlight layering are in `ui-feedback-and-highlights`. The box commands are owned by `cmd-box`. Units and coordinate parsing are owned by `cmd-coordinates-and-units`. The color window is in `ui-color-window`.

## Requirements

### Requirement: Window identity and limits
Each window SHALL have:
- an integer id, which is the smallest non-negative integer not used by an open window
- a client
- a frame area, which is the whole window
- a screen area, which is the drawing area: the frame minus chrome
- a surface area, which is the region of layout coordinates mapped onto the screen area

At most 31 windows SHALL be open. Creating one more SHALL report `Can't have more than 31 windows.` and fail. The client MAY refuse a new window, in which case nothing is created. Windows SHALL be kept in a front-to-back stacking order. A new window SHALL go to the front.

Compatibility: behavioral
(Magic ref: windows/windMove.c:49-60, windows/windMove.c:256-388, dbwind/DBWprocs.c:84-173, dbwind/DBWprocs.c:1124-1125)

#### Scenario: Id reuse
- **GIVEN** windows 0, 1 and 2 are open
- **WHEN** window 1 is closed and a new window is opened
- **THEN** the new window SHALL get id 1

#### Scenario: Window limit
- **GIVEN** 31 windows are open
- **WHEN** `openwindow` is executed
- **THEN** `Can't have more than 31 windows.` SHALL be reported and no window SHALL be created

### Requirement: Window chrome flags
Every new window SHALL copy the session defaults for border, caption and scroll bars. All three are on initially. The `layout` client SHALL also be scrollable and SHALL accept window commands.

The commands that change the defaults SHALL affect only windows created later:
- `windowborder [on|off]` SHALL print `New windows will have a border.` or `New windows will not have a border.`
- `windowcaption [on|off]` SHALL print `New windows will have a title caption.` or `New windows will not have a title caption.`
- `windowscrollbars on|off` SHALL print `New windows will have scroll bars.` or `New windows will not have scroll bars.`

Values are matched by prefix against `on`, `off`.

Query forms:
- `windowborder` with no argument SHALL return the pointed window's border state, `on` or `off`, as the command result (Magic's Tcl build; the non-Tcl build prints `Window border is on|off`).
- `windowcaption` with no argument SHALL return the pointed window's caption as the command result (the non-Tcl build prints `Window caption is "<caption>"`).
- The setting forms print their message and return an empty result.
- With no window, either query SHALL report `No window specified for caption command` and print usage.
- `windowscrollbars` requires exactly one argument.

The `color` client SHALL turn off scrolling, scroll bars and caption for its windows.

Compatibility: behavioral (chrome geometry informative)
(Magic ref: windows/windCmdAM.c:81-220, windows/windCmdSZ.c:1001-1054, windows/windMove.c:49-51, cmwind/CMWmain.c:152-191)

#### Scenario: Caption default off
- **WHEN** `windowcaption off` and then `openwindow` are executed
- **THEN** the new window SHALL have no caption and existing windows SHALL keep theirs

### Requirement: Window chrome layout and frame interaction
Magic's own chrome (used when Magic manages its own windows) SHALL have this layout:
- the border is 2 pixels wide
- the caption strip has the height of the default font plus 3
- the scroll bars are 14 pixels wide on X11, OpenGL and Cairo (7 pixels on other displays)
- the left and bottom scroll bars have arrow icons at their ends
- a zoom icon sits in the lower-left corner

Mouse actions on the chrome:
- middle-click on a scroll bar SHALL center the view at the clicked fraction of the cell's bounding box
- middle-click on an arrow SHALL scroll by one screen-area width or height
- clicking the zoom icon SHALL zoom out by 2 (left), view all (middle) or zoom in by 2 (right)
- dragging the frame with the left or right button SHALL move or resize the window

Hefesto SHALL provide equivalent capabilities (pan to a position, page scroll, zoom in/out/fit, move/resize window) through its native UI. It MAY use different chrome (see Q-UI-40).

Compatibility: informative
(Magic ref: windows/windInt.h:60-92, windows/windDisp.c:386-668, windows/windClient.c:315-640)

#### Scenario: Zoom icon
- **WHEN** the user left-clicks the zoom icon of a layout window
- **THEN** the view SHALL zoom out by a factor of 2

### Requirement: View mapping
A window SHALL map layout coordinates to pixels using an integer scale s, expressed in 1/65536 pixel per layout unit, and a sub-pixel origin. Making surface rectangle R visible in screen area P SHALL compute:

```
s  = max(1, min( floor((P.w+1)*65536/(R.w+1)), floor((P.h+1)*65536/(R.h+1)) ))
      where P.w = P.xtop-P.xbot, R.w = R.xtop-R.xbot (same for h)
hu = floor(P.w*32768 / s) + 1
surface.xbot = trunc((R.xbot+R.xtop)/2) - hu ;  surface.xtop = surface.xbot + 2*hu + 1
origin.x     = (P.xtop+P.xbot)*32768 - hu*s
(y analogous)
screenX(X)   = (origin.x + (X - surface.xbot)*s) >> 16
```

If s would be below 1, the system SHALL warn `Warning:  At minimum scale!` and use s = 1.

A pixel SHALL map back to the nearest layout coordinate, rounding half away from zero:
```
X = surface.xbot + round((px*65536 - origin.x) / s)
```

Compatibility: behavioral (exact values depend on the drawing-area size; see Q-UI-16)
(Magic ref: windows/windView.c:154-234, windows/windTrans.c:48-355, windows/windows.h:35-37)

#### Scenario: Load view (verify against Magic)
- **GIVEN** a screen area (0,0)-(499,399) and a requested area (-10,-10)-(110,60)
- **WHEN** the window is moved to that area
- **THEN** s SHALL be 270809 and the surface area SHALL be (-11,-24)-(112,75)

### Requirement: Opening and closing windows
`openwindow [cell] [name]` SHALL create a `layout` window. The 300×300-pixel frame SHALL be centered on the command point, or on the screen center if the command came from no window (the frame position is a [platform] hint).

The new window SHALL show:
- the named cell, if given, loaded as when a cell is loaded into a window
- otherwise, if a box exists, the box's cell, zoomed so the box plus a margin on each side fills the window. The margin is max(2, ⌊width/20⌋) in x and max(2, ⌊height/20⌋) in y.
- otherwise the unnamed cell `(UNNAMED)`

`name` is a host window name. If creation fails, report `Could not create window`.

`closewindow [name]` SHALL close the named window, or the pointed window. Errors:
- name not found: `Window named N cannot be found`
- no pointed window: `Point to a window first`
- the client refuses: `Unable to close that window`

Closing a layout window SHALL free its expansion bit (see Requirement: Per-window expansion state).

Compatibility: behavioral
(Magic ref: windows/windCmdNR.c:67-113, windows/windCmdAM.c:321-374, dbwind/DBWprocs.c:84-173, windows/windMove.c:217-255)

#### Scenario: Open on box
- **GIVEN** a box at (0,0)-(100,40) in cell `inv`
- **WHEN** `openwindow` is executed
- **THEN** a new window SHALL show `inv`, framed to (-5,-2)-(105,42) before the view mapping is applied

### Requirement: Loading a cell into a window
When a cell is loaded into a layout window, the system SHALL:
1. Make the cell the window's root and expand the root in that window.
2. Frame the cell's bounding box B with a margin. In x the margin is max(0, ⌊(60 − B.w)/2⌋) + ⌊(B.w + 1)/10⌋, and y is analogous. A cell that does not exist on disk SHALL be framed as (0,0)-(1,1) before the margin.
3. If no other window shows the current edit cell, make the loaded cell the edit cell, unless it is marked non-editable.
4. Delete the previously shown `(UNNAMED)` cell if it was never modified.
5. Update all captions.

Compatibility: behavioral
(Magic ref: dbwind/DBWprocs.c:286-625)

#### Scenario: Load framing
- **GIVEN** a cell with bounding box (0,0)-(100,50)
- **WHEN** it is loaded into a window
- **THEN** the requested view area SHALL be (-10,-10)-(110,60)

### Requirement: Window captions
A layout window's caption SHALL be computed from its root cell R and the current edit cell E:
- If R is the edit cell's root, the caption SHALL be `R EDITING E`.
- Otherwise it SHALL be `R [NOT BEING EDITED]`.

Names longer than the limit SHALL be shortened. The limit is 90 characters per name in the EDITING form and 175 characters in the other form. A name of length L ≥ limit SHALL be replaced by `...` followed by its last (limit − 3) characters. The caption SHALL be truncated to 199 characters.

All layout windows' captions SHALL be recomputed whenever the edit cell changes. The window's icon name SHALL be R. A color window's caption SHALL be `COLOR = 0<octal index>`.

Compatibility: exact (caption text)
(Magic ref: commands/CmdSubrs.c:919-1115, cmwind/CMWmain.c:376-410)

#### Scenario: Editing caption
- **GIVEN** window root `top` and edit cell `inv` (a child of `top`)
- **THEN** the caption SHALL be `top EDITING inv`

#### Scenario: Other window
- **GIVEN** a second window showing `nand2`, while the edit cell is in `top`
- **THEN** its caption SHALL be `nand2 [NOT BEING EDITED]`

### Requirement: specialopen, over, underneath and grow
`specialopen [llx lly urx ury] TYPE [args]` SHALL open a window of client TYPE (unique prefix; hidden clients refused):
- With four integer coordinates, the frame SHALL be those screen coordinates, with width and height raised to the minimum window size.
- Otherwise the frame SHALL be a 300×300 frame centered on the point.
- Remaining args go to the client: for `layout` a cell name, for `color` an octal color index.

Bad usage SHALL print `Usage: specialopen [leftx bottomy rightx topy] type [args]` and the valid types. The `netlist` client SHALL allow only one window (`Sorry, can't have more than one netlist menu at a time.`).

Other window commands:
- `over` SHALL move the pointed window to the front of the stacking order.
- `underneath` SHALL move it to the back.
- `grow` SHALL toggle the pointed window between full-screen and its previous frame and depth.
- Each SHALL report `Point to a window first` (or `Point to a window first.`) when no window is pointed.

Compatibility: behavioral
(Magic ref: windows/windCmdSZ.c:404-480, windows/windCmdNR.c:114-150, windows/windCmdSZ.c:594-633, windows/windMove.c:438-559, windows/windMove.c:752-860)

#### Scenario: Color window by spec
- **WHEN** `specialopen color 12` is executed
- **THEN** a color window editing color index 10 (octal 12) SHALL open

### Requirement: windowpositions and windownames
`windowpositions [frame] [FILE]` SHALL report each window, in stacking order, as the line `specialopen %d %d %d %d %s` with llx lly urx ury and the client name. The rectangle is the screen area, or the frame area if `frame` is given. With FILE, the lines SHALL be written to that file. Magic writes them to the wrong stream (see Q-UI-17). In Tcl, each window's values replace the previous result, so only the last window is returned (see Q-UI-17). Hefesto SHALL write every line to FILE, and without FILE SHALL return one list per window.

`windownames [all|TYPE]` SHALL return, as the command result (Magic's Tcl build with no window-name hook):
- with no argument: the id of the single layout window if exactly one exists; otherwise the list of all windows' ids
- with `all`: the list of every window's id
- with `TYPE`: the list of the ids of every window of that client

Lists run from the top (front) window down. Magic's non-Tcl build instead prints `Window ID = N` (`, Name = S`) for a single window, and a `Window ID` (or `Window ID	Window Name`) header followed by one line per window.

Compatibility: behavioral
(Magic ref: windows/windCmdSZ.c:1132-1238, windows/windCmdSZ.c:481-593)

#### Scenario: Positions to terminal (verify against Magic)
- **GIVEN** one layout window with screen area (2,9)-(509,463)
- **WHEN** `windowpositions` is executed in Magic's non-Tcl build (informative)
- **THEN** Magic prints `specialopen 2 9 509 463 layout`

### Requirement: view, xview and zoom
`view` SHALL frame the root cell's bounding box B, expanded on each side by ⌊(B.w+1)/10⌋ in x and ⌊(B.h+1)/10⌋ in y, computed side by side in the order xbot, xtop, ybot, ytop. Each later expansion uses the already-expanded size.

Other forms:
- `view get` SHALL return the window's surface area as the list `llx lly urx ury`, using the current print units (Magic's Tcl build; the non-Tcl build prints `(llx, lly) to (urx, ury)`).
- `view bbox` SHALL return the root bounding box the same way.
- `view llx lly urx ury` (or a single argument holding four space-separated values) SHALL frame that area. Coordinates are parsed with the unit rules Magic parses all four values of the single-argument form as x coordinates; Hefesto SHALL parse them as x, y, x, y (see Q-UI-18).
- Any other form SHALL print `Usage: view [get|bbox|llx lly urx ury]`.
- A non-scrollable window SHALL report `Sorry, can't zoom out this window.`.

`xview` SHALL unexpand every subcell in this window, then act as `view`.

`zoom F` SHALL rescale the surface area about its center:
```
c = trunc((xbot+xtop)/2); new xbot = c - (c-xbot)*F; new xtop = c + (xtop-c)*F
```
Results are truncated toward zero (y analogous). The window SHALL then be moved to the new area. F > 1 zooms out and F < 1 zooms in.
- F SHALL satisfy 0 < F < 20. Otherwise report `zoom factor must be between 0 and 20.`.
- A wrong argument count SHALL report `Usage: zoom factor`.

Compatibility: behavioral
(Magic ref: windows/windCmdSZ.c:798-1000, windows/windCmdSZ.c:1239-1267, windows/windView.c:235-276, windows/windView.c:358-401)

#### Scenario: Zoom out by two
- **GIVEN** a surface area (-11,-24)-(112,75)
- **WHEN** `zoom 2` is executed
- **THEN** the requested area SHALL be (-72,-73)-(174,125) before the view mapping

#### Scenario: View framing
- **GIVEN** a root cell with bounding box (0,0)-(100,50)
- **WHEN** `view` is executed
- **THEN** the requested area SHALL be (-10,-5)-(111,55) before the view mapping

#### Scenario: Zoom bounds
- **WHEN** `zoom 25` is executed
- **THEN** `zoom factor must be between 0 and 20.` SHALL be reported

### Requirement: scroll
`scroll DIR [AMOUNT [w|l]]` SHALL pan the pointed scrollable window. DIR is a direction name: `n|north|top|u|up`, `s|south|bottom|d|down`, `e|east|right|r`, `w|west|left|l`, or a diagonal. `center` SHALL do nothing.

Amount forms:
- no AMOUNT: half the screen area
- AMOUNT with `w`: that fraction of the screen width or height
- AMOUNT with `l`: that fraction of the root cell's bounding box
- AMOUNT alone: a layout distance, parsed with the unit rules

The view content SHALL move so that the layout appears to move opposite to DIR. For example, `scroll r` reveals material further right.

Errors:
- a units letter other than `w` or `l` SHALL report `'units' must be one of 'w' (window) or 'l' (layout);`
- a non-numeric fractional amount SHALL report `'amount' is a fractional value.`

Compatibility: behavioral
(Magic ref: windows/windCmdSZ.c:75-213, windows/windView.c:402-551)

#### Scenario: Scroll right by a tenth
- **GIVEN** a screen area 500 pixels wide
- **WHEN** `scroll r .1 w` is executed
- **THEN** the view SHALL pan right by 50 pixels' worth of layout

### Requirement: center and findbox
`center` SHALL keep the surface size and recenter the view:
- with no arguments, on the point under the cursor
- with `center X Y` (integers only; otherwise `Coordinates must be integer values`), on (X,Y)
- with `center horizontal|vertical F` (first letter `h` or `v`), at fraction F along the root bounding box in that axis only, keeping the other axis. A non-numeric F SHALL report `Must specify a fractional value.`

`findbox` SHALL center the view on the box center. `findbox zoom` SHALL frame the box plus a margin of max(2, ⌊w/20⌋) in x and max(2, ⌊h/20⌋) in y. Errors:
- no box: `Put the box in a window first.`
- box in another root: `The box is not in the same coordinate system as the window.`

Compatibility: behavioral
(Magic ref: windows/windCmdAM.c:221-320, commands/CmdFI.c:668-761)

#### Scenario: findbox zoom
- **GIVEN** a box at (0,0)-(10,10) in the window's root
- **WHEN** `findbox zoom` is executed
- **THEN** the requested area SHALL be (-2,-2)-(12,12)

### Requirement: redraw, reset and updatedisplay
- `redraw` SHALL mark every window's whole area for redisplay.
- `reset` SHALL re-initialize the display and reload the color map, display styles and cursor glyphs. On a host-managed window system it SHALL report that it does not apply.
- `updatedisplay` SHALL immediately perform all pending redisplay.
- `updatedisplay suspend` SHALL stop all layout redisplay until `updatedisplay resume`.

Compatibility: behavioral
(Magic ref: windows/windCmdNR.c:409-472, windows/windCmdSZ.c:716-761)

#### Scenario: Suspended display
- **GIVEN** `updatedisplay suspend` was executed
- **WHEN** `paint m1` changes the layout
- **THEN** the window SHALL NOT be redrawn until `updatedisplay resume` and the next update

### Requirement: Per-window expansion state
Each layout window SHALL own one expansion bit. Each subcell instance SHALL record the set of windows in which it is expanded. The root instance of a window SHALL be expanded in that window. Expansion is independent per window: expanding an instance in window A SHALL NOT expand it in window B, even if both show the same root.

`expand [selection|surround|overlap|all] [toggle]`:
- The default mode is `overlap`.
- A last argument that is any prefix of `toggle` SHALL mean toggle (see Q-UI-19).
- It SHALL expand (or toggle) every instance, at any depth, whose bounding box the box overlaps or surrounds.
- Unavailable cells SHALL be read from disk. A cell that cannot be read reports `Cell C is unavailable.  It could not be expanded.`.

`unexpand [selection|surround|overlap|all]` is the inverse. Its default mode is `surround`.

Both commands SHALL require the box to be in the pointed window (`The box isn't in the same window as the cursor.`). Expanded instances SHALL show their contents. Unexpanded instances SHALL show only their outline and names (see Requirement: Subcell display).

Compatibility: behavioral
(Magic ref: dbwind/DBWprocs.c:84-100, database/DBexpand.c:141-275, commands/CmdE.c:819-1063, commands/CmdTZ.c:734-872, database/DBcellsubr.c:63-72)

#### Scenario: Independent windows
- **GIVEN** windows 0 and 1 both show `top`, which contains instance `u1` of `inv`
- **WHEN** `expand` with the box over `u1` is executed in window 0
- **THEN** window 0 SHALL draw the paint of `inv`
- **AND** window 1 SHALL still draw only the outline of `u1`

### Requirement: Layer visibility (see)
Each layout window SHALL have a set of visible tile types and flags for labels, cell outlines and "all same". All types, labels and cells are visible initially, and "all same" is off.

`see [no] ARG`:
- ARG SHALL be a layer list parsed like paint layer lists, which may include the pseudo-layers `$`/`*`, `labels` and `subcells`, or the word `allSame`.
- `see ARG` SHALL make those types visible. `see no ARG` SHALL hide them. With no ARG, all types are affected.
- Space SHALL never be hidden.
- Each user type SHALL be expanded to its layer mask. A stacked-contact type SHALL follow each of its residues whose home plane equals its own.
- `labels` SHALL control label display. `subcells` SHALL control cell outlines and names.
- `allSame` SHALL turn on "all same", which draws non-edit material in normal style. `see no allSame` turns it off.
- More than one argument after the optional `no` SHALL report `Usage: see [no] layers|allSame`.
- A non-layout window SHALL report `Point to a layout window first.`.

Compatibility: behavioral
(Magic ref: commands/CmdRS.c:421-577, dbwind/DBWprocs.c:106-118)

#### Scenario: Hide metal1
- **WHEN** `see no metal1` is executed
- **THEN** metal1 paint SHALL NOT be drawn in that window
- **AND** labels attached to metal1 SHALL NOT be drawn
- **AND** other windows SHALL be unaffected

### Requirement: Redisplay order for layout windows
To redisplay an area A of a layout window, the system SHALL:
1. Erase A with the background style.
2. For each technology style k in table order (52, 53, …), draw every visible tile whose type is bound to style k, across the root and all instances expanded in this window. Draw it in style k if it belongs to the edit cell under the edit transform, or if "all same" is on. Otherwise draw it in pale style k+N.
3. Draw labels and ports, if labels are visible (see Requirement: Label display).
4. Draw outlines and names of unexpanded instances, if cells are visible (see Requirement: Subcell display).
5. Draw the grid and the edit-cell origin, if the grid is on (see Requirement: Grid display).
6. Draw any tile-structure debug overlay (see ui-wizard-and-debug).
7. Redraw highlights over the area (see ui-feedback-and-highlights).

Additional rules:
- In a window whose root is not the edit cell's root, all paint and labels SHALL be pale unless "all same" is on.
- For each contact type that is not visible, its visible residues SHALL be drawn in its place.
- A stacked-contact type SHALL be drawn only on the plane where it lives, through the residue that shares that plane.
- The redisplay area SHALL be grown by one layout unit on every side when searching for tiles. Labels SHALL be searched in an area grown by the largest label extent seen so far in this window.

Compatibility: behavioral
(Magic ref: dbwind/DBWdisplay.c:156-574, dbwind/DBWdisplay.c:575-685)

#### Scenario: Pale instance paint
- **GIVEN** the edit cell is `top`, which contains an expanded instance of `inv` with polysilicon
- **WHEN** the window is redrawn
- **THEN** the polysilicon of `inv` SHALL be drawn in the pale style paired with the polysilicon style
- **AND** polysilicon painted directly in `top` SHALL use the normal style

#### Scenario: Style order is draw order
- **GIVEN** nwell bound to an earlier layout style than metal1
- **WHEN** both overlap
- **THEN** metal1 SHALL be drawn after nwell

### Requirement: DRC error display
DRC error tiles SHALL be drawn with the styles that the technology binds to the DRC error types. Only error tiles stored in the window's root cell SHALL be displayed. Error tiles inside subcells SHALL NOT be drawn.

Compatibility: behavioral
(Magic ref: dbwind/DBWdisplay.c:575-685)

#### Scenario: Subcell errors hidden
- **GIVEN** a subcell `inv` with a DRC error tile, shown expanded in a window rooted at `top`
- **WHEN** the window is redrawn
- **THEN** the error tile of `inv` SHALL NOT appear

### Requirement: Label display
A label SHALL be drawn only if its attached type is visible in the window. A port label (one with any port direction flag) SHALL use the port style (13) or the pale port style (28). Other labels SHALL use the label style (12) or the pale label style (27). The pale rule is the same as for paint.

Rendered-font labels SHALL be drawn in the label's font:
- The text height in pixels SHALL be ⌊s·size/(65536·8)⌋, where size is in 1/8 layout units. The label SHALL be skipped if this is 0.
- The text SHALL always read upright. A transformed angle in [90,270) SHALL be turned by 180°.
- The text SHALL start at the transformed corner that is bottommost (for angles in [0,90) ∪ [180,270)) or leftmost (otherwise), with a 2-pixel tolerance near multiples of 90°.

Bitmap (unfonted) labels:
- The label rectangle SHALL be drawn as an outline. A zero-area rectangle SHALL be drawn as a small cross.
- The text SHALL be placed at the rectangle point given by the transformed justification.
- The text size SHALL be the largest of the four bitmap sizes whose `B` glyph height is below the window's pixels-per-unit. If that is below the smallest size, the smallest size SHALL be used. If even 3 × pixels-per-unit does not exceed the smallest size's height, no text SHALL be drawn, only the rectangle (see Q-UI-20).

For each port, a line SHALL be drawn in the port-connection style (14) along each side of the label rectangle flagged in its port directions (n, s, e, w).

Compatibility: behavioral
(Magic ref: dbwind/DBWdisplay.c:156-300, dbwind/DBWdisplay.c:686-1029)

#### Scenario: Port direction line
- **GIVEN** a port label on rectangle (0,0)-(10,4) with direction `n`
- **WHEN** it is drawn
- **THEN** a port-connection-style line SHALL be drawn along y=4 from x=0 to x=10

#### Scenario: Hidden attachment layer
- **GIVEN** a label attached to metal2, with metal2 hidden by `see no metal2`
- **THEN** the label SHALL NOT be drawn

### Requirement: Subcell display
For every instance that is NOT expanded in the window (the root is always expanded), the system SHALL draw:
- the instance's transformed bounding box, as an outline in the bounding-box style (15)
- if the on-screen box is at least as wide and as tall as the text `BBB` in the small font: the cell name centered horizontally at 2/3 of the box height, and the instance id centered at 1/3 of the box height, both in the large font and clipped to the box

Expanded instances SHALL draw their contents and their own unexpanded descendants this way. They SHALL NOT draw their own outline.

Compatibility: behavioral
(Magic ref: dbwind/DBWdisplay.c:156-574, dbwind/DBWdisplay.c:1030-1079, database/DBcellsrch.c:825-926)

#### Scenario: Unexpanded instance
- **GIVEN** instance `u1` of cell `inv` is unexpanded and its box spans 200×100 pixels
- **THEN** an outline SHALL be drawn, with `inv` at about 2/3 height and `u1` at about 1/3 height

### Requirement: Grid display and grid command
Each layout window SHALL have a grid with spacing (sx, sy) and origin (ox, oy). The default is spacing 1×1 at the origin, and the grid starts off. When on, grid lines SHALL be drawn in the grid style (16) at x = ox + k·sx and y = oy + k·sy for all integers k. Nothing SHALL be drawn when the line spacing is below 4 pixels in either axis.

When the grid is on and the window shows the edit cell, an origin square SHALL be drawn in style 18 at the edit cell's origin, 2 pixels beyond the origin point on each side. It is drawn only if 2 layout units span at least 4 pixels.

`grid` command forms (summary; argument parsing, units and script results are owned by capability `cmd-coordinates-and-units`, requirement "Grid command"; this requirement owns the grid display):
- `grid` SHALL toggle the grid. `grid toggle` is the same.
- `grid on`, `grid off` and `grid 0` SHALL turn it on or off.
- `grid state` SHALL return `1` or `0` (Magic's Tcl build; the non-Tcl build prints `Grid is on|off`).
- `grid box` or `grid what` SHALL return `ox oy ox+sx oy+sy` in internal units (the non-Tcl build prints `Grid unit box is (ox, oy) to (ox+sx, oy+sy)`).
- `grid sx [sy [ox oy]]` and `grid box sx [sy [ox oy]]` SHALL set the grid and turn it on. Spacings are distances, origins are positions. sy defaults to sx and the origin to (0,0). A spacing ≤ 0 SHALL report `Grid spacing must be greater than zero.`. 3 or more than 4 values SHALL print usage.
- `grid multiple [m|off]` SHALL store a value from 1–255. In Magic the value has no visible effect, and out-of-range values are stored with an error. Hefesto SHALL draw every m-th grid line when the lines would otherwise be too dense to draw, and SHALL reject out-of-range values without storing them (see Q-UI-21).
- `grid help` SHALL print usage.

Compatibility: behavioral
(Magic ref: commands/CmdFI.c:1304-1471, dbwind/DBWdisplay.c:156-574, graphics/graphicsInt.h:84-89)

#### Scenario: Grid with origin
- **WHEN** `grid 10 20 5 0` is executed
- **THEN** vertical lines SHALL be at x = …, -5, 5, 15, … and horizontal lines at y = …, -20, 0, 20, …
- **AND** `grid box` SHALL return `5 0 15 20`

#### Scenario: Toggle off
- **GIVEN** the grid is on
- **WHEN** `grid` is executed with no arguments
- **THEN** the grid SHALL be turned off

### Requirement: Box display
The box SHALL be drawn only in windows whose root is the box's root cell.
- If the box's screen rectangle is a single point, the system SHALL draw it as an outline-highlight cross plus a 1-pixel-larger outlined square.
- If either nonzero dimension is under 4 pixels, the system SHALL draw a plain outline in the outline-highlight style.
- Otherwise the system SHALL draw thick sides, each 2 pixels wide, inside the rectangle. The style SHALL be solid highlights (1), or medium highlights (2) when the box does not overlap the edit cell's bounding box.

A side that falls outside the window SHALL NOT be drawn.

On a high-density display the pixel counts above (the cross size, the 4-pixel threshold, the side width) SHALL be logical pixels: they scale with the display's scale factor, so the box looks as it does in Magic on a standard-density display.

Compatibility: behavioral
(Magic ref: dbwind/DBWtools.c:672-779)

#### Scenario: Box on a high-density display
- **GIVEN** a window on a display with scale factor 2
- **WHEN** a box larger than 4 × 4 logical pixels is drawn
- **THEN** each side SHALL be 2 logical pixels (4 device pixels) wide

#### Scenario: Box outside edit cell
- **GIVEN** the edit cell's bounding box is (0,0)-(100,100)
- **WHEN** the box is placed at (200,200)-(300,300)
- **THEN** it SHALL be drawn in the medium-highlights style

### Requirement: Crosshair
`crosshair X Y` SHALL place a crosshair at the root-cell point (X,Y), in the root of the pointed window. Coordinates are parsed with the unit rules. `crosshair off` SHALL remove it. Wrong usage SHALL report `Usage: crosshair off|x y`.

The crosshair SHALL be drawn in every window whose root is the same cell:
- a vertical line across the full window height, if X is strictly inside the window horizontally
- a horizontal line across the full width, if Y is strictly inside vertically
- both in the yellow-1 style (44)

Moving the crosshair SHALL redraw only the old and new line areas.

Compatibility: behavioral
(Magic ref: commands/CmdCD.c:3801-3854, dbwind/DBWtools.c:426-622)

#### Scenario: Shared root
- **GIVEN** windows 0 and 1 both show `top`, and window 2 shows `inv`
- **WHEN** `crosshair 100 100` is executed in window 0
- **THEN** windows 0 and 1 SHALL show the crosshair and window 2 SHALL NOT

### Requirement: Change-driven redisplay
When an area of a cell changes, the system SHALL find every window showing that cell, either as root or through instances expanded in that window. It SHALL map the area up through the hierarchy and mark the corresponding screen area for redisplay.

Rules:
- For arrays: if the changed area is more than half the cell's width or height, the system SHALL mark the area spanning all elements. Otherwise it SHALL mark each element's area.
- A window SHALL skip the change if none of the changed layers is visible there.
- Changes that may affect labels SHALL grow the screen area by the largest label extent seen in that window.
- A window that shows a tile-structure debug overlay SHALL redraw completely.
- Redisplay SHALL happen at the next update, after the current command.

Compatibility: behavioral
(Magic ref: dbwind/DBWdisplay.c:1295-1650, windows/windDisp.c:267-385)

#### Scenario: Hidden layer change
- **GIVEN** window 0 has metal1 hidden
- **WHEN** metal1 is painted in the root cell
- **THEN** window 0 SHALL NOT be redrawn for that change
