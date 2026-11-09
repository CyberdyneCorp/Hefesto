# Paint and Erase Commands

## Purpose

Defines the commands that change mask paint in the edit cell: `paint`, `erase`, `fill`, `corner`, `contact`, `polygon`, `splitpaint`, `spliterase` and `drop`. It also defines the layer-list syntax (including the specials `*`, `$`, `errors`, `labels`, `subcell`, `connect`) that these and many other commands share. Box semantics are in capability `cmd-box`, coordinates in `cmd-coordinates-and-units`, selection in `cmd-selection`. How painting one type over another resolves (paint/erase result tables, contact residues, stacked contacts) is defined by the technology (see capability `tech-loading-and-scaling` and the technology-file capabilities). This capability specifies which rectangles and triangles each command paints or erases, and with which types. The command surface is `behavioral`. The resulting geometry is `exact`.

## Requirements

### Requirement: Layer list syntax

A layer-list argument SHALL be parsed into a set of types as follows. Grammar:

```
layerlist = { sep | "+" | "-" | name } ;
sep       = "," | " " ;
name      = 1*( any character except "," "+" "-" " " ) ;
```

Processing:

1. Start with the empty set, in **add** mode.
2. `+` switches to add mode and `-` switches to remove mode. The mode persists until the next `+` or `-`.
3. Each `name` is first resolved as a technology type name or alias, which may denote several types (see the technology capabilities). If it resolves, those types are added to or removed from the set.
4. If the name is not a type, it SHALL be matched with the keyword-matching rule against the specials below. A match is added or removed like a type.
5. If the name is ambiguous as a type, the command SHALL report "Ambiguous layer: NAME". If it matches neither a type nor a special, it SHALL report "Unrecognized layer: NAME". In both cases it SHALL list the legal type names and specials, and the command SHALL fail without effect.

| Special | Meaning |
|---|---|
| `*` | every type except space, the DRC error types, the label pseudo-type and the subcell pseudo-type (it does include stacked contact types) |
| `$` | the types visible under the pointer: types of all tiles in the one-unit area at the pointer point, across all cells expanded in that window, restricted to the window's visible layers, with DRC error types removed (space may be included). Fails if the pointer is not in a layout window. |
| `errors` | the three DRC error types |
| `labels` | the label pseudo-type |
| `subcell` | the subcell pseudo-type |
| `connect` | every type that connects (per the technology connectivity) to any type already in the set at this point of the parse |

Names longer than 49 characters are not supported (see Q-CMDEDIT-18). Because `-` is a separator, type names containing `-` cannot be named in a layer list.

Compatibility: exact

(Magic ref: commands/CmdSubrs.c:486-652, database/DBcellsrch.c:927-970)

#### Scenario: All but one
- **GIVEN** a technology with types `poly`, `metal1`, `metal2`, `via`
- **WHEN** the list `*,-metal2` is parsed
- **THEN** the set SHALL be {poly, metal1, via} (plus any other non-error user types)

#### Scenario: Mode persists
- **WHEN** the list `-poly,metal1` is parsed
- **THEN** the set SHALL be empty, because both names are removed in remove mode

#### Scenario: Connect special
- **GIVEN** `metal1` connects to `metal1` and `via`
- **WHEN** the list `metal1,connect` is parsed
- **THEN** the set SHALL be {metal1, via}

#### Scenario: Unknown name
- **WHEN** `paint foo` is run and `foo` is not a type or special
- **THEN** "Unrecognized layer: foo" SHALL be reported, followed by the legal names
- **AND** nothing SHALL be painted

### Requirement: Common effects of edit-cell paint changes

Unless a command's requirement says otherwise, every command in this capability that changes paint SHALL:

