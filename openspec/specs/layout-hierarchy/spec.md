# Layout Hierarchy

## Purpose

Specifies the hierarchical data model of a Magic-compatible layout: cell definitions and their namespace, cell instances (uses) with instance identifiers, transforms and arrays, bounding boxes and extended bounding boxes, locked instances, expansion/descend rules that control which subcells hierarchical operations see, flattening (copy and in-place), and cell rename/delete semantics, including internal cells.

The `.mag` syntax of `use`, `array`, `transform` and `box` records belongs to capability `cell-file-format`; user commands (`getcell`, `flatten`, `cellname`, `instance`, `expand`, ...) belong to `cmd-hierarchy` and the other `cmd-*` capabilities. Transforms are defined in `layout-geometry-model`; paint copying in `layout-paint-semantics`.

## Requirements

### Requirement: Cell definitions and the cell namespace

The system SHALL keep a single global namespace of cell definitions in which every definition has a unique name, and SHALL create new definitions with the default state described below.
Compatibility: behavioral

- Creating a definition whose name already exists fails (returns no new cell). A missing name defaults to `(UNNAMED)`.
- A trailing `.mag` extension is stripped from the new definition's name.
- A new definition is empty: all paint planes are space, no labels, no subcells, no properties, timestamp 0, bounding box and extended bounding box `(0,0)-(1,1)`, and no flags set.
- A definition carries state flags; the observable ones are: available (contents loaded), modified, not-found (load failed once), internal, needs-new-timestamp, child-timestamps-changed, child-bboxes-changed, fixed-bbox (has `FIXED_BBOX`), no-edit (read-only/locked file), vendor-GDS (has `GDS_FILE`), dereference (search path instead of stored path), fixed-timestamp. Their persistence and timestamp semantics are specified in the `cell-*` capabilities.
- The definition records the set of tile types it contains (used to skip work); this is an optimization and MAY be omitted.

(Magic ref: database/DBcellname.c:1921-2055, database/database.h.in:338-454)

#### Scenario: .mag suffix is stripped
- **GIVEN** no cell named `inv`
- **WHEN** a definition named `inv.mag` is created
- **THEN** the definition SHALL be named `inv`

#### Scenario: New cell bounding box
- **GIVEN** a newly created definition `foo`
- **WHEN** its bounding box is queried
- **THEN** it SHALL be `(0,0)-(1,1)`

### Requirement: Cell instances

A cell instance (use) SHALL reference one child definition and record its parent definition, an instance identifier unique within the parent, a Manhattan transform from child to parent coordinates, array parameters, a locked flag and a per-window expansion state.
Compatibility: behavioral

- A new use has identity transform, array indices `xlo=xhi=ylo=yhi=0`, separations 0, not locked, unexpanded everywhere.
- A definition may be instantiated any number of times, in any number of parents, but never (directly or indirectly) inside itself (recursion is rejected by the placing commands; see `cmd-hierarchy`).
- Placing a use marks the parent modified and needing a new timestamp; removing a use does the same.
- Two uses of the same child with identical transform and identical array dimensions/separations at the same bounding box are duplicates; copy operations detect and skip such duplicates (see `cmd-*`).

(Magic ref: database/database.h.in:456-537, database/DBcellname.c:2066-2104, database/DBcell.c:69-170,187-298)

#### Scenario: Placing marks the parent modified
- **GIVEN** an unmodified parent `top`
- **WHEN** an instance of `inv` is placed in `top`
- **THEN** `top` SHALL be marked modified

### Requirement: Instance identifier assignment

When a use is linked into a parent without an identifier, the system SHALL assign `<base>_<n>` where `<base>` is the child's name after its last `/` and `<n>` is the smallest non-negative integer such that no existing use in the parent has exactly that identifier; an explicitly supplied identifier SHALL be rejected if another use in the parent already has it.
Compatibility: exact

- Only existing identifiers of the form `<base>_<something>` are considered taken; the search starts at 0 every time.
- When a cell is read and its uses are made unique (duplicates or missing identifiers), the first use with a given identifier keeps it; each later duplicate loses its identifier; then every use without an identifier receives `<defname>_<k>` where `<defname>` is the child's full name (including any `/`, see Q-MODEL-8; with `magic-compat` off, it SHALL be the part after the last `/`) and `<k>` starts at 0 per child definition and increases, skipping identifiers already present.
- Identifier lookup ignores a trailing array subscript: `foo_0[1,2]` and `foo_0[3]` find use `foo_0`; if that fails the full string is tried.
- Renaming a use to its own name is a no-op; renaming to an identifier already used in the parent fails; a successful rename is undoable and marks the parent modified (but does not request a new timestamp).

