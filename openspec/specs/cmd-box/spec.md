# Box Command and Box Tool

## Purpose

Defines the **box**: the single rectangular editing cursor that marks the area for paint, erase, fill, selection, cell placement and most other editing commands. This capability covers the `box` command (every option), the pointer ("box tool") gestures that move and resize the box, the undo behavior of box changes, and how other commands obtain the box. Coordinate parsing, units and snapping are defined in capability `cmd-coordinates-and-units`. The command surface is `behavioral`. The resulting box coordinates are `exact`.

## Requirements

### Requirement: Box state

The editor SHALL maintain at most one box for the whole session. The box consists of a root cell definition and a rectangle `(xbot, ybot)–(xtop, ytop)` in that cell's coordinates, with `xbot <= xtop` and `ybot <= ytop`. A zero-width or zero-height box is legal. The box SHALL be shown in every window whose top cell is the box's root cell, and not in other windows. The box MAY be absent, for example at startup with no cell loaded, after `box remove`, or after its root cell is removed from memory.

Every change of the box SHALL be recorded in the undo history as an event that holds the old and new (root cell, rectangle). Undo and redo SHALL restore the box. The box itself is not part of any cell and is never saved.

Compatibility: behavioral

(Magic ref: dbwind/DBWtools.c:803-850, dbwind/DBWundo.c:261-301)

#### Scenario: Undo restores the box
- **GIVEN** the box at (0,0)–(10,10)
- **WHEN** `box values 5 5 20 20` is run and then `undo` is run
- **THEN** the box SHALL again be (0,0)–(10,10)

#### Scenario: Box removed with its cell
- **GIVEN** the box is in a window whose top cell is `A`
- **WHEN** cell `A` is removed from memory
- **THEN** the box SHALL become absent

### Requirement: Box command syntax

`box [option [args…]] [-edit]` SHALL accept these options, matched with the keyword-matching rule of capability `cmd-coordinates-and-units`:

`width`, `height`, `size`, `position`, `values`, `remove`, `select`, `move`, `grow`, `shrink`, `corner`, `exists`, `help`.

Parsing rules:

1. If the last argument begins with `-edit`, it SHALL be removed, and coordinates SHALL be read and reported in edit-cell coordinates (see "Edit-coordinate switch").
2. A first argument that is ambiguous and starts with `h` (for example `h` or `he`) SHALL select `height`.
3. A first argument that is not a recognized option SHALL select the **default form**: report the box (see "Default box report"). If exactly four arguments are given, it SHALL set the box from them (see "Setting the box directly").
4. More than six arguments (seven words including `box`) SHALL print the option help.
5. A recognized option with the wrong number of arguments SHALL print the option help and change nothing.

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:680-760, commands/CmdAB.c:1280-1286)

#### Scenario: h means height
- **GIVEN** a box (0,0)–(4,6) with units `default`
- **WHEN** `box h` is run in scripting mode
- **THEN** the result SHALL be `6`

#### Scenario: Too many arguments
- **WHEN** `box values 1 2 3 4 5 6` is run
- **THEN** the option help SHALL be printed
- **AND** the box SHALL not change

### Requirement: Box queries

With no value arguments, the query options SHALL report the box without changing it. Values SHALL be formatted with the reported-value formatting of capability `cmd-coordinates-and-units`:

| Option | Scripting result | Interactive output |
|---|---|---|
| `width` | width | `Root box width is W` (or `Edit box …`) |
| `height` | height | `Root box height is H` |
| `size` | list `W H` | `Root box size is W x H` |
| `position` | list `llx lly` | `Root box lower-left corner at (llx, lly)` |
| `values` | list `llx lly urx ury` | `Root box coordinates (llx, lly) to (urx, ury)` |
| `exists` | boolean | `True` / `False` |

x values (width, llx, urx) SHALL be formatted as x quantities, and y values as y quantities (this matters for user-grid units). `box exists` SHALL work when no box is present. Every other query SHALL fail with "Box tool must be present" when there is no box.

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:795-1020, commands/CmdAB.c:787-794)