1. Operate on the current edit cell, using the box transformed into edit coordinates (see capability `cmd-box`). It SHALL fail if there is no box, or the box is not in a window on the edit cell, or there is no editable edit cell.
2. Record every paint change in the undo history. All changes made by one command form one undo unit.
3. Mark the edit cell as modified.
4. Re-attach labels whose underlying paint changed in the changed area, using the label-layer rules of the layout database. Sticky labels keep their layer.
5. Clear the selection.
6. Recompute the edit cell's bounding box.
7. Queue the changed area for incremental DRC (see capability `drc-commands`).

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:944-986, commands/CmdE.c:672-790)

#### Scenario: Paint is one undo unit
- **GIVEN** an empty edit cell and the box (0,0)–(10,10)
- **WHEN** `paint metal1,poly` is run and then `undo` is run once
- **THEN** both the metal1 and the poly SHALL be gone

### Requirement: Paint command

`paint LAYERS` SHALL paint every type in the parsed set over the whole box area (in edit coordinates):

- Space is removed from the set.
- If the set includes the label pseudo-type, the command SHALL fail with "Label layer cannot be painted. Use the "label" command". If it includes the subcell pseudo-type, it SHALL fail with "Subcell layer cannot be painted. Use "getcell"." In both cases nothing is painted.
- Each stacked contact type in the set SHALL be replaced by its residue contact types.
- Types that are not active in the technology SHALL be dropped.
- For a contact type whose residues are not all active, only its active residues SHALL be painted, and the contact type itself SHALL NOT be.
- The remaining types SHALL be painted one at a time in increasing type order, each over the full box rectangle, using the technology's paint rules.

`paint` takes exactly one argument besides the `pick` form. Any other argument count SHALL report `Usage: paint <layers> | cursor | pick x y`. A zero-area box paints nothing.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:896-986, database/DBpaint2.c:256-316)

#### Scenario: Paint two layers
- **GIVEN** an empty edit cell (root = edit, identity transform) and the box (0,0)–(10,20)
- **WHEN** `paint poly,metal1` is run
- **THEN** the edit cell SHALL contain a poly rectangle (0,0)–(10,20) and a metal1 rectangle (0,0)–(10,20)

#### Scenario: Labels cannot be painted
- **WHEN** `paint labels` is run
- **THEN** the command SHALL fail with the label-layer error and change nothing

#### Scenario: Painted in edit coordinates
- **GIVEN** the edit cell is a child placed in the root with translation (+100, +0), and the root box is (100,0)–(110,10)
- **WHEN** `paint metal1` is run
- **THEN** the child cell SHALL contain metal1 at (0,0)–(10,10) in its own coordinates

### Requirement: Paint or erase by example (cursor and pick)

`paint cursor`, `erase cursor` (the argument begins with `cursor`), `paint pick X Y` and `erase pick X Y` (exactly the word `pick` and two coordinates in edit-cell coordinates) SHALL choose the types to paint or erase from the layout at a reference point:

1. Reference area. For `pick`, it is the unit square `(X', Y')–(X'+1, Y'+1)`, where `(X', Y')` is `(X, Y)` transformed to root coordinates. For `cursor`, it is the area of the screen pixel under the pointer, in root coordinates, without snapping.
2. The type set is every type of any tile overlapping the reference area, searched through all cells expanded in that window, restricted to active types and to the window's visible layers, with space removed.
3. If the set is **empty**, the command SHALL erase from the box area every type that is both visible and active. It SHALL also erase labels in the box area: any label on a visible type whose paint disappeared and, if the box has zero area, every label in the box area (label-area rule below).
4. Otherwise `paint` SHALL paint the set as in "Paint command", and `erase` SHALL erase the set as in "Erase command", including erasing labels that lost their paint.
5. The common effects apply, and the changes SHALL form one undo unit.

The `cursor` form SHALL be recorded in the command log as `paint pick Xi Yi` / `erase pick Xi Yi`, using the edit-coordinate pointer point in internal units. This is also the action of the middle pointer button in the box tool.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:913-937, commands/CmdLQ.c:989-1060, commands/CmdE.c:617-660)

