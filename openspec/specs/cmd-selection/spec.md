# Selection and Selection-Editing Commands

## Purpose

Defines the selection model and the commands that create, inspect and operate on it: `select` (every option and qualifier, point-selection cycling, chunk/region/net growth, `short`, `save`), `what`, `identify`/`id`, `delete`, `copy`, `move`, `stretch`, `clockwise`/`rotate`, `sideways`, `upsidedown`, `orient`, and `array`. Coordinates, units and keyword matching are in capability `cmd-coordinates-and-units`. The box is in `cmd-box`. Paint semantics are in `cmd-paint-erase`. Writing the selection to a file uses the cell writer of capability `cmd-hierarchy`. The command surface is `behavioral`. Resulting geometry and transforms are `exact`.

## Requirements

### Requirement: Selection model

The selection SHALL be a flattened, root-coordinate copy of chosen layout items: paint (with types), labels, and subcell uses (keeping their array parameters and instance identifiers). It is tied to one **selection root cell**, the top cell of the window it was made in. It is displayed as a highlight in all windows on that root cell.

Selecting in a window whose top cell differs from the current selection root SHALL first clear the selection and then make that cell the new root. Each item SHALL remember where it came from (which cell of the hierarchy), so that operations can tell edit-cell items from non-edit items. Every change of the selection SHALL be recorded in the undo history, and undo/redo SHALL restore the previous selection. The selection is never saved with a cell.

Compatibility: behavioral

(Magic ref: select/selCreate.c:110-300, select/selUndo.c:175-300)

#### Scenario: Undo restores selection
- **GIVEN** an empty selection
- **WHEN** `select area` is run and then `undo` is run
- **THEN** the selection SHALL be empty again

#### Scenario: Switching windows clears selection
- **GIVEN** items selected in a window on cell `A`
- **WHEN** `select area` is run with the box in a window on cell `B`
- **THEN** the selection SHALL contain only items from `B`

### Requirement: Select qualifiers

`select` SHALL accept one leading qualifier. With `magic-compat` ON (the default), it is matched as a prefix of the qualifier word (the typed word must be a prefix), tested in this order, and the first match wins: `more`, `less`, `nocycle`, `same`, `do`, `no`, `simple`, `top` (the last only when followed by `cell`). With `magic-compat` OFF, a typed word that equals a qualifier exactly SHALL select that qualifier first; otherwise the same prefix matching in the same order applies (see Q-CMDEDIT-60, resolution: replicate in magic-compat, fix in native):

- `more`: add to the selection instead of replacing it.
- `less`: remove matching items from the selection.
- `nocycle`: do not cycle at the same point. Ignore subcell labels for this command, and reset type cycling.
- `same`: treat this command as a repeat at the same place (enables cycling).
- `do` / `no` / `simple`: label policy for net/region selection (also used by `select labels`). With `magic-compat` ON, `no` is a prefix of `nocycle`, which is tested first, so `no` behaves as `nocycle` (turning off cycling and resetting the type counter), as in Magic; with it OFF, `no` is an exact match and selects the `no` label policy. In both modes `s` is a prefix of `same`, tested before `simple`, so `s` means `same` (see Q-CMDEDIT-60).
- `top cell`: in `select top cell`, select the window's top cell itself.

A trailing `at X Y` (the third-to-last word is exactly `at`) SHALL give the reference point in edit coordinates instead of the pointer. The point is transformed to root coordinates, and the reference area is the unit square at that point.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:840-960, commands/CmdRS.c:903-957; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-60)

#### Scenario: no qualifier with magic-compat ON
- **GIVEN** `magic-compat` ON
- **WHEN** `select no net` is run
- **THEN** the qualifier SHALL be `nocycle`: cycling is turned off, the type counter is reset, and the net is selected with the default label policy

#### Scenario: no qualifier with magic-compat OFF
- **GIVEN** `magic-compat` OFF
- **WHEN** `select no net` is run
- **THEN** the qualifier SHALL be the `no` label policy, and the net SHALL be selected with that policy

#### Scenario: More qualifier
- **GIVEN** metal1 at A selected
- **WHEN** `select more area poly` is run with the box over poly at B
- **THEN** the selection SHALL contain both the metal1 at A and the poly at B