(Magic ref: database/DBcellname.c:2401-2460,2465-2500,2511-2545,2554-2584,2605-2795)

#### Scenario: Sequential default identifiers
- **GIVEN** parent `top` with no uses
- **WHEN** three instances of `inv` are placed without identifiers
- **THEN** their identifiers SHALL be `inv_0`, `inv_1`, `inv_2`

#### Scenario: Gap reuse
- **GIVEN** parent `top` with uses `inv_0`, `inv_2`
- **WHEN** another instance of `inv` is placed without an identifier
- **THEN** its identifier SHALL be `inv_1`

#### Scenario: Library path stripped from base
- **GIVEN** a child definition named `lib/nand2`
- **WHEN** it is placed without an identifier in an empty parent
- **THEN** the identifier SHALL be `nand2_0`

### Requirement: Arrays

An arrayed use SHALL represent the grid of elements `x` from `xlo` to `xhi` and `y` from `ylo` to `yhi` (inclusive, either direction), where element `(x,y)` is the child translated by `(|x - xlo| * xsep', |y - ylo| * ysep')` in child coordinates and then mapped by the use transform; arraying is applied before the transform.
Compatibility: exact

- The element offset is `dx = xsep * (x - xlo)` when `xlo <= xhi` and `dx = xsep * (xlo - x)` when `xlo > xhi` (same for y). Separations are in child coordinates and may be negative or zero.
- Element transform: `T_elem(p) = T_use(p + (dx, dy))`.
- Creating an array from root-coordinate parameters `(xlo,ylo,xhi,yhi,xsep,ysep)` through the root-to-child rotation matrix `M`: `xsep_child = M.a*xsep + M.b*ysep`, `ysep_child = M.d*xsep + M.e*ysep`; if `M.a == 0` (90/270-degree orientations) the x and y index ranges are swapped. This keeps indices running left-to-right and bottom-to-top as seen in the root.
- Element enumeration order for hierarchical searches: rows `y` ascending over the numeric index range, and within a row `x` ascending. Only elements that can overlap the search area `S` (mapped into child coordinates as `S'`) are visited: with indices canonicalized so that `lo <= hi`, and coordinates of `S'` and the child box `B` negated on an axis whose separation is negative (making `sep > 0`), `first = lo + (S'.xbot - B.xtop + sep - 1) / sep` and `last = lo + (S'.xtop - B.xbot) / sep` using C truncating division, then clipped to `[lo, hi]` and mapped back to the original index direction. A separation of 0 visits all elements.

(Magic ref: database/DBcellbox.c:289-303,363-387,412-525,750-775,800-818, database/DBcellsrch.c:1180-1258)

#### Scenario: Element placement
- **GIVEN** child bbox `(0,0)-(10,10)`, use transform identity translated by `(100,0)`, array `x 0..3 sep 20`, `y 0..1 sep 15`
- **WHEN** element `(2,1)` is located
- **THEN** its child-origin SHALL be at parent `(140,15)`

#### Scenario: Reverse-indexed array
- **GIVEN** an array with `xlo=3, xhi=0, xsep=20`, identity transform
- **WHEN** element `x=3` and element `x=0` are located
- **THEN** element 3 SHALL be at offset 0 and element 0 at offset 60

### Requirement: Array element naming

The printed name of an array element SHALL be the instance identifier followed by `[y,x]` for a two-dimensional array, `[x]` when only x varies, `[y]` when only y varies, and nothing for a non-arrayed use; parsing of hierarchical names SHALL accept `[y,x]`, `[y][x]` and a single `[i]`.
Compatibility: exact

- The y index comes first in two-dimensional names.
- A name without subscript matches an array only when it equals the identifier exactly, and then denotes element `[0][0]` or `[0]`.
- Subscripts out of range, or a subscript count different from the array dimensionality, do not match.
- Hierarchical paths separate levels with `/` (e.g. `top_0/inv_1[2]/a`).
- Locked uses are displayed with a leading `*` (display only; not part of the identifier).

(Magic ref: database/DBcellbox.c:62-110, database/DBlabel2.c:276-480)

#### Scenario: Two-dimensional element name
- **GIVEN** use `inv_0` arrayed `x 0..3`, `y 0..1`
- **WHEN** element `x=2, y=1` is named
- **THEN** the name SHALL be `inv_0[1,2]`

#### Scenario: One-dimensional element name
- **GIVEN** use `inv_0` arrayed `x 0..3`, `y 0..0`
- **WHEN** element `x=2` is named
- **THEN** the name SHALL be `inv_0[2]`