#### Scenario: Copy layers from an example point
- **GIVEN** the edit cell contains metal1 and poly overlapping at (5,5)–(6,6), and the box is (20,0)–(30,10)
- **WHEN** `paint pick 5 5` is run with units `internal`
- **THEN** metal1 and poly SHALL both be painted over (20,0)–(30,10)

#### Scenario: Pick on empty space erases
- **GIVEN** the box (0,0)–(10,10) covers metal1, and the point (50,50) is empty
- **WHEN** `paint pick 50 50` is run
- **THEN** all visible, active paint in (0,0)–(10,10) SHALL be erased

### Requirement: Erase command

`erase [LAYERS]` SHALL remove paint (and possibly labels and subcells) from the box area:

1. With no argument, or a list equal to `space`, the set SHALL be `*,labels`.
2. Space is removed. An empty set SHALL do nothing.
3. **Paint**: for each user type in the set that is active, in increasing type order, that type SHALL be erased over the box using the technology's erase rules. For a contact whose residues are not all active, only its active residues are erased. A stacked contact type named directly in the set (by its `stackable` alias, for example `v12`): with `magic-compat` ON (the default), it SHALL NOT be erased by this step, as in Magic, which loops only over user types below the number of user layers; with it OFF, it SHALL be erased as its residue contacts, mirroring how `paint` handles a named stacked type (see Q-CMDEDIT-16, resolution: replicate in magic-compat, fix in native).
4. **Error layers**: DRC error types in the set SHALL be erased from the box with no activity filtering, in decreasing type order.
5. **Labels**: a label SHALL be deleted if its rectangle is in the box area (label-area rule) and either:
   - the set contains the label pseudo-type, or
   - its type is in the set, and after the erase the best remaining layer under it does not connect to its type. Labels attached to space are always deleted in this case.
6. **Subcells**: if the set contains the subcell pseudo-type, every subcell use of the edit cell whose bounding box overlaps the box SHALL be deleted. An array use is deleted as a whole. The parent's DRC is queued over each deleted use's bounding box.
7. DRC is not queued when only labels were erased.

**Label-area rule**: a label rectangle L is in area A when A contains L (boundaries may touch). When A has zero area, L is also in A if L touches A and A is not strictly inside L's interior, so a point box picks up the labels it touches. This is a deliberate rule of Magic and Hefesto SHALL apply it in both `magic-compat` modes, because it decides which labels `erase` and selection affect (see Q-CMDEDIT-20, resolution: replicate).

More than one argument, other than `pick X Y`, SHALL report `Usage: erase [<layers> | cursor | pick x y]`. The documentation describes `$` as "paint and labels", but `$` means the types under the pointer (see Q-CMDEDIT-14).

Compatibility: exact

(Magic ref: commands/CmdE.c:607-805, database/DBpaint2.c:333-417, database/DBlabel.c:263-323, utils/geometry.h:149-152; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-14, docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-16, docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-20)

#### Scenario: Erase everything
- **GIVEN** the box (0,0)–(10,10) contains metal1, poly and a label `A` at (2,2)–(2,2) attached to metal1
- **WHEN** `erase` is run
- **THEN** all paint in the box SHALL be gone and label `A` SHALL be deleted

#### Scenario: Erase one layer keeps labels on surviving paint
- **GIVEN** a label `B` at (2,2) attached to metal1 inside the box, and metal1 extends outside the box under the label
- **WHEN** `erase poly` is run
- **THEN** label `B` SHALL remain

#### Scenario: Erase subcells
- **GIVEN** two subcell uses whose bounding boxes overlap the box, and one that does not
- **WHEN** `erase subcell` is run
- **THEN** exactly the two overlapping uses SHALL be deleted

#### Scenario: Named stacked contact with magic-compat ON
- **GIVEN** `magic-compat` ON, a technology whose stacked contact `v12` stacks `via1` and `via2`, and `v12` painted over the box (0,0)–(10,10) in the edit cell
- **WHEN** `erase v12` is run
- **THEN** the edit cell SHALL be unchanged, as in Magic (golden case CMDEDIT-PAINT-09)