#### Scenario: Values in scripting mode
- **GIVEN** a box (−2,0)–(8,5) and units `default`
- **WHEN** `box values` is run in scripting mode
- **THEN** the result SHALL be the list `-2 0 8 5`

#### Scenario: Values in microns
- **GIVEN** `U = 10` nm, `units microns`, and a box (0,0)–(150,50)
- **WHEN** `box size` is run in scripting mode
- **THEN** the result SHALL be `1.5 0.5`

#### Scenario: Exists with no box
- **GIVEN** no box
- **WHEN** `box exists` is run in scripting mode
- **THEN** the result SHALL be `0` (false)

### Requirement: Box size and position setters

When given values, the setter options SHALL change the box as follows, starting from the current box `(x0,y0)–(x1,y1)`:

- `box width W`: `x1 = x0 + W`. W is a relative x distance.
- `box height H`: `y1 = y0 + H`. H is a relative y distance.
- `box size W H`: `x1 = x0 + W`, `y1 = y0 + H`.
- `box position X Y`: moves the box so its lower-left is `(X, Y)`, keeping width and height. X and Y are absolute coordinates.
- `box values X0 Y0 X1 Y1`: sets the box to these absolute coordinates. This form does not require an existing box. With no box, the root cell is the top cell of the current window.
- `box select`: sets the box to the bounding box of the current selection, in root coordinates (with an empty selection, the empty bounding box).

A negative width or height produces an inverted rectangle. It SHALL be normalized by swapping the coordinates (see "Box normalization").

Compatibility: exact

(Magic ref: commands/CmdAB.c:874-1020, commands/CmdAB.c:1291-1316)

#### Scenario: Set width
- **GIVEN** a box (10,10)–(20,30) and units `internal`
- **WHEN** `box width 5` is run
- **THEN** the box SHALL be (10,10)–(15,30)

#### Scenario: Negative height normalizes
- **GIVEN** a box (10,10)–(20,30) and units `internal`
- **WHEN** `box height -4` is run
- **THEN** the box SHALL be (10,6)–(20,10)

#### Scenario: Position keeps size
- **GIVEN** a box (10,10)–(20,30) and units `internal`
- **WHEN** `box position 0 -5` is run
- **THEN** the box SHALL be (0,−5)–(10,15)

### Requirement: Setting the box directly

`box X0 Y0 X1 Y1` (four non-keyword arguments) SHALL set the box to the given absolute coordinates, like `box values`. It SHALL also print the default box report for the new box (see "Default box report"). It does not require an existing box.

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:782-784, commands/CmdAB.c:1222-1280)

#### Scenario: Four-number form
- **GIVEN** units `internal`
- **WHEN** `box 0 0 10 20` is run
- **THEN** the box SHALL be (0,0)–(10,20)
- **AND** the box report SHALL be printed

### Requirement: Box move, grow and shrink

`box move|grow|shrink DIR DIST` SHALL change the box by the distance `DIST` in direction `DIR` (any direction or position name from capability `cmd-coordinates-and-units`). DIST is a relative distance. When units are `user`, DIST is parsed as an x distance for east/west and as a y distance for north/south. For diagonal directions it is parsed separately for x (`dx`) and for y (`dy`). Otherwise `dx = dy = DIST`.

If `dx = 0` and `dy = 0`, the command SHALL do nothing. Otherwise:

| DIR | move | grow | shrink |
|---|---|---|---|
| north | y0+=dy, y1+=dy | y1+=dy | y1-=dy |
| south | y0-=dy, y1-=dy | y0-=dy | y0+=dy |
| east | x0+=dx, x1+=dx | x1+=dx | x1-=dx |
| west | x0-=dx, x1-=dx | x0-=dx | x0+=dx |
| ne | north and east | north and east | north and east |
| nw | north and west | north and west | north and west |
| se | south and east | south and east | south and east |
| sw | south and west | south and west | south and west |
| center | no change | all four sides outward | all four sides inward |