### Requirement: Bounding box and extended bounding box

The bounding box of a definition SHALL be the union of all paint on planes 1 and above except the DRC check plane, the bounding boxes of all subcell uses, and the rectangles of all labels; the extended bounding box SHALL additionally include subcell extended boxes and the rendered text box of every outline-font label; degenerate results SHALL be normalized as described.
Compatibility: exact

- Paint includes DRC error tiles and router hint planes (see Q-MODEL-19). With `magic-compat` OFF, only the paint planes SHALL count.
- Label rectangles (even zero-size points and lines) contribute their corner points.
- If nothing at all is present, both boxes are `(0,0)-(1,1)`.
- If a box has zero width, `xtop = xbot + 1`; zero height, `ytop = ybot + 1` (both boxes independently).
- The extended box always contains the bounding box.
- The bounding box of a use is the child bounding box extended by the array span (`|xsep*(xhi-xlo)|` towards the sign of `xsep`, same for y) and then transformed; the extended box of a use is computed the same way from the child's extended box.
- The `FIXED_BBOX` property does NOT change the computed bounding box (it is consulted by LEF/DEF/GDS writers; see `layout-properties`).
- When a definition's box changes, every use of it is re-positioned, every parent is flagged "child boxes changed" (not modified), and the computation propagates upward. Hefesto MAY compute boxes lazily but MUST return identical values.
- Magic recomputes the parents in the order of the definition's uses, most recent first, and recomputes the last pending parent only when the oldest use has a parent. Loading a definition into a window gives it a parentless root use that is never freed, so when the definition was loaded in a window before every placement that still uses it, the last pending parent keeps its old bounding box (its use of the definition is still re-positioned) until something else recomputes that parent; the stale box bounds the parent's DRC check areas, its `.mag` timestamp processing and the boxes of its own parents. This SHALL be replicated with `magic-compat` ON; with it OFF every parent SHALL be recomputed (see Q-MODEL-32, resolution: compat).

(Magic ref: database/DBcellbox.c:244-280,553-775, database/DBbound.c:56-170, database/DBlabel.c:1352-1500, database/DBcellname.c:2066-2097, dbwind/DBWprocs.c:545-547)

#### Scenario: Paint plus point label
- **GIVEN** a cell with `metal1 (0,0)-(10,10)` and a point label at `(20,20)`
- **WHEN** the bounding box is computed
- **THEN** it SHALL be `(0,0)-(20,20)`

#### Scenario: Label only
- **GIVEN** a cell whose only content is a point label at `(5,5)`
- **WHEN** the bounding box is computed
- **THEN** it SHALL be `(5,5)-(6,6)`

#### Scenario: Arrayed use box
- **GIVEN** child bbox `(0,0)-(10,10)` and a use with identity transform, `x 0..3 sep 20`, `y 0..1 sep 15`
- **WHEN** the use bounding box is computed
- **THEN** it SHALL be `(0,0)-(70,25)`
- **AND** with `xsep = -20` it SHALL be `(-60,0)-(10,25)`

#### Scenario: Parent of a child loaded in a window before its placement
- **GIVEN** a cell `child` whose only content is a label `(0,0)-(10,10)`, loaded in a window, and then a cell `top` with one use of `child` (so `top` has the box `(0,0)-(10,10)`)
- **WHEN** a label `(0,0)-(20,10)` is added to `child`
- **THEN** the use's box SHALL be `(0,0)-(20,10)` in either `magic-compat` setting
- **AND** with `magic-compat` ON the box of `top` SHALL stay `(0,0)-(10,10)`; with it OFF it SHALL be `(0,0)-(20,10)`
- **AND** had `top` placed `child` before the window load, its box SHALL be `(0,0)-(20,10)` in either setting

### Requirement: Locked instances

A use SHALL carry a locked flag that is persisted with the parent and that prevents interactive modification of that instance.
Compatibility: behavioral

- Locked uses cannot be moved, copied, rotated, flipped, deleted or edited through selection-based commands (enforced by the `cmd-*` capabilities, chiefly `cmd-selection`); other paint in the parent is unaffected.
- Locking/unlocking is undoable (recorded as before/after state) but in Magic does NOT mark the parent modified. Hefesto SHALL mark the parent modified (see Q-MODEL-13).
- A descend mode exists that skips locked uses (see requirement "Descend rules").
- In `.mag` files the lock is encoded by a `*` before the instance identifier (see capability `cell-file-format`).

(Magic ref: database/database.h.in:495-517, database/DBcellname.c:1260-1312, database/DBio.c:2428,3021,6247)

