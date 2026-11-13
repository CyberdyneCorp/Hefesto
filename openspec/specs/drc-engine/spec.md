# DRC Engine

## Purpose

Defines how Hefesto keeps DRC results up to date for a cell hierarchy in a Magic-compatible way: the per-cell check and error planes, how edits mark areas for rechecking, how the background (continuous) checker schedules and performs checks, how hierarchical interactions, arrays and exact overlaps are checked, how errors of subcells propagate into parents, and the flat check. The geometric rule checks themselves are defined in `drc-rule-semantics`; rule compilation in `drc-rule-language`. Hefesto leaves the same error tiles (type and geometry) in every cell's error plane as Magic 8.3.694 for the same tech, layout and sequence of checks; where a rule-level quirk is resolved as *compat*, this holds with the `magic-compat` setting ON (the default), as stated in `drc-rule-language` and `drc-rule-semantics`.

## Requirements

### Requirement: DRC check and error planes
Every cell definition SHALL carry two DRC planes: a check plane holding pending-check areas (type `checkpaint`, short `CP`; type `checksubcell`, `CS`, exists but is never painted by the checker, in either `magic-compat` setting, so `.mag` files only get `<< checkpaint >>` sections; a `checksubcell` area read from a file is still painted and checked; see Q-DRC-23, resolution: replicate) and an error plane holding error areas of types `error_p` (`EP`, errors found by checking the cell's own paint), `error_s` (`ES`, errors found in hierarchical interaction areas) and `error_ps` (`EPS`, both).
Compatibility: exact

Painting on the error plane composes as a 2-bit set: P over S or S over P gives PS; erasing P from PS leaves S and vice versa. Both planes are stored in maximal-horizontal-strip form (see capability `layout-geometry-model`). Check-plane and error-plane contents are written to and read from `.mag` files like paint (see capability `cell-file-format`); a `checkpaint` area read from a file is checked when the cell is next enqueued (Magic 8.3.694 does not enqueue a cell on reading its `<< checkpaint >>` section; a file with a format error enqueues its cell). Error tiles are excluded from paint-command output and from extraction, mask generation and selection of paint.

(Magic ref: database/DBtechtype.c:86-90, database/DBtpaint.c:95-102, database/DBio.c:5371, database/DBio.c:1053-1060)

#### Scenario: Error type composition
- **GIVEN** an error plane containing `error_s` over [0,10]×[0,10]
- **WHEN** an `error_p` rectangle [5,15]×[0,10] is painted
- **THEN** the plane contains `error_s` [0,5]×[0,10], `error_ps` [5,10]×[0,10] and `error_p` [10,15]×[0,10]

### Requirement: Marking changed areas
Whenever paint or subcells of a cell C change over area A (paint, erase, undo/redo, copy, move, delete, cell placement or removal, wiring, routing, plowing, reading CIF/GDS/LEF, timestamp-mismatch bounding box changes), the editor SHALL mark C for rechecking by painting `checkpaint` over A expanded by the current halo h on each side into C's check plane, appending C to the end of the pending-cell list if it is not already in it, and recursively marking every parent cell P (through every non-window use U of C) over the area of A transformed into P's coordinates, enlarged to cover all array elements of U.
Compatibility: exact

- The halo h is the current halo (initially the style halo; see `drc halo` in `drc-commands`). Because each level re-expands, the marked area grows by h per hierarchy level.
- Internal cells (the yank buffers and other hidden cells) are never marked.
- Marking with no area only enqueues the cell.
- For an array use, the area is the union of A transformed for element (xlo,ylo) and for element (xhi,yhi) (i.e. the bounding box of all element images).

(Magic ref: drc/DRCcontin.c:184-310)

#### Scenario: Two-level propagation
- **GIVEN** halo 3, a cell `leaf` used once in `top` with translation (100,0), and a paint change in `leaf` over [0,10]×[0,10]
- **WHEN** the change is marked
- **THEN** `leaf`'s check plane receives [−3,13]×[−3,13]
- **AND** `top`'s check plane receives [97,113]×[−3,13]
- **AND** the pending list is `leaf`, then `top`

### Requirement: Pending list and background scheduling
The background checker SHALL process the pending list in order, repeatedly taking the first pending cell, finding its first `checkpaint` tile in area-enumeration order, processing one step square for it, and removing the cell from the list (after recomputing its bounding box) when no check tiles remain; it SHALL run only when background checking is enabled and no command is executing, and SHALL abandon the current square without modifying the database when interrupted by user input.
Compatibility: behavioral

- Background states: off, on, and "not set". The state is set to "not set" for the duration of each command (unless off) and restored afterwards, so the checker never runs inside a command.
- After every command (except internal bypass commands) any in-progress check is broken and restarted later.
- Checking order matters for hierarchical results: children marked before their parents are checked first, so their error planes are current when parents copy them up.
- `drc catchup` runs the checker to completion regardless of the enabled state (see `drc-commands`).

Hefesto's mechanism (see Q-DRC-12, resolution: platform; the same on every platform, including iPadOS):
- The background checker runs on a worker thread, never on the UI thread, and checks one step square at a time over an immutable snapshot of the hierarchy the square reads (paint, uses, mask hints and the DRC style taken when the square starts).
- Squares are taken in Magic's order: pending cells in list order (children, appended before the parents that copy their errors up, are checked first), and within a cell its first `checkpaint` tile in area-enumeration order.
- When the square is done, its result (the erase of `checkpaint`, `error_p` and `error_s` and the paint of the new errors, see "Step square processing") is committed on the editor's thread as one atomic update, and only if no edit (any database change, including undo/redo and file reads) has been made since the snapshot was taken that touches the cells the square read; otherwise the result is discarded and the square's check tiles stay for a retry. Edits are tracked with an edit-generation counter, so a discarded square leaves the check and error planes exactly as before it started, as in the scenario below.
- A square is never committed while a command executes; commits happen between commands. A command that makes no edit does not discard the square in progress, which gives the same results as Magic's break-and-restart. A discarded square is abandoned at once, so a new edit's area can start being checked without waiting for it (`hefesto-performance` "Edit latency").
- `drc catchup` runs the checker synchronously on the calling thread to completion, with the same order and results.
- When the checker starts work on a non-empty pending list it emits `magic::drcstate busy`, and when the pending list becomes empty (or the checker stops) `magic::drcstate idle`, at the same transitions as Magic. They are delivered to the Tcl interpreter (capability `scripting-api`) and to the desktop shell, which shows them in its DRC status indicator; the `]` / `%` prompt switch of Magic's console is informative.

(Magic ref: drc/DRCcontin.c:481-600, drc/DRCcontin.c:439-470, textio/txCommands.c:1535-1570, drc/DRCmain.c:1007-1025)

#### Scenario: Interrupted check leaves no partial result
- **GIVEN** a pending cell whose square check is in progress
- **WHEN** the user issues a command before the square completes
- **THEN** the cell's check and error planes are unchanged by the aborted square
- **AND** the square is checked again later

#### Scenario: Edit during a worker check discards the square
- **GIVEN** background checking on and the worker checking a square of cell `inv` from a snapshot
- **WHEN** the user paints into `inv` before the square completes
- **THEN** the square's result is not committed, its `checkpaint` tiles remain, and the square is checked again from a new snapshot that includes the paint

#### Scenario: Children before parents
- **GIVEN** a paint change in `leaf`, used in `top`, with background checking on
- **WHEN** the checker runs
- **THEN** every square of `leaf` is committed before the first square of `top` starts, so `top` copies `leaf`'s new errors

#### Scenario: Busy and idle transitions
- **GIVEN** background checking on and an empty pending list
- **WHEN** a paint command marks an area and the checker then empties the pending list
- **THEN** `magic::drcstate busy` is emitted once when checking starts and `magic::drcstate idle` once when the list is empty

### Requirement: Step square processing
For a check tile T in cell C, the checker SHALL compute the step square S = [fx, fx+s]×[fy, fy+s] where s is the step size and fx, fy are the floor multiples of s at or below LEFT(T), BOTTOM(T); compute the erase box B as the bounding box of all check tiles overlapping S, clipped to S; compute the check box B⁺ = (B expanded by halo h) clipped to S; run the interaction check of C over area S with erase box B into a temporary error plane; and then atomically erase `checkpaint` from B, erase `error_p` from B, erase `error_s` from B⁺, and paint the temporary errors into C's error plane.
Compatibility: exact

- Errors produced by the plain (non-interaction) checks are painted as `error_p`; errors produced inside interaction areas (and illegal-overlap and off-grid flattening errors) as `error_s`.
- After the update, C is marked modified unless it is non-editable or its contents come from a vendor GDS library (`CDNOEDIT`/`CDVENDORGDS` equivalents); its timestamp is not changed. The DRC result is still kept in memory for such cells.
- The redisplay area is the XOR of old and new error areas clipped to S (informative).

(Magic ref: drc/DRCcontin.c:642-775)

#### Scenario: Negative coordinates
- **GIVEN** step size 100 and a check tile with lower-left (−30, 250)
- **WHEN** the square is computed
- **THEN** S = [−100,0]×[200,300]

### Requirement: Interaction check over an area
The interaction check of cell C over area Ar with erase box B SHALL partition Ar by a grid of squares of side s offset by −⌊s/2⌋ from the origin (first square x0 = ⌊Ar.xbot/s⌋·s − ⌊s/2⌋ using C truncating division, decreased by s if greater than Ar.xbot; same for y), and for each square Q with Ar' = Q ∩ Ar process it as specified below, where h is the current halo.
Compatibility: exact

```
I = interactionArea(C, Ar', h)          # see "Interaction area"
if I is none:                            # no subcell interactions
    U = B ∩ Ar'; if U null: continue
    basicCheck(C, K = U ⊕ h, L = U)      -> error_p
    copyChildErrors(C, U)                -> error_p (why 4)
    arrayChecks(C, U)                    -> error_p
else:
    Bc = B ∩ Ar'
    for each strip of Bc outside I, in order above, below, right, left:
        above = Bc with ybot = I.ytop          (if I.ytop < Bc.ytop)
        below = Bc with ytop = I.ybot          (if I.ybot > Bc.ybot)
        right = [I.xtop, Bc.xtop] × [I.ybot, I.ytop]  (if I.xtop < Bc.xtop)
        left  = [Bc.xbot, I.xbot] × [I.ybot, I.ytop]  (if I.xbot > Bc.xbot)
        basicCheck(C, strip ⊕ h, strip); copyChildErrors(C, strip); arrayChecks(C, strip)   -> error_p
    I = I ∩ (B ⊕ h)
    Y = flatten(C hierarchy, area I ⊕ h, DRC paint table)          # see Flattening
    basicCheck(Y, K = I ⊕ h, L = I)                                  -> error_s
    exactOverlapCheck(C, I)                                          -> error_s (why 3)
```

`R ⊕ h` is R expanded by h on all sides. The left and right strips use the y-extent of I as computed before clipping I to B ⊕ h.

With `magic-compat` ON, `copyChildErrors`, `arrayChecks`, `flatten` and `exactOverlapCheck` act on the scan cell, Magic's `DRCDummyUse` def, not on C: each square sets the scan cell to C, but an error function that re-enters the interaction check from a report (the why-4 resolution of `drc why` and `drc listall why`, drc-commands "drc why") leaves it on the last cell that re-entered check processed. The rest of that square then copies the errors of that cell's children (errors of C's other children in later strips are missed) and flattens that cell's paint with the identity transform, so its own errors are reported at its own coordinates as if they were C's. With `magic-compat` OFF the scan cell is always C. The background checker reports into the error plane without re-entering, so it is unaffected.

(Magic ref: drc/DRCsubcell.c:766-965, drc/DRCsubcell.c:428, drc/DRCmain.c:403-438)

#### Scenario: A why resolution retargets the rest of the square
- **GIVEN** magic-compat ON, step 1000, a parent with a use `a` abutting parent paint at the origin (an interaction area) and a use of `c` at (0,100) and of `d` at (0,−100), `c` and `d` each holding a too-narrow bar at their own origin with an `error_p` tile
- **WHEN** `drc listall why` runs over the parent with the box covering it
- **THEN** it reports `c`'s error at root coordinates and again at `c`'s own coordinates, and not `d`'s error; with magic-compat OFF it reports `c`'s and `d`'s errors once each at root coordinates

#### Scenario: Cell without subcells
- **GIVEN** a cell with only paint, step 1000, and erase box B = [10,20]×[10,20]
- **WHEN** the interaction check runs over the square containing B
- **THEN** exactly one basic check runs, with K=[10−h,20+h]×[10−h,20+h] and L=B, and all its errors are `error_p`

### Requirement: Interaction area
The interaction area of cell C within area Ar for radius h SHALL be computed as follows; it is "none" when C has no subcell use overlapping Ar ⊕ h, when the computed area is empty, when fewer than two distinct sources contain paint in it, or when its intersection with Ar is null.
Compatibility: exact

```
SA = Ar ⊕ h; I = ∅; found = false; list = []
for each subcell use U whose bounding box overlaps SA:
    found = true
    H = (bbox(U) ⊕ h) ∩ SA
    P = bounding box of all non-space paint tiles of C (all paint planes) overlapping H, using full tile extents
    if P does not contain H: list.append((U, H))
    if P non-empty: I = I ∪bbox ((P ⊕ h) ∩ H)
if not found: return none (no subcells)
for each (U, H) in list:
    O = (0,0,0,0) ∪bbox bounding boxes of all other uses overlapping H
    if magic-compat ON or some other use overlaps H:
        I = I ∪bbox ((O ⊕ h) ∩ H)        # ON: O may still be (0,0,0,0)
if I empty: return none
sources = 0
if C has non-space paint overlapping I: sources += 1
for each use (at any depth, through all array elements) whose subtree has non-space, non-DRC paint overlapping I:
    sources += 1 (stop at 2)
if sources < 2: return none
return I ∩ Ar (none if null)
```

`∪bbox` ignores an empty (degenerate) operand. With `magic-compat` ON (the default) O is seeded with the null rectangle (0,0,0,0) and its expansion is included even when no other use overlaps H, so `(-h,-h)-(h,h)` clipped to H is added whenever H covers the origin, as in Magic; with it OFF nothing is added for a use with no other use nearby (Q-MASK-400, resolution: compat). Hierarchical DRC and hierarchical mask generation share this finder and the setting (capability `mask-hierarchical-generation` "Interaction area discovery").

(Magic ref: drc/DRCsubcell.c:249-310, drc/DRCsubcell.c:406-525, drc/DRCsubcell.c:133-145, drc/DRCsubcell.c:470-478, utils/geometry.c:77)

#### Scenario: Subcell far from paint
- **GIVEN** halo 5, a parent with a subcell whose bbox is [0,10]×[0,10] and parent paint only at [100,110]×[0,10], and no other subcells
- **WHEN** the interaction area over [−50,200]×[−50,50] is computed
- **THEN** P is empty for the subcell's halo area and no other use overlaps it
- **AND** with `magic-compat` OFF I is empty; with it ON I = [−5,5]×[−5,5] from the (0,0,0,0) seed, but only the subcell has paint there (one source)
- **AND** in both settings the result is none and the parent is checked with plain basic checks plus copied child errors

### Requirement: Flattening for interaction checks
Flattening a hierarchy into the check buffer over area F SHALL copy the paint of the cell and all its descendants (all array elements, transformed into the cell's coordinates) that overlaps F into an empty internal cell using the DRC paint table, so that illegal overlaps become subcell-error tiles, and SHALL copy the mask-hint properties of all levels into the buffer; a non-Manhattan overlap between cells whose intersection is not representable on the integer grid SHALL be reported as an error with why index 5 over the affected rectangle.
Compatibility: exact

The basic check run on the buffer reports subcell-error tiles with why index 2 (see `drc-rule-semantics`). Mask hints in the buffer are used for exception/exemption tests. See capability `mask-hierarchical-generation` for hint flattening.

(Magic ref: drc/DRCsubcell.c:920-945, drc/DRCsubcell.c:723-730, drc/DRCtech.c:700-737)

#### Scenario: Illegal overlap between cells
- **GIVEN** a tech where poly over ndiff is an illegal hierarchical overlap, parent poly [0,10]×[0,2] and child ndiff [5,15]×[−5,5]
- **WHEN** the interaction area is flattened and checked
- **THEN** the overlap [5,10]×[0,2] is reported as an `error_s` with why "Can't overlap those layers"

### Requirement: Copying subcell errors
In non-interaction parts of a parent, the checker SHALL copy every error tile (`error_p`, `error_s`, `error_ps`) of each direct child use (each array element separately) that overlaps the strip U, clipped to U in the child's coordinates and transformed into the parent, as a parent error with why index 4 ("See error definition in the subcell").
Compatibility: exact

Only one level is copied; deeper errors arrive because each child already holds copies of its own children's errors. Errors in interaction areas are not copied but re-derived by flattening.

(Magic ref: drc/DRCsubcell.c:161-235, drc/DRCsubcell.c:840-855)

#### Scenario: Child error appears in parent
- **GIVEN** a child cell with an `error_p` tile [0,2]×[0,10] placed in a parent at translation (50,50), with nothing else nearby
- **WHEN** the parent area is checked
- **THEN** the parent error plane gets `error_p` [50,52]×[50,60]

### Requirement: Array checks
For every arrayed use (more than one element) overlapping a non-interaction strip U, the checker SHALL check up to four interaction regions between adjacent array elements, each clipped to U, by flattening all elements overlapping the region expanded by h with the DRC paint table, running a basic check with clip equal to the region, and running the exact-overlap check over the elements, reporting errors as `error_p` and exact-overlap errors with why index 1.
Compatibility: exact

With bb the use's bounding box in parent coordinates, (xsize, ysize) the transformed child bounding box size, and (xsep, ysep) the transformed element pitch (for a non-arrayed direction: halo + child width or height):
- If ysep < ysize + h: region A = [bb.xbot, bb.xbot+xsize+h] × [bb.ybot+ysep−h, bb.ybot+ysize+h]; region C = [bb.xtop−h, bb.xtop] × (same y as A).
- If xsep < xsize + h: region B = [bb.xbot+xsep−h, bb.xbot+xsize+h] × [bb.ybot, bb.ybot+ysep−h]; region D = (same x as B) × [bb.ytop−h, bb.ytop].

(Magic ref: drc/DRCarray.c:101-275)

#### Scenario: Abutting array elements
- **GIVEN** `spacing m1 m1 2 touching_ok "s"` (halo 2) and a cell with bounding box [0,10]×[0,10] containing m1 [1,2]×[0,10] and [9,10]×[0,10], used as a 2×1 array with x pitch 10 at the origin
- **WHEN** the parent is checked
- **THEN** only regions B = [8,12]×[0,10] and D = [8,12]×[8,10] are checked (ysep = 2+10 is not < 10+2)
- **AND** the error [11,12]×[0,10] is reported as `error_p`

### Requirement: Exact-overlap check
For every tile of an exact-overlap type in any cell of the hierarchy (the cell itself and descendants) within an interaction or array area I, the checker SHALL, if the flattened buffer contains that type within the tile's rectangle expanded by 1 (clipped to I), report as errors (why index 3, or 1 for arrays) every flattened tile of a different type (other than the type, stacked types containing it, and for contact types their residues) lying under the tile's rectangle, and every flattened tile of the type lying over a neighboring tile (in the tile's own cell) of a different type, clipped to the expanded rectangle and to I.
Compatibility: exact

(Magic ref: drc/DRCsubcell.c:542-720, drc/DRCarray.c:327-345)

#### Scenario: Partial contact overlap
- **GIVEN** `exact_overlap m1c` and two sibling cells each with m1c, at [0,4]×[0,4] and [2,6]×[0,4] in parent coordinates
- **WHEN** the interaction area is checked
- **THEN** errors with why "This layer can't abut or partially overlap between subcells" are reported over the non-matching parts

### Requirement: Forced recheck of an area
Requesting a recheck of area A under root use R (`drc check`) SHALL read in all cells of the hierarchy under A, and for each cell D reached (recursively, through all array elements, with A transformed into D's coordinates) paint `checkpaint` over (A ∩ bbox(D)) expanded by h into D's check plane and enqueue D (children before parents), without upward propagation.
Compatibility: exact

If some cell cannot be read, `Failure to read in entire subtree of cell.` and `Failed on cell <name>.` are reported and nothing is marked. When the clipped area covers the whole cell, remaining elements of the same array are skipped (optimization only).

(Magic ref: drc/DRCmain.c:784-860)

#### Scenario: Recheck whole design
- **GIVEN** a design with cells top ⊃ mid ⊃ leaf and the box covering top's bounding box
- **WHEN** `drc check` is run followed by `drc catchup`
- **THEN** every cell's error plane is regenerated over its whole bounding box plus halo

### Requirement: Flat check
The flat check of a box area A under root use R SHALL divide A into chunks of 300×300 internal units starting at A's lower-left corner (the last chunk in each row/column clipped to A), and for each chunk G flatten the whole hierarchy over G ⊕ h with the DRC paint table, run a basic check with K = G ⊕ h and L = G, and count every reported error; it SHALL print `<n> total errors found.` and modify no error planes.
Compatibility: exact

(Magic ref: drc/DRCsubcell.c:969-1020)

#### Scenario: Flat check count
- **GIVEN** `width poly 2 "w"` and a flat cell containing only poly [0,1]×[0,10]
- **WHEN** `drc *flatcheck` is run with the box covering the cell
- **THEN** the output is `1 total errors found.`

### Requirement: Halo override
The current halo used by marking, step-square processing, interaction and array checks SHALL default to the style halo and MAY be reduced (never increased beyond the style halo) by the user; a reduced halo causes rules with larger distances to be checked only partially.
Compatibility: exact

(Magic ref: commands/CmdCD.c:4413-4446, drc/DRCtech.c:4228-4232)

#### Scenario: Halo capped at style value
- **GIVEN** a style halo of 20
- **WHEN** the user sets the halo to 50
- **THEN** the halo remains 20

### Requirement: Background checker host mechanism
Hefesto SHALL treat the following Magic host mechanism as reference only: Magic runs the background checker from the Tcl idle loop (Tcl builds) or before blocking for terminal input (non-Tcl builds), processes Tcl events between squares, and toggles a `drcstate busy|idle` callback and the prompt character (`]` while busy, `%` otherwise).
Compatibility: informative

Hefesto replaces this mechanism with the worker-thread checker specified in "Pending list and background scheduling" (see Q-DRC-12, resolution: platform): squares are checked on a worker over a snapshot instead of from the idle loop, a square is discarded when an edit lands in it instead of being broken by pending input, `drc catchup` runs synchronously, and `magic::drcstate busy|idle` are emitted at the same transitions.

(Magic ref: drc/DRCcontin.c:481-560)

#### Scenario: Busy indication
- **GIVEN** Magic with pending checks
- **WHEN** the checker starts and finishes
- **THEN** `magic::drcstate busy` and later `magic::drcstate idle` are scheduled