#### Scenario: At point
- **GIVEN** units `internal` and metal1 covering (0,0)–(10,10) in the edit cell
- **WHEN** `select at 5 5` is run
- **THEN** a metal1 chunk SHALL be selected as if the pointer were at (5,5)

### Requirement: Select options

After the qualifier, the option SHALL be matched with the keyword-matching rule against: `area`, `visible`, `cell`, `labels`, `intersection`, `clear`, `flat`, `help`, `keep`, `move`, `pick`, `save`, `feedback`, `bbox`, `box`, `chunk`, `region`, `net`, `short`. Behavior:

- No option, or an unrecognized word given as the only argument (taken as a layer list): **point selection** (see "Point selection and cycling").
- Any other unrecognized option: report `"WORD" isn't a valid select option.` and print help.
- A recognized option with the wrong number of arguments: report `Bad arguments:` with the usage line, and do nothing.
- `select help`: print one usage line per option.

Every option other than `box`, `chunk`, `region` and `net` SHALL reset the point-selection cycling level.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:780-1012)

#### Scenario: Invalid option
- **WHEN** `select frobnicate metal1` is run
- **THEN** `"frobnicate" isn't a valid select option.` SHALL be reported and help printed

### Requirement: Area and visible selection

`select [more|less] area [LAYERS [GLOB]]` SHALL select every item under the box (the whole box rectangle, in root coordinates), through the hierarchy of cells expanded in the box's window. LAYERS defaults to `*,labels,subcell`. A layer list equal to `space` means `*,labels`. Items selected:

- **Paint**: every tile of a type in LAYERS, clipped to the box.
- **Labels**: if `labels` is in LAYERS, or for `area` always (the `area` form adds the label pseudo-type), every label inside the box. Labels attached to a type in LAYERS are included if any non-label type was given. If LAYERS named no types or named all types, every label is included. If GLOB is given, only labels whose text matches the glob pattern are included.
- **Subcells**: if `subcell` is in LAYERS, every use whose bounding box overlaps the box, as whole uses.

Without `more`/`less`, the selection SHALL be cleared first. With `less`, matching items in the box area SHALL be removed from the selection instead. `select visible [LAYERS]` SHALL behave like `area`, but restricted to layers visible in the window, to labels only if labels are displayed, and to subcells only if subcells are displayed. It takes no GLOB.

If the box is in several windows with different expansion states, the window under the pointer SHALL be used. If the pointer is not in one of them, the command SHALL report "The box is in more than one window; use the cursor to select the one you want to select from."

Compatibility: exact

(Magic ref: commands/CmdRS.c:560-625, commands/CmdRS.c:1020-1040, select/selCreate.c:438-540, select/selUnselect.c:121-210)

#### Scenario: Area selection clips paint
- **GIVEN** metal1 at (0,0)–(20,20) in the edit cell and the box (5,5)–(10,10)
- **WHEN** `select area metal1` is run
- **THEN** the selection SHALL contain metal1 (5,5)–(10,10) only

#### Scenario: Less removes
- **GIVEN** the selection contains metal1 (0,0)–(20,20)
- **WHEN** `select less area metal1` is run with the box (0,0)–(10,20)
- **THEN** the selection SHALL contain metal1 (10,0)–(20,20)

#### Scenario: Glob labels
- **GIVEN** labels `VDD`, `VSS`, `IN` inside the box
- **WHEN** `select area labels V*` is run
- **THEN** exactly `VDD` and `VSS` SHALL be selected

### Requirement: Point selection and cycling

`select [more|less|nocycle|same] [LAYERS] [at X Y]` (no option word) SHALL select from the material at the reference point. The reference area is the pointer pixel area, or the unit square at the `at` point. The candidate types are the types present in that area (through expanded cells), restricted to visible layers, with space and DRC types removed, and intersected with LAYERS if given.

Successive invocations count as **at the same place** when the new pointer position is within 2 screen units (or 2 internal units for `at`) of the previous reference point, and the command immediately follows the previous `select`. While at the same place with an unchanged `less` flag, each repeated invocation SHALL advance the level `chunk → region → net → chunk`. When the level returns to chunk, the selection SHALL be cleared and the next candidate type (in increasing type order, wrapping around) SHALL be chosen. Otherwise the level SHALL restart at chunk with the next candidate type.