Negative distances SHALL be allowed and act in the opposite sense. A shrink beyond zero size produces an inverted rectangle, which SHALL be normalized.

Compatibility: exact

(Magic ref: commands/CmdAB.c:827-873, commands/CmdAB.c:1022-1182)

#### Scenario: Grow center
- **GIVEN** a box (10,10)–(20,20) and units `internal`
- **WHEN** `box grow center 2` is run
- **THEN** the box SHALL be (8,8)–(22,22)

#### Scenario: Move by lambda
- **GIVEN** `Λ = 2`, units `default`, snap `lambda`, and a box (0,0)–(4,4)
- **WHEN** `box move e 3` is run
- **THEN** the box SHALL be (6,0)–(10,4)

#### Scenario: Shrink past zero
- **GIVEN** a box (0,0)–(4,4) and units `internal`
- **WHEN** `box shrink n 6` is run
- **THEN** the box SHALL be (0,−2)–(4,0)

#### Scenario: Zero distance is a no-op
- **WHEN** `box move n 0` is run
- **THEN** the box SHALL not change and nothing SHALL be printed

### Requirement: Box cursor forms

When the last argument of `box move DIR`, `box corner DIR`, `box grow DIR` or `box shrink DIR` begins with `cursor`, the pointer position SHALL be used, snapped according to capability `cmd-coordinates-and-units`. DIR names the reference corner: `sw`/`bl`, `ne`/`tr`, `nw`/`tl` or `se`/`br`.

- `box move DIR cursor` SHALL translate the box so the named corner lands on the pointer.
- `box corner DIR cursor` SHALL move only the named corner to the pointer (see "Box corner dragging").
- `box grow DIR cursor` and `box shrink DIR cursor` SHALL not change the box (see Q-CMDEDIT-11).

If no box exists, a zero-size box at (0,0) in the pointer's window SHALL be used as the starting box. A Manhattan or `center` DIR leaves the reference corner undefined in Magic. Hefesto SHALL treat it as `sw` (see Q-CMDEDIT-10).

After a cursor form, the command SHALL be recorded in the command log as `box values Xi Yi Xi Yi`, with the resulting root coordinates in internal units.

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:765-790, commands/CmdAB.c:800-828, dbwind/DBWtools.c:868-940)

#### Scenario: Move upper-right corner to cursor
- **GIVEN** a box (0,0)–(10,10), snap `internal`, and the pointer at root (30,40)
- **WHEN** `box move ne cursor` is run
- **THEN** the box SHALL be (20,30)–(30,40)
- **AND** the logged command SHALL be `box values 20i 30i 30i 40i`

### Requirement: Box corner command

`box corner DIR X Y` SHALL be accepted with exactly two further arguments after DIR. The distance argument is parsed first as a relative distance. If it is zero, the command SHALL do nothing. Otherwise Magic parses the **direction word itself** as the x coordinate (which fails and yields 0) and the distance as the y coordinate, then moves the named corner to that point (see Q-CMDEDIT-9). Hefesto SHALL implement the documented intent: `box corner DIR X Y` moves the corner named by DIR to the absolute point (X, Y). Hefesto SHALL treat this as a deliberate deviation recorded in Q-CMDEDIT-9.

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:1124-1145, doc/html/box.html)

#### Scenario: Corner to point (Hefesto intent)
- **GIVEN** a box (0,0)–(10,10) and units `internal`
- **WHEN** `box corner ne 20 30` is run in Hefesto
- **THEN** the box SHALL be (0,0)–(20,30)

### Requirement: Default box report

`box` with no option (or with an unrecognized first argument) SHALL print a report of the box. The report is not returned as a scripting result. It SHALL contain the width, height, lower-left and upper-right corners, and, when `width·height > 0`, the area. These are shown:

- in microns (value·U/1000, 3 decimals),
- in lambda (value·L0/L1, 2 decimals), only when `L0 ≠ L1`,
- in internal units (integers).

When `L0 = L1`, the internal-unit line SHALL be labeled `lambda:`, because internal units and lambda are the same. Magic's exact layout is:

```
Root cell box:
           width x height  (   llx,  lly  ), (   urx,  ury  )  area (units^2)

microns:  %6.3f x %-6.3f  (% 6.3f, % -6.3f), (% 6.3f, % -6.3f)  %-10.3f
lambda:   %6.2f x %-6.2f  (% 6.2f, % -6.2f), (% 6.2f, % -6.2f)  %-10.2f
internal: %6d x %-6d  (% 6d, % -6d), (% 6d, % -6d)  %-10d
```

The `-edit` switch changes `Root` to `Edit`. The area column and its header appear only when the area is positive. The area SHALL be computed in 64-bit integers. The report layout is `informative`. The values are `exact`.

An unrecognized first argument with other than four arguments SHALL print the report and leave the box unchanged. This is how Magic handles the documented `box DIR DIST` form, which it does not implement (see Q-CMDEDIT-8).

Compatibility: informative

(Magic ref: commands/CmdAB.c:1184-1226, doc/html/box.html)

#### Scenario: Report values
- **GIVEN** `U = 10` nm, `L0 = L1 = 1`, and a box (0,0)–(100,50)
- **WHEN** `box` is run
- **THEN** the microns line SHALL show width 1.000, height 0.500 and area 0.500
- **AND** the second line SHALL be labeled `lambda:` and show the integers 100, 50 and area 5000

#### Scenario: Undocumented direction form (verify against Magic)
- **GIVEN** a box (0,0)–(10,10)
- **WHEN** `box north 5` is run
- **THEN** the box report SHALL be printed
- **AND** the box SHALL remain (0,0)–(10,10)

### Requirement: Edit-coordinate switch

When `-edit` is given, the box SHALL first be transformed from root to edit-cell coordinates. The option SHALL then be applied and reported in edit coordinates, and the result SHALL be transformed back to root coordinates. Because the edit cell may be rotated or mirrored, "width" under `-edit` is measured along the edit cell's x axis. With `-edit`, the box SHALL be required to lie in a window on the edit cell, as for editing commands.

For `box values X0 Y0 X1 Y1 -edit` and the four-number form, the given rectangle SHALL be read as edit coordinates and transformed to root coordinates.

Compatibility: exact

(Magic ref: commands/CmdAB.c:740-748, commands/CmdAB.c:776-792, commands/CmdAB.c:1252-1257)

#### Scenario: Edit cell rotated 90 degrees
- **GIVEN** the edit cell is placed in the root with a 90-degree rotation and no translation, and the root box is (0,0)–(10,20)
- **WHEN** `box width -edit` is run in scripting mode with units `default`
- **THEN** the result SHALL be `20`

### Requirement: Box normalization and bounds

After any setter, the new box SHALL be applied in two steps: first translate the box so its lower-left is the new lower-left, then move the upper-right corner to the new upper-right. If the result is inverted on an axis, the two coordinates on that axis SHALL be swapped. Each step SHALL be a separate undoable box event within the same command.

If either coordinate of the new lower-left corner is outside the range [−(2^30 − 6), 2^30 − 6] (the tile-plane infinity 2^30 − 4, less a margin of 2), the command SHALL report "Box out of bounds." and leave the box unchanged. Magic checks only the lower-left corner (see Q-CMDEDIT-12).

Compatibility: exact

(Magic ref: commands/CmdAB.c:1228-1279, dbwind/DBWtools.c:940-1066)

#### Scenario: Out of bounds
- **WHEN** `box position 2000000000i 0` is run
- **THEN** "Box out of bounds." SHALL be reported and the box SHALL not change

### Requirement: Box remove and help