#### Scenario: Named stacked contact with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the same technology and paint
- **WHEN** `erase v12` is run
- **THEN** `v12` SHALL be erased as its residue contacts, leaving no `v12` in (0,0)–(10,10)

#### Scenario: Point box picks up a touching label
- **GIVEN** a label `A` with rectangle (0,0)–(4,4) attached to metal1, and the box is the point (4,2)–(4,2)
- **WHEN** `erase labels` is run
- **THEN** label `A` SHALL be deleted, whatever the `magic-compat` setting

### Requirement: Fill command

`fill DIR [LAYERS]` SHALL extend paint across the box in the Manhattan direction DIR. DIR must be Manhattan. LAYERS defaults to every type except space and the DRC types. With the root box `B = (X0,Y0)–(X1,Y1)`:

1. The **entry strip** SHALL be a 2-unit-wide band straddling the entry side: for `north`, `(X0, Y0−1)–(X1, Y0+1)`; for `south`, `(X0, Y1−1)–(X1, Y1+1)`; for `east`, `(X0−1, Y0)–(X0+1, Y1)`; for `west`, `(X1−1, Y0)–(X1+1, Y1)`.
2. Every tile of a type in LAYERS overlapping the strip SHALL be collected. The search covers the window's top cell and all cells expanded in that window, in root coordinates.
3. Each tile rectangle SHALL be clipped to B. Its far coordinate SHALL then be set to B's far side: `ytop = Y1` for north, `ybot = Y0` for south, `xtop = X1` for east, `xbot = X0` for west.
4. Each resulting rectangle SHALL be transformed to edit coordinates and painted into the edit cell with the tile's type. This applies even when the tile came from a subcell.

So paint that crosses or touches the entry side from outside is extended. Paint inside the box that does not reach the entry side is unchanged.

Compatibility: exact

(Magic ref: commands/CmdFI.c:506-640)

#### Scenario: Fill north
- **GIVEN** the box (0,0)–(10,20), poly at (2,−5)–(4,0) (touching the bottom side from below), and metal1 at (6,5)–(8,8)
- **WHEN** `fill n` is run
- **THEN** poly SHALL be painted at (2,0)–(4,20)
- **AND** the metal1 SHALL be unchanged

#### Scenario: Fill restricted to a layer
- **GIVEN** the same layout
- **WHEN** `fill n metal1` is run
- **THEN** nothing SHALL be painted

### Requirement: Corner command

`corner D1 D2 [LAYERS] [bevel]` SHALL make L-shaped wires inside the box. D1 and D2 are Manhattan directions and must be perpendicular. Otherwise the command SHALL print "Can't corner-fill D1 and then D2." and do nothing. The final argument is taken as `bevel` when it begins with `bevel` and there are more than two other arguments. LAYERS defaults to every type except space and the DRC types.

Tiles SHALL be collected exactly as for `fill D1`. For each tile, let `r` be the tile clipped to the root box `B = (X0,Y0)–(X1,Y1)`, `w = r.xtop − r.xbot` and `h = r.ytop − r.ybot`. The first leg `r2` and second leg `r3` (initially `r3 = r`) SHALL be:

| D1 D2 | first leg r2 | second leg r3 | error if |
|---|---|---|---|
| n e | r with `ytop = Y1 − (r.xbot − X0)` | `xbot = r.xbot, xtop = X1, ytop = r2.ytop, ybot = ytop − w` | `r3.ybot < Y0` |
| n w | r with `ytop = Y1 − (X1 − r.xtop)` | `xbot = X0, xtop = r.xtop, ytop = r2.ytop, ybot = ytop − w` | `r3.ybot < Y0` |
| s e | r with `ybot = Y0 + (r.xbot − X0)` | `xbot = r.xbot, xtop = X1, ybot = r2.ybot, ytop = ybot + w` | `r3.ytop > Y1` |
| s w | r with `ybot = Y0 + (X1 − r.xtop)` | `xbot = X0, xtop = r.xtop, ybot = r2.ybot, ytop = ybot + w` | `r3.ytop > Y1` |
| e n | r with `xtop = X1 − (r.ybot − Y0)` | `ybot = r.ybot, ytop = Y1, xtop = r2.xtop, xbot = xtop − h` | `r3.xbot < X0` |
| e s | r with `xtop = X1 − (Y1 − r.ytop)` | `ybot = Y0, ytop = r.ytop, xtop = r2.xtop, xbot = xtop − h` | `r3.xbot < X0` |
| w n | r with `xbot = X0 + (r.ybot − Y0)` | `ybot = r.ybot, ytop = Y1, xbot = r2.xbot, xtop = xbot + h` | `r3.xtop > X1` |
| w s | r with `xbot = X0 + (Y1 − r.ytop)` | `ybot = Y0, ytop = r.ytop, xbot = r2.xbot, xtop = xbot + h` | `r3.xtop > X1` |

Both legs SHALL be clipped to B and transformed to edit coordinates. Each non-empty leg SHALL be painted with the tile's type. If the error condition holds for any tile, the command SHALL report "There's not enough room in the box for all the wires." The legs are still painted.