#### Scenario: Locking does not dirty the parent (verify against Magic)
- **GIVEN** an unmodified parent with use `inv_0`
- **WHEN** `inv_0` is locked
- **THEN** the parent SHALL NOT be marked modified
- **AND** an undo SHALL unlock it

### Requirement: Descend rules

Hierarchical operations SHALL decide whether to descend into a use according to a descend mode, which is either a window expansion mask or one of the special modes below.
Compatibility: behavioral

| Mode | Descend into a use when |
|---|---|
| window mask (single bit) | the use is expanded in that window |
| all | always |
| special | the use's expansion state equals the "special" marker (used for internal yank buffers) |
| no-subcircuit | the child (loaded on demand) has no port labels |
| no-vendor | the child is not a vendor-GDS (read-only) cell |
| no-lock | the use is not locked and is marked "special" |
| property-flatten | the use is "special", or its child has a property named `flatten` |
| none | never |

- Expanding a use loads its child if needed and sets the window bit; unexpanding clears it; toggling inverts.
- Searches pass to each level the composed transform `T_root ∘ T_elem` and the search area mapped back into child coordinates (see `layout-geometry-model`).

(Magic ref: database/DBcellsubr.c:61-108, database/DBexpand.c:71-130, database/DBcellsrch.c:1131-1258)

#### Scenario: No-subcircuit mode
- **GIVEN** child `a` with a port label and child `b` without ports, both placed in `top`
- **WHEN** a no-subcircuit flatten of `top` is performed
- **THEN** the paint of `b` SHALL be copied and `a` SHALL be kept as an instance

### Requirement: Flattening into a new cell

Flattening a cell into a new destination definition SHALL copy all paint (all types except space and DRC types) of every descended level, transformed into root coordinates, plus mask-hint properties, plus labels according to the label option, plus (when the descend mode is not "all") the non-descended uses as instances, and SHALL NOT be undoable.
Compatibility: behavioral

- Paint is merged per plane through the paint tables (`layout-paint-semantics`, "Copying paint between cells").
- Labels (default): every label of every level is copied with its rectangle and justification transformed; text becomes the hierarchical path with the last `/` replaced by `.` (e.g. a label `A` inside `x_0/y_1` becomes `x_0/y_1.A`); only labels of the top level keep their port flags and index; font, size, rotation and offset are copied untransformed (see Q-MODEL-20). With `magic-compat` OFF, rotation and offset SHALL be transformed as for hierarchical copies.
- "Top labels only" copies only the labels of the root level. "No labels" copies none.
- Properties other than `MASKHINTS_*` are not copied. Which instances' mask hints are copied is specified by capability `mask-hints`, "Copying, flattening and transforming hints" (Q-MISC-15).
- The destination must not already exist unless a box-limited flatten is requested.
- The flattened cell itself is the root level. With `magic-compat` ON, as in Magic, a restricting descend mode (`-doproperty`, `-nosubcircuits`, `-novendor`) also applies its test to the root level: when the flattened cell has no `flatten` property, is a subcircuit (has ports) or is a vendor-GDS cell respectively, no paint, mask hints, path-named labels or kept instances are copied and the new cell is empty, except that "top labels only" still copies the root level's labels. With `magic-compat` OFF the root level is always copied and the mode applies only to the uses below it (see Q-MODEL-33, resolution: compat). `xor`, which flattens its source the same way, follows the same rule.

(Magic ref: commands/CmdFI.c:1933-2010,2040-2250, database/DBcellcopy.c:284-305,551-560,1359,1435,1730-1790, database/DBcellsrch.c:245,586,845, database/DBcellsubr.c:61-108)

#### Scenario: Hierarchical label naming
- **GIVEN** `top` containing use `x_0` of `mid`, which contains use `y_1` of `leaf` with label `A` at `(1,1)`
- **WHEN** `top` is flattened into `flat`
- **THEN** `flat` SHALL contain a label with text `x_0/y_1.A`
- **AND** that label SHALL NOT be a port

#### Scenario: Restricting flatten of a root level that fails its own test
- **GIVEN** `top` with a port label `P`, metal1 paint, no `flatten` property and an unported child use `c_0`
- **WHEN** `top` is flattened into `flat` with `-nosubcircuits`, or with `-doproperty`
- **THEN** with `magic-compat` ON `flat` SHALL be empty (no paint, labels or uses) (golden MODEL-G-060)
- **AND** with `-dotoplabels` added, `flat` SHALL contain only the label `P`
- **AND** with `magic-compat` OFF `flat` SHALL contain `top`'s paint and label `P`, and `c_0` flattened (`-nosubcircuits`) or kept as an instance (`-doproperty`)