- **chunk**: select the largest rectangle of the chosen type (see "Chunk selection"), plus labels in it. Set the box to the chunk's rectangle (unless `less`).
- **region**: select all paint of exactly the chosen type that is electrically contiguous through same-type paint, through the hierarchy. If the region adds nothing beyond the chunk, advance directly to net.
- **net**: select all paint and labels connected to the point under the technology connectivity rules, through the hierarchy (see capability `extract-commands` for connectivity).

If no paint candidate exists, point selection SHALL fall back to **cell selection** at that point.

If LAYERS is given and matches nothing at the point, the command SHALL report "No paint of this type under the cursor/box". An invalid list SHALL report "Invalid layer specification".

A pointer-based point selection SHALL be logged as the same command with ` at Xi Yi` appended (root lower-left of the pointer area).

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:1395-1560)

#### Scenario: Cycling levels
- **GIVEN** a metal1 wire connected through a via to metal2
- **WHEN** `select` is run three times at the same point on the metal1
- **THEN** the selection SHALL be, in order: the metal1 chunk, the contiguous metal1 region, then the whole net including the metal2

### Requirement: Box-anchored chunk, region and net selection

`select [more|less] box|chunk|region|net [LAYERS]` SHALL use the unit square at the box's lower-left corner as the reference area. A box SHALL exist. The type is the first candidate type (in increasing order after the previously used type). `chunk`, `region` and `net` select at exactly that level, with no cycling. `box` follows the point-selection cycling rule. If LAYERS is given and the unit square contains no matching type, the unit square one unit down and to the left SHALL be tried before reporting "No paint of this type under or next to the cursor/box".

Compatibility: exact

(Magic ref: commands/CmdRS.c:1300-1340, commands/CmdRS.c:1420-1450)

#### Scenario: Net from box corner
- **GIVEN** the box lower-left at (3,3) on a metal1 wire that connects through a via to metal2
- **WHEN** `select net` is run
- **THEN** the metal1, the via and the connected metal2 SHALL be selected

### Requirement: Chunk selection

The chunk of type T at the reference area A SHALL be computed as follows:

1. If any non-Manhattan tile of T (or of a stacked contact containing T, if T is a contact) overlaps A, the chunk is that tile's bounding rectangle.
2. Otherwise, start with a search window equal to A grown by 10 units on each side.
3. Find the rectangle that contains A, lies inside the search window, contains only T (or stacking contacts of T), and maximizes first its smaller dimension and then its larger dimension. The search recursively splits around the first offending tile, trying the x split before the y split, with a recursion depth limit of 22.
4. If the best rectangle touches a side of the search window, extend the window on that side (double the window size on sides the rectangle touches, otherwise move the side to 1 unit past the rectangle) and repeat. Stop when the rectangle is strictly inside the window.
5. If no rectangle contains A, select nothing.

The selected chunk SHALL contain T in that rectangle (for a contact type, only T, not its stacked contacts) and the labels in it.

Compatibility: exact

(Magic ref: select/selCreate.c:569-735, select/selCreate.c:735-905)

#### Scenario: Chunk prefers wider rectangle
- **GIVEN** an L-shaped metal1 made of (0,0)–(100,10) and (0,0)–(10,50), and the reference point (5,5)
- **WHEN** a chunk is selected
- **THEN** the chunk SHALL be (0,0)–(100,10), because both candidates have smaller dimension 10 and this one has the larger dimension (100 > 50)

### Requirement: Cell selection

`select [more|less|top] cell [NAME]` SHALL select a subcell use:

- **With NAME**: find the use by its hierarchical instance path from the window's top cell. If it is not found, report `Couldn't find a cell use named "NAME"`. The use (with all array elements) SHALL be selected, and the box SHALL be set to its bounding box.
- **Without NAME** (or as the fallback of point selection): among the uses whose area contains the reference point, consider expanded uses and unexpanded uses all of whose ancestors are expanded. Visit them in order of increasing area. Each repeated invocation at the same place selects the next one, replacing the previous cell in the selection. After the largest, it wraps to the window's top cell, then starts over. For an array, the element under the point is identified by its indices.
- `select top cell` SHALL select the window's top cell.

The selected use SHALL be added with its root transform (for an array, the transform of the chosen element is used to place the box). The box SHALL be set to that element's bounding box. A use whose duplicate already exists in the selection SHALL not be added twice. The scripting result SHALL be the instance path relative to the top cell, with `\[` and `\]` unescaped. Interactively the command prints `Selected cell is DEF (PATH)`. `select less cell` removes the use instead.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:1560-1660, select/selCreate.c:1118-1185, database/DBcellsel.c)

#### Scenario: Cycle through nested cells
- **GIVEN** top cell `T` containing use `a` of cell `A` (area 100), and `A` containing use `b` of cell `B` (area 10), both expanded, overlapping at point P
- **WHEN** `select cell` is run three times at P
- **THEN** the results SHALL be `a/b`, then `a`, then `T`

### Requirement: Intersection selection

`select intersection [~|!]LAYER` SHALL replace the selection's paint with its intersection with LAYER under the box: the selection paint restricted to areas where LAYER (or, with a `~` or `!` prefix, any type on LAYER's plane other than LAYER) is present in the layout under the box, through expanded cells. The paint of LAYER itself SHALL not remain in the selection. An unknown layer SHALL report `Cannot parse layer type "LAYER".`

Compatibility: exact

(Magic ref: commands/CmdRS.c:627-680, select/selCreate.c:307-436)

#### Scenario: Intersect with not-nwell
- **GIVEN** the selection holds dnwell (0,0)–(20,20), and nwell exists at (0,0)–(10,20)
- **WHEN** `select intersection ~nwell` is run with the box covering everything
- **THEN** the selection SHALL hold dnwell only at (10,0)–(20,20) (verify against Magic)

### Requirement: Selection utility options

- `select clear` SHALL empty the selection. It takes no qualifier.
- `select bbox` SHALL report the selection bounding box in root coordinates, formatted per `units` (a scripting list `llx lly urx ury`, or printed `Select bounding box: …`).
- `select labels` with `do`/`no`/`simple` SHALL set the global policy for including subcell labels in net/region selection, and SHALL report which policy is active.
- `select flat` SHALL replace the selection with a fully flattened copy: all paint and labels of selected subcells, at all depths, become selection paint and labels, and the subcell uses are removed from the selection.
- `select feedback [STYLE [TEXT]]` SHALL copy the selection's paint to feedback areas with the given display style (default orange) and text (default `selection`). An unknown style SHALL report `Unknown style`.
- `select save FILE` SHALL write the selection as a cell named by FILE (`.mag` format, see capability `cmd-hierarchy`), after re-attaching labels and marking the whole cell for DRC.
- `select short NAME1 NAME2` SHALL select the net of the label matching NAME1 (as `goto NAME1; select net`), then replace the selection with the paint along a path with the fewest tile-to-tile steps connecting a tile under label NAME1 to a tile under label NAME2. Labels are matched by glob. If no path exists, it SHALL report "No shorting path found between source and destination!"

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:1040-1215, select/selOps.c:350-410, select/selOps.c:941-1050)

#### Scenario: Bounding box result
- **GIVEN** the selection covers (0,0)–(10,5) and (20,20)–(30,30), and units `default`
- **WHEN** `select bbox` is run in scripting mode
- **THEN** the result SHALL be `0 0 30 30`

### Requirement: Floating selection (pick, move, keep)

Hefesto SHALL support a floating selection: the selection MAY carry a display offset that is not yet applied to the layout:

- `select pick` SHALL delete the edit-cell parts of the selection from the layout, and keep the selection unchanged.
- `select move` SHALL set the selection's offset to (pointer − box lower-left). The pointer must be in a window on the edit cell, and the box must exist, otherwise the command reports "Error: Pointer is not in the edit cell.". `select move X Y` sets the offset to (X, Y).
- `select keep` SHALL copy the whole selection, with its offset, into the edit cell (paint of active types, labels and subcells).

These three take no qualifier.

Compatibility: behavioral

(Magic ref: commands/CmdRS.c:1086-1140, select/selCreate.c:1240-1268)

#### Scenario: Drag sequence
- **GIVEN** metal1 (0,0)–(10,10) selected in the edit cell, units `internal`
- **WHEN** `select pick`, then `select move 20 0`, then `select keep` are run
- **THEN** the edit cell SHALL contain metal1 at (20,0)–(30,10) and not at (0,0)–(10,10)

### Requirement: Edit-cell restriction for selection operations

Operations that modify the layout through the selection (`delete`, `move`, `stretch`, rotations and flips, `orient`, `array`, `select pick`) SHALL act only on the parts of the selection that belong to the edit cell:

- **Paint**: paint whose type is present in the edit cell at that place.
- **Subcells**: uses that are direct children of the edit cell and not locked.
- **Labels**: labels that exist in the edit cell with the same rectangle and text.

When non-edit parts are skipped, the command SHALL warn that only edit-cell items were affected (paint, subcells and labels each have their own warning). `copy` and `select keep` copy the **whole** selection into the edit cell, including items that came from other cells.

If there is no editable edit cell, these operations SHALL fail with "The current cell is not editable."

Compatibility: behavioral

(Magic ref: select/selOps.c:99-170, select/selEnum.c:132-330, select/selOps.c:286-345)

#### Scenario: Delete skips child paint
- **GIVEN** the selection contains metal1 from the edit cell and poly from a child cell
- **WHEN** `delete` is run
- **THEN** only the metal1 SHALL be erased
- **AND** a warning SHALL report that paint outside the edit cell was not deleted

### Requirement: Delete command

`delete` (no arguments) SHALL erase the edit-cell parts of the selection from the edit cell and then clear the selection:

- Paint is erased in the selected rectangles or triangles, per type.
- Subcell uses are removed.
- Labels are removed by matching rectangle and text, regardless of their attached type.

A box in a window on the edit cell SHALL be required. DRC is queued over the selection area.

Compatibility: exact

(Magic ref: commands/CmdCD.c:3855-3870, select/selOps.c:99-285)

#### Scenario: Delete
- **GIVEN** metal1 (0,0)–(10,10) in the edit cell, fully selected
- **WHEN** `delete` is run
- **THEN** the metal1 SHALL be gone and the selection SHALL be empty

### Requirement: Copy and move displacement

`copy` and `move` SHALL accept these forms. The displacement is computed in root coordinates:

| Form | Displacement |
|---|---|
| `DIR` | 1 lambda in DIR (diagonals move both axes) |
| `DIR AMOUNT` | AMOUNT along DIR (for diagonals, AMOUNT on both axes) |
| `DIR AX AY` | for diagonal DIR: AX in x and AY in y |
| `DX DY` (first word is not a direction) | (DX, DY) |
| `to X Y` | (X, Y), transformed from edit to root coordinates, minus the box lower-left |
| (no argument) | pointer position minus the box lower-left |

`center` gives zero displacement. For `copy`, a single non-direction word SHALL report "Improperly defined copy. . . direction needed." More than three arguments SHALL print usage. The pointer and `to` forms require the box to be in the selection's window. The pointer form also requires the pointer to be in a window on the edit cell. It is logged as `copy to Xi Yi` / `move to Xi Yi`.

After the operation, the box SHALL be translated by the same displacement if it is in the selection's root. For `move`, this also happens when there is no selection.

- `copy` SHALL copy the entire selection, displaced, into the edit cell. Subcell instance identifiers SHALL be made unique in the edit cell. The copy SHALL become the new selection.
- `move` SHALL translate the edit-cell parts of the selection (delete, then re-add displaced). The moved items become the selection.

Compatibility: exact

(Magic ref: commands/CmdCD.c:2653-2800, commands/CmdLQ.c:673-860, select/selOps.c:286-345, select/selCreate.c:1270-1376)

#### Scenario: Copy east
- **GIVEN** units `internal`, metal1 (0,0)–(10,10) selected, and the box (0,0)–(10,10)
- **WHEN** `copy e 20` is run
- **THEN** metal1 SHALL exist at both (0,0)–(10,10) and (20,0)–(30,10)
- **AND** the box SHALL be (20,0)–(30,10)
- **AND** the selection SHALL be the copy

#### Scenario: Move by dx dy
- **GIVEN** units `internal` and metal1 (0,0)–(10,10) selected
- **WHEN** `move 5 7` is run
- **THEN** the metal1 SHALL be at (5,7)–(15,17)

### Requirement: Move origin

`move origin DIR [AMOUNT]`, `move origin DX DY` and `move origin to X Y` SHALL move the coordinate origin of the box's root cell by the displacement `(dx, dy)`, instead of moving the selection. Every paint tile, label, subcell use and stored point of the cell SHALL be translated by `(−dx, −dy)`. The box SHALL be translated by `(−dx, −dy)` so it stays on the same geometry. Windows SHALL be scrolled so the layout appears unchanged on screen. The undo history SHALL be discarded.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:840-870, database/DBcellsrch.c:2172-2260)

#### Scenario: Shift origin
- **GIVEN** units `internal` and the root cell containing metal1 at (10,10)–(20,20)
- **WHEN** `move origin e 10` is run
- **THEN** the metal1 SHALL be at (0,10)–(10,20)

### Requirement: Stretch command

`stretch [DIR [AMOUNT]]` SHALL move the edit-cell parts of the selection along a Manhattan direction (default 1 lambda). It erases material in the swept path and extends the material that touched the selection from behind. With no argument, the displacement is the pointer minus the box lower-left, reduced to its larger-magnitude axis, and the command is logged as `stretch n|s|e|w Ni`. Then:

1. The selection, displaced by `(dx, dy)`, is computed. The edit-cell parts of the original selection are deleted.
2. For each displaced paint tile, the area it swept through (the tile extended backward by the displacement) SHALL be cleared, in the edit cell, of non-contact types on that tile's plane. Contacts are handled by type: the same contact type is erased, and other contacts on the plane are reduced to their residues.
3. For each displaced paint tile, consider the 1-unit strip directly behind its trailing edge where the displaced selection has no paint. Translate the strip back by the displacement. For every active edit-cell tile found there (on planes of the selected type), the gap between the old trailing edge and the new position SHALL be painted with that tile's type, so that unselected material abutting the selection is stretched.
4. The displaced selection is then added to the edit cell and becomes the selection. The box moves with it if it is in the selection root.

A non-Manhattan DIR SHALL be rejected. A zero displacement does nothing.

Compatibility: exact

(Magic ref: commands/CmdRS.c:3238-3370, select/selOps.c:1634-2180)

#### Scenario: Stretch a wire end
- **GIVEN** units `internal`, a metal1 wire (0,0)–(30,4), and the selection holding only metal1 (20,0)–(30,4) (from `select area`)
- **WHEN** `stretch e 10` is run
- **THEN** the edit cell SHALL contain metal1 (0,0)–(40,4) as one continuous wire

### Requirement: Rotation and flip commands

The following commands SHALL transform the edit-cell parts of the selection by a fixed transform `T`. Transforms map `(x, y)` to `(a·x + b·y + c, d·x + e·y + f)`:

| Command | T (a b d e) |
|---|---|
| `clockwise` / `rotate` (default 90), `clockwise 90` | x' = y, y' = −x |
| `clockwise 180` | x' = −x, y' = −y |
| `clockwise 270` | x' = −y, y' = x |
| `sideways` | x' = −x, y' = y |
| `upsidedown` | x' = x, y' = −y |

The angle argument SHALL be an integer. A negative angle gets 360 added. Any result other than 90, 180 or 270 SHALL report "Rotation angle must be 90, 180, or 270 degrees". `rotate` is an alias of `clockwise` with identical semantics, so `rotate 90` rotates clockwise (see Q-CMDEDIT-61).

Unless a final argument beginning with `-orig` is given (`clockwise`/`rotate` and `orient` only), T SHALL be followed by a translation that brings the lower-left of the transformed selection bounding box back to the original selection bounding box's lower-left. With `-orig`, the rotation is about the root origin. The same composite transform SHALL be applied to the box if the box is in the selection's root. Transformed items replace the originals and become the selection. Labels have their justification, rotation and offset transformed. Subcell uses compose T with their transform.

Compatibility: exact

(Magic ref: commands/CmdCD.c:2306-2390, commands/CmdRS.c:2674-2735, commands/CmdTZ.c:1027-1080, utils/geometry.c:39-51, select/selOps.c:1055-1245)

#### Scenario: Clockwise keeps lower-left
- **GIVEN** metal1 selected at (10,10)–(30,20) and the box (10,10)–(30,20)
- **WHEN** `clockwise` is run
- **THEN** the metal1 SHALL be at (10,10)–(20,30)
- **AND** the box SHALL be (10,10)–(20,30)

#### Scenario: Negative angle
- **WHEN** `rotate -90` is run
- **THEN** the selection SHALL be transformed as by `clockwise 270`

#### Scenario: Rotate about origin
- **GIVEN** metal1 selected at (10,10)–(30,20)
- **WHEN** `clockwise 90 -orig` is run
- **THEN** the metal1 SHALL be at (10,−30)–(20,−10)

### Requirement: Orient command

`orient ORIENT [-orig]` SHALL apply a relative transform to the selection, as the rotation commands do. ORIENT is matched with the keyword-matching rule against the table below (case-insensitive, so `n` is `N`):

| Names | Transform |
|---|---|
| `0`, `N` | identity |
| `90`, `E` | clockwise 90 |
| `180`, `S` | 180 |
| `270`, `W` | clockwise 270 |
| `h`, `0h`, `180v`, `FN` | sideways (x' = −x) |
| `v`, `0v`, `180h`, `FS` | upside down (y' = −y) |
| `90h`, `270v`, `FE` | x' = −y, y' = −x |
| `270h`, `90v`, `FW` | x' = y, y' = x |

An unknown or ambiguous name (for example `9`) SHALL print `Usage: orient [orientation]`. The orientation is applied relative to the current one (composed with it), not set absolutely, as Magic implements it. `180v` SHALL map to sideways, as `getcell` and `dump` map it, whatever the `magic-compat` setting; Magic lists `180v` in its keyword table but has no case for it and prints the usage, so accepting it only accepts a form Magic rejects (see Q-CMDEDIT-62, resolution: fix).

Compatibility: exact

(Magic ref: commands/CmdLQ.c:3077-3202, commands/CmdLQ.c:3088-3167, commands/CmdCD.c:5344-5345; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-62)

#### Scenario: FN is a horizontal flip
- **GIVEN** metal1 selected at (10,10)–(30,20)
- **WHEN** `orient FN` is run
- **THEN** the metal1 SHALL still occupy (10,10)–(30,20), mirrored about the vertical axis

#### Scenario: 180v is sideways
- **GIVEN** metal1 selected at (10,10)–(30,20)
- **WHEN** `orient 180v` is run
- **THEN** the result SHALL be the same as for `orient FN`, and no usage SHALL be printed

#### Scenario: Orientation is relative
- **GIVEN** a selected instance with orientation `90`
- **WHEN** `orient 90` is run
- **THEN** the instance orientation SHALL be `180`

### Requirement: Array command

`array [-list] ...` SHALL create or query arrays from the selection:

- `array NX NY` or `array count NX NY`: indices `0..NX−1` by `0..NY−1`. NX and NY must be integers ≥ 1.
- `array XLO XHI YLO YHI` or `array count XLO XHI YLO YHI`: explicit index ranges, which may be descending.
- The x and y spacing SHALL be the box width and height (root coordinates). With no box, the command reports "Position the box to indicate the array spacing."
- Edit-cell **subcell uses** in the selection SHALL become array uses with the given indices and spacing. The spacing is converted into the use's own coordinate frame. The new array SHALL be placed so that its bounding-box lower-left equals the original use's bounding-box lower-left.
- Edit-cell **paint** SHALL be replicated `(|XHI−XLO|+1) × (|YHI−YLO|+1)` times, at offsets `(i·xsep, j·ysep)`.
- Edit-cell **labels** SHALL be replicated the same way. In each copy, the first number in the label text is replaced by `n1 + i` and the second by `n2 + j`, where `n1` and `n2` are the numbers in the original text and `i`, `j` the column and row index. For a one-dimensional array, the single varying index increments both (verify against Magic). Text with no digits is copied unchanged.
- The result replaces the originals and becomes the selection.
- Queries: `array count`, `array width`, `array height`, `array pitch` and `array position`, with no values, SHALL report for each selected use its index ranges, x separation, y separation, both, or bounding-box lower-left. In scripting mode with `-list`: with `magic-compat` ON (the default), the result SHALL be the flat list holding the values of the last use reported, as in Magic, where each use's query replaces the result; with it OFF, the result SHALL be a list with one sublist per selected array use, in the order the uses are reported, each sublist holding that use's values (see Q-CMDEDIT-63, resolution: replicate in magic-compat, fix in native).
- Setting `width`, `height`, `pitch` or `position` SHALL print "Unimplemented function." and change nothing, whatever the `magic-compat` setting. With `magic-compat` OFF the decision calls for implementing these setters, but that part is not implemented; it is deferred to a later milestone by user decision, and M2 implements only the native list-of-lists query (see Q-CMDEDIT-63).
- Bad arguments print the option help.

Compatibility: exact

(Magic ref: commands/CmdAB.c:197-560, select/selOps.c:1423-1630, netmenu/NMlabel.c:86-200; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-63)

#### Scenario: Array a cell
- **GIVEN** a selected edit-cell use of `inv` with bounding box (0,0)–(10,20) and the box (0,0)–(12,25)
- **WHEN** `array 3 2` is run
- **THEN** the use SHALL become an array with x indices 0..2, y indices 0..1, x separation 12, y separation 25, and bounding box (0,0)–(34,45)

#### Scenario: Label numbering
- **GIVEN** a selected label `D0` and the box width 10, height 10
- **WHEN** `array 4 1` is run
- **THEN** labels `D0`, `D1`, `D2`, `D3` SHALL exist at x offsets 0, 10, 20, 30

#### Scenario: array -list with magic-compat ON
- **GIVEN** `magic-compat` ON and two selected array uses, the first reported with indices 0..2 by 0..1 and the second reported with indices 0..3 by 0..0
- **WHEN** `array -list count` is run in scripting mode
- **THEN** the result SHALL be the flat list `0 3 0 0` of the last use reported

#### Scenario: array -list with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the same selection
- **WHEN** `array -list count` is run in scripting mode
- **THEN** the result SHALL be the list of lists `{0 2 0 1} {0 3 0 0}`
- **AND** with only the first use selected, the result SHALL be `{0 2 0 1}`

#### Scenario: Array setters are unimplemented
- **GIVEN** a selected array use
- **WHEN** `array width 20` is run, with `magic-compat` ON or OFF
- **THEN** "Unimplemented function." SHALL be printed and the use SHALL not change

### Requirement: What command

`what [-list|-listall]` SHALL describe the selection:

- **Paint**: the set of selected user types. Stacked contact types are replaced by their residues. Interactively, under `Selected mask layers:`, each type is printed. When the selection root is the edit root, each type is followed by the names of the cells that contain it.
- **Labels**: under `Selected label(s):`, the distinct labels sorted by text, then cell name, then type (descending). Each is printed as `"TEXT" is attached to TYPE in cell CELL`, with ` (N instances)` when the same text and cell occur N > 1 times.
- **Subcells**: under `Selected subcell(s):`, `Instance "USEID" of cell "DEF"` for each selected use.
- `-list` (scripting) SHALL return a 3-element list `{types} {{text type cell} …} {{useid def} …}`. `-listall` returns, as the first element, a list of `{type {cell …}}` pairs instead.

Any other argument SHALL report `Usage: what [-list]`. `what` does not change anything.

Compatibility: behavioral

(Magic ref: commands/CmdTZ.c:1196-1689)

#### Scenario: What in list form (verify against Magic)
- **GIVEN** the selection contains metal1 paint, label `A` on metal1 in cell `top`, and use `x1` of `inv`
- **WHEN** `what -list` is run in scripting mode
- **THEN** the result SHALL be `{metal1} {{A metal1 top}} {{x1 inv}}`

### Requirement: Identify command

`identify ID` and its alias `id ID` SHALL rename the first selected subcell use to ID:

- ID SHALL not contain `,` or `/` (or the other characters rejected for instance identifiers). Otherwise the command fails with an illegal-character error.
- The use SHALL be a child of the edit cell, and not a window's top-level use.
- ID SHALL be unique among the uses of the parent. Otherwise the command reports "New name isn't unique within its parent definition." and changes nothing.
- If no subcell is selected, it SHALL report "There isn't a selected subcell; can't change ids."

On success, both the layout use and the selection copy SHALL take the new identifier, and the parent cell SHALL be marked modified.

Compatibility: behavioral

(Magic ref: commands/CmdFI.c:1553-1627, commands/CmdSubrs.c:1343-1375)

#### Scenario: Rename instance
- **GIVEN** a selected use `inv_0` that is a child of the edit cell
- **WHEN** `identify buf1` is run
- **THEN** the instance identifier SHALL be `buf1`

#### Scenario: Duplicate rejected
- **GIVEN** the edit cell already has a use `buf1`
- **WHEN** `identify buf1` is run on another selected use
- **THEN** the command SHALL fail and the identifier SHALL be unchanged