`box remove` SHALL make the box absent and record the change for undo. `box help` SHALL print one line per option with its arguments and description.

Compatibility: behavioral

(Magic ref: commands/CmdAB.c:762-766, commands/CmdAB.c:1280-1286)

#### Scenario: Remove then query
- **WHEN** `box remove` is run and then `box width`
- **THEN** the second command SHALL fail with "Box tool must be present"

### Requirement: Box corner dragging

Moving a single corner (from a pointer gesture or `box corner … cursor`) SHALL follow these rules:

- If the target point lies in a window whose top cell differs from the box's root cell, the whole box SHALL be moved instead (translated so the chosen corner is at the point) into the new root cell.
- Otherwise only the chosen corner SHALL move to the point. If that inverts the box, the coordinates SHALL be swapped on the inverted axis.
- Moving the whole box SHALL translate it so the chosen corner (default lower-left) is at the point, in the target window's root cell.
- If the pointer is not in a layout window, the command SHALL report "Can't put box there." and change nothing.

Compatibility: exact

(Magic ref: dbwind/DBWtools.c:868-1066)

#### Scenario: Corner drag past the opposite side
- **GIVEN** a box (0,0)–(10,10)
- **WHEN** the upper-right corner is dragged to (−5, 4) in the same window
- **THEN** the box SHALL be (−5,0)–(0,4)

### Requirement: Box tool pointer gestures

When the box tool is the active pointer tool, pointer buttons SHALL act as follows. Points are snapped as in capability `cmd-coordinates-and-units`.

- **Primary (left) button**: on release, move the whole box so its lower-left corner is at the release point.
- **Secondary (right) button**: on release, move the box's upper-right corner to the release point (corner dragging rules apply).
- **Both buttons**: when the second of the two buttons is pressed, the corner of the visible part of the box nearest to the pointer SHALL be chosen. The first button pressed determines the action (left = move whole box, right = move that corner). The action happens when the last button is released, at the release point. Nearest corner: the pointer is compared with the midpoint of the box clipped to the window, on each axis (lower half → bottom, otherwise top; left half → left, otherwise right).
- **Middle button press**: paint or erase at the box, using the layers under the pointer (see the `paint cursor` behavior in capability `cmd-paint-erase`).

On touch devices, Hefesto SHALL provide equivalent gestures for "move box by a corner" and "move one corner". The gestures follow the mode-based mapping of decision X-5 (one-finger drag = primary button, long press = secondary/tertiary actions, floating modifier key = middle/right-button semantics); remaining details are tracked in Q-CMDEDIT-13.

Compatibility: behavioral

(Magic ref: dbwind/DBWbuttons.c:416-490, dbwind/DBWtools.c:430-470)

#### Scenario: Left click moves the box
- **GIVEN** a box (0,0)–(10,10), snap `internal`, and the box tool active
- **WHEN** the left button is released at root (100,50)
- **THEN** the box SHALL be (100,50)–(110,60)

#### Scenario: Right click resizes
- **GIVEN** the same box
- **WHEN** the right button is released at root (30,40)
- **THEN** the box SHALL be (0,0)–(30,40)

### Requirement: Box availability to other commands

Commands that need the box SHALL obtain it in one of two ways:

- In root coordinates, together with its root cell. If no box exists, the command SHALL fail with an error.
- In edit coordinates. In addition to existing, the box SHALL lie in a window on the edit cell (see "Root and edit coordinate frames" in capability `cmd-coordinates-and-units`).

When finding "the window containing the box", any window whose top cell is the box's root cell SHALL qualify, even if the box is outside the window's visible area.

Compatibility: behavioral

(Magic ref: dbwind/DBWtools.c:206-352)

#### Scenario: Box off-screen still usable
- **GIVEN** the box lies entirely outside the visible area of the only window on the edit cell
- **WHEN** `paint metal1` is run
- **THEN** metal1 SHALL be painted in the box area