### Requirement: Flattening an instance in place

Flattening a use in place SHALL, for each array element in order (x varying fastest from `xlo` toward `xhi`, then y from `ylo` toward `yhi`), copy the element's paint, labels (per option), mask hints and (for non-"all" modes) non-descended sub-instances into the parent, rename the child's top-level labels with the element prefix, and finally remove the use; the operation SHALL NOT be undoable.
Compatibility: behavioral

- Top-level child labels are re-created in the parent as non-port labels with text `<id>[x][y]/<text>` (2-D array), `<id>[x]/<text>` or `<id>[y]/<text>` (1-D), or `<id>/<text>`, and the unprefixed copies are removed. Note the `[x][y]` order differs from element naming (`[y,x]`); see Q-MODEL-7. With `magic-compat` OFF, the prefix SHALL be `<id>[y][x]/<text>`.
- Magic sizes the renamed text without room for the index suffix (a heap overflow) and, in the captured run, leaves the labels unprefixed. Hefesto SHALL apply the names above (see Q-MODEL-31).
- Flattening every selected use applies the same procedure to each and deletes them from the selection.
- The mask hints copied are those of the element and of every descended level below it (capability `mask-hints`, "Copying, flattening and transforming hints"; Q-MISC-14).

(Magic ref: database/DBcellcopy.c:580-735,781-800, commands/CmdFI.c:2105-2140)

#### Scenario: In-place flatten of an arrayed use (verify against Magic)
- **GIVEN** use `inv_0` arrayed `x 0..1`, `y 0..1` whose child has top-level label `out`
- **WHEN** `inv_0` is flattened in place with labels
- **THEN** the parent SHALL contain labels `inv_0[0][0]/out`, `inv_0[1][0]/out`, `inv_0[0][1]/out`, `inv_0[1][1]/out`
- **AND** `inv_0` SHALL no longer exist

### Requirement: Renaming a cell definition

Renaming a definition SHALL fail for internal cells, for a target name that already exists, and for vendor-GDS (read-only) cells unless forced; a forced rename of a vendor cell SHALL remove its `GDS_FILE`, `GDS_START` and `GDS_END` properties; a successful rename SHALL mark every parent of its uses modified and SHALL NOT be undoable.
Compatibility: behavioral

- In Magic, the renamed definition itself is not flagged modified by the rename. Hefesto SHALL mark it modified (see Q-MODEL-15).
- All uses keep their identifiers and keep referring to the definition.

(Magic ref: database/DBcellname.c:95-175,2122-2155)

#### Scenario: Rename refused for existing name
- **GIVEN** definitions `a` and `b`
- **WHEN** `a` is renamed to `b`
- **THEN** the rename SHALL fail and both definitions SHALL be unchanged

### Requirement: Deleting a cell definition

Deleting a definition SHALL fail if it is internal or if any use of it lies in a non-internal parent; otherwise it SHALL remove its uses from internal cells, ask for confirmation if it is modified (unless forced), clear the undo history if it is the cell being recorded, replace it in any window showing it as root by an empty `(UNNAMED)` cell, and free the definition and its properties.
Compatibility: behavioral

- Deleting `(UNNAMED)` first renames it so that a fresh `(UNNAMED)` can be created.
- Deletion is not undoable.

(Magic ref: database/DBcellname.c:228-360,2168-2200)

#### Scenario: Delete refused while instantiated
- **GIVEN** `inv` placed in `top`
- **WHEN** deletion of `inv` is requested
- **THEN** it SHALL fail reporting the dependent instance identifier

### Requirement: Internal cells

The system SHALL support internal definitions (selection buffers, yank buffers, DRC/extraction/CIF scratch cells) that are excluded from user-visible cell lists, saving, DRC, extraction, rename and delete.
Compatibility: informative

Magic's internal cells are flagged internal and conventionally named with a `__` prefix (`__SELECT__`, `__SELECT2__`, `__CIF__`, `__CIF2__`, `__RESIS__`, `__SHOW__`, `__CHANNEL__`, `__lefFlat__`, `__lefYank__`, `__PLOWYANK__`, `__mz_result`, ...). Hefesto MAY implement these buffers without cell definitions; it MUST NOT let user cell names collide with its internal buffers.

(Magic ref: database/DBcellname.c:2853-2866, select/selCreate.c:120-150, database/database.h.in:53)

#### Scenario: Internal cell rename refused
- **GIVEN** the selection buffer cell
- **WHEN** a rename of it is attempted
- **THEN** the rename SHALL fail