With `bevel`, each wire SHALL instead be painted as one polygon with a 45-degree mitred bend, built from the same two legs reduced to the wire width. The polygon SHALL be painted into the edit cell, in edit coordinates, through the normal paint path (with the technology's paint and contact rules), and the edit cell SHALL be marked modified, whatever the `magic-compat` setting. Magic instead paints bevel paths directly into the planes of the edit window's root cell, so with a child edit cell it changes a different cell at the wrong place, without contact handling or a modified flag; Hefesto does not reproduce this (see Q-CMDEDIT-15, resolution: fix). When the edit cell is the root cell the result SHALL equal Magic's (golden case CMDEDIT-PAINT-10).

Compatibility: exact

(Magic ref: commands/CmdCD.c:2863-3200, commands/CmdCD.c:2970-3016, commands/CmdCD.c:3227-3729; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-15)

#### Scenario: Corner north then east
- **GIVEN** the box (0,0)–(20,20) and a metal1 wire at (4,−10)–(8,0)
- **WHEN** `corner n e` is run
- **THEN** metal1 SHALL be painted at (4,0)–(8,16) and at (4,12)–(20,16)

#### Scenario: Not perpendicular
- **WHEN** `corner n s` is run
- **THEN** "Can't corner-fill n and then s." SHALL be printed and nothing painted

#### Scenario: Bevel corner (verify against Magic)
- **GIVEN** the same box and wire
- **WHEN** `corner n e metal1 bevel` is run
- **THEN** a metal1 polygon with a 45-degree bend SHALL connect the entry at (4..8, 0) to the exit on the east side

#### Scenario: Bevel corner in a child edit cell
- **GIVEN** the window's root cell `top` contains an instance of `child` translated by (100,0), `child` is the edit cell, and the box and wire of the scenario above are in `top`'s coordinates translated by (100,0)
- **WHEN** `corner n e metal1 bevel` is run
- **THEN** the bevel polygon SHALL be painted into `child`, at the same place as the unbeveled corner in `child`'s coordinates, and `child` SHALL be marked modified
- **AND** `top` SHALL gain no paint

### Requirement: Contact command

`contact TYPE` SHALL paint the contact TYPE wherever its first two residue types overlap inside the box, in the edit cell only:

1. TYPE SHALL be a contact type. Otherwise the command SHALL fail with "Error: tile type "TYPE" is not a contact."
2. Let `R1` and `R2` be TYPE's two lowest-numbered residue types.
3. For each tile of exact type `R1` in the edit cell that overlaps the box, clip it to the box (area `A`). For each tile of exact type `R2` that overlaps `A`, the intersection with `A` SHALL be painted with TYPE, if TYPE is active.
4. Only paint in the edit cell is considered, not subcells. A residue already covered by another contact (so that its tile has a different type) is not found.

`contact erase TYPE` (exactly the word `erase`) SHALL remove TYPE from the box area and leave its residues:

1. TYPE SHALL be a contact.
2. Every tile in the edit cell (on TYPE's home plane) of TYPE, or of any stacked contact that contains TYPE, that overlaps the box SHALL be clipped to the box. On each clipped rectangle, TYPE SHALL be erased, and then each user residue type of TYPE SHALL be painted, in increasing type order.
3. Any other three-argument form SHALL report `Usage: contact erase <contact_type>`.

`contact` does not clear the selection or re-attach labels. It does queue DRC and record undo.

Compatibility: exact

(Magic ref: commands/CmdCD.c:2421-2560, commands/CmdCD.c:2565-2640)

#### Scenario: Contact on overlap
- **GIVEN** contact type `m2c` with residues metal1 and metal2, metal1 at (0,0)–(6,10), metal2 at (3,0)–(10,10), and the box (0,0)–(10,10)
- **WHEN** `contact m2c` is run
- **THEN** `m2c` SHALL be painted at (3,0)–(6,10)

#### Scenario: Contact erase keeps metals
- **GIVEN** `m2c` at (3,0)–(6,10) and the box covering it
- **WHEN** `contact erase m2c` is run
- **THEN** (3,0)–(6,10) SHALL contain metal1 and metal2 and no `m2c`

### Requirement: Polygon command

`polygon TYPE x1 y1 x2 y2 x3 y3 [… xn yn]` SHALL paint TYPE inside the polygon with the given vertices in the edit cell:

- At least three vertices (8 words) are required. Otherwise it SHALL report the usage.
- An odd number of coordinate values SHALL report "Unpaired coordinate value".
- An unknown TYPE SHALL fail with the type-lookup error.
- The vertices are absolute coordinates in **edit-cell** coordinates. They are not transformed through the box or the root, and no box is required.
- The polygon SHALL be implicitly closed. It SHALL be converted to Manhattan rectangles and 45-degree or arbitrary-angle triangles exactly as polygon records are converted on CIF/GDS input (see the MASK capabilities). It SHALL be painted on every plane where TYPE is painted, using the paint rules.

The documentation states that only convex polygons are guaranteed. For non-convex or self-intersecting vertex lists, Hefesto SHALL produce the result of the same nonzero-winding strip decomposer that CIF and GDS polygon input use (capability `mask-input-styles`), as Magic does, whose `polygon` command calls that decomposer; Hefesto SHALL share one decomposer between this command and MASK input (see Q-CMDEDIT-19, resolution: replicate). The cell is marked modified, its bounding box is recomputed and DRC is queued over the polygon's bounding box. The selection is not cleared and labels are not re-attached.

Compatibility: exact

(Magic ref: commands/CmdLQ.c:1215-1270, cif/CIFrdpt.c:604-636; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-19)

#### Scenario: Right triangle (verify against Magic)
- **GIVEN** units `internal`
- **WHEN** `polygon metal1 0 0 10 0 0 10` is run
- **THEN** the edit cell SHALL contain one metal1 split tile (0,0)–(10,10) with the material in the lower-left triangle

#### Scenario: Unpaired
- **WHEN** `polygon metal1 0 0 10 0 0 10 5` is run (9 words)
- **THEN** "Unpaired coordinate value" SHALL be reported and nothing painted

#### Scenario: Self-intersecting polygon uses the MASK decomposer
- **GIVEN** units `internal`
- **WHEN** `polygon metal1 0 0 10 10 10 0 0 10` is run (a bow-tie)
- **THEN** the edit cell SHALL contain the same metal1 tiles that CIF input produces for a polygon record with the same vertices

### Requirement: Splitpaint command

`splitpaint DIR LAYER [LAYER2]` SHALL split the box diagonally. It SHALL paint LAYER in the right triangle whose right-angle vertex is the box corner named by DIR, and LAYER2 (if given) in the opposite triangle:

- DIR names the corner: `ne`, `nw`, `se` or `sw` (or a synonym from capability `cmd-coordinates-and-units`). For a Manhattan name or `center`: with `magic-compat` ON (the default), it SHALL be mapped as Magic does, `north`→`nw`, `east`→`ne`, `south`→`se`, `west`→`sw` and `center`→`nw`, and Hefesto SHALL report the warning `Direction "<DIR>" is not a corner; using <corner>`; with it OFF, the command SHALL fail with `Direction "<DIR>" is not a corner; use ne, nw, se or sw` and change nothing (see Q-CMDEDIT-21, resolution: replicate in magic-compat, fix in native).
- LAYER and LAYER2 are layer lists. Space is removed, and inactive types are removed from LAYER.
- A zero-area box SHALL do nothing.
- Every type in LAYER is painted as a triangle on each plane where it is painted, using the technology's paint rules for non-Manhattan tiles. The same applies to LAYER2 for the opposite triangle. Afterwards, adjacent compatible triangles are merged within the box area grown by 1.
- The triangle orientation SHALL be the root-coordinate triangle, transformed into the edit cell.
- The selection is cleared, the cell is marked modified, the bounding box is recomputed and DRC is queued. Labels are not re-attached.

Compatibility: exact

(Magic ref: commands/CmdRS.c:3040-3115, commands/CmdRS.c:3087-3088; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-21)

#### Scenario: NE triangle
- **GIVEN** the box (0,0)–(10,10)
- **WHEN** `splitpaint ne metal1 poly` is run
- **THEN** metal1 SHALL occupy the triangle (0,10),(10,10),(10,0)
- **AND** poly SHALL occupy the triangle (0,0),(0,10),(10,0)

#### Scenario: Manhattan direction with magic-compat ON
- **GIVEN** `magic-compat` ON and the box (0,0)–(10,10)
- **WHEN** `splitpaint east metal1` is run
- **THEN** the warning `Direction "east" is not a corner; using ne` SHALL be reported
- **AND** metal1 SHALL occupy the triangle (0,10),(10,10),(10,0)

#### Scenario: Manhattan direction with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the box (0,0)–(10,10)
- **WHEN** `splitpaint east metal1` is run
- **THEN** the command SHALL fail with `Direction "east" is not a corner; use ne, nw, se or sw`
- **AND** nothing SHALL be painted

### Requirement: Spliterase command

`spliterase DIR [LAYER]` SHALL erase the types in LAYER from the right triangle of the box whose right-angle vertex is the corner named by DIR (same corner names, Manhattan-name mapping, warning and `magic-compat` OFF error as `splitpaint`; see Q-CMDEDIT-21):

- LAYER defaults to `*`. A list equal to `space` means `*,labels`. Space is removed. An empty set does nothing. Inactive types are removed.
- A zero-area box does nothing.
- Each type is erased from the triangle on each of its planes, using the technology's erase rules. Triangles are then merged as for `splitpaint`.
- Labels are not erased. The label pseudo-type has no effect.

Compatibility: exact

(Magic ref: commands/CmdRS.c:3140-3214, commands/CmdRS.c:3184-3185; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-21)

#### Scenario: Erase a triangle
- **GIVEN** metal1 over the box (0,0)–(10,10)
- **WHEN** `spliterase sw metal1` is run
- **THEN** metal1 SHALL remain only in the triangle (0,10),(10,10),(10,0)

### Requirement: Drop command

`drop LAYERS` SHALL paint LAYERS into the cells (the edit cell and its descendants) that contain the paint now selected, at the places where that paint is:

1. A box SHALL exist in a window on the edit cell. LAYERS SHALL parse.
2. The root cell of the edit cell's window SHALL be the selection's root cell. Otherwise the command SHALL fail with "The selection does not match the edit cell." and change nothing, whatever the `magic-compat` setting. Magic accepts the case where only the top cell of the current window is the selection's root, and then searches the edit cell's hierarchy with an identity transform using another cell's root coordinates, painting unrelated cells; Hefesto refuses it (see Q-CMDEDIT-17, resolution: fix).
3. If the selection contains no paint (space and DRC types excluded), it SHALL do nothing.
4. For each selected paint tile of type T (for split tiles, the type of the selected side), at area A in root coordinates: search the whole hierarchy under the edit cell for tiles of exactly type T overlapping A (transformed into edit coordinates by the edit transform). All descendants are searched, whatever their expansion state. For every such tile found in a cell C, the tile area (in C's coordinates) clipped to A (transformed into C) SHALL be painted with every type in LAYERS, one type at a time, in C itself.
   Before anything is painted, every cell C that step 4 would paint SHALL be checked for editability. If any is not editable (for example a read-only or locked cell), the command SHALL fail with `Cannot drop into non-editable cell <C>` and paint nothing in any cell. Magic never checks this (see Q-CMDEDIT-17).
5. DRC is queued over the box area in the edit cell, and the bounding box is recomputed. The selection is NOT cleared.

Every instance of a modified child cell sees the change, because the cell definition is modified. Hefesto SHALL keep these all-instances semantics, which Magic documents (see Q-CMDEDIT-17).

Compatibility: exact

(Magic ref: commands/CmdCD.c:4614-4770, commands/CmdCD.c:4628-4693, commands/CmdCD.c:4769-4825, doc/html/drop.html; Hefesto decision: docs/decisions/open-questions/CMDEDIT.md#Q-CMDEDIT-17)

#### Scenario: Drop pwell into the cell owning the selected paint
- **GIVEN** the edit cell `top` contains a use of `child`, `child` contains `dnwell` at (0,0)–(10,10), and `select area dnwell` selected it through the hierarchy
- **WHEN** `drop pwell` is run
- **THEN** `child` SHALL contain `pwell` at (0,0)–(10,10)
- **AND** `top` SHALL gain no pwell

#### Scenario: Selection root differs from the edit cell's root
- **GIVEN** the selection was made in a window whose root is `top`, and the edit cell is in a window whose root is `other`
- **WHEN** `drop pwell` is run
- **THEN** the command SHALL fail with "The selection does not match the edit cell."
- **AND** no cell SHALL change

#### Scenario: Non-editable target cell
- **GIVEN** the scenario "Drop pwell into the cell owning the selected paint", with `top` also containing selected `dnwell` of its own, and `child` read-only
- **WHEN** `drop pwell` is run
- **THEN** the command SHALL fail with `Cannot drop into non-editable cell child`
- **AND** neither `top` nor `child` SHALL change

### Requirement: Paint commands in a non-editable context

If no cell is being edited, `paint` SHALL fail with "The cell is not editable." `erase` SHALL fail with "No cell def being edited!" `contact` SHALL fail with "The cell in the window is not editable." `polygon` SHALL fail with "No cell being edited". `paint`, `contact`, `splitpaint` and `spliterase` SHALL fail with "Put the cursor in a layout window" when no layout window can be determined. In all these cases nothing SHALL change.

Compatibility: behavioral

(Magic ref: commands/CmdLQ.c:903-908, commands/CmdLQ.c:940-944, commands/CmdE.c:636-641, commands/CmdCD.c:2421-2440, commands/CmdLQ.c:1207-1214)

#### Scenario: No edit cell
- **GIVEN** no cell is loaded for editing
- **WHEN** `polygon metal1 0 0 1 0 0 1` is run
- **THEN** "No cell being edited" SHALL be reported and nothing SHALL change
