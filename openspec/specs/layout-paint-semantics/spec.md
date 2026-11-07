# Layout Paint Semantics

## Purpose

Specifies, as observable semantics, what geometry results when paint is added to, removed from, or overwritten in a cell: the per-plane result tables (paint, erase, write), the default rules that make contacts appear on all of their planes and dissolve into residues, stacked contacts, the cross-plane image resolution step, non-Manhattan (triangle) painting including fracturing, rounding and re-merging, copying paint between cells, active/locked layers, and which operations mark a cell as modified.

The syntax of the technology `compose`, `contact` and `stackable` statements belongs to the `tech-*` capabilities; user commands (`paint`, `erase`, `splitpaint`, ...) belong to `cmd-paint-erase`. Geometry, planes and canonical decomposition are defined in `layout-geometry-model`.

## Requirements

### Requirement: Result-table model of painting

Every paint, erase and write operation SHALL be defined per plane by a result table `R[plane][operand][old]` giving, for each existing type `old` at a point of that plane, the type that point holds after applying `operand`; the operation over a rectangle SHALL apply the table independently at every point of the rectangle on every affected plane.
Compatibility: exact

- Three table families exist: paint `P[p][t][old]`, erase `E[p][t][old]` (both per plane) and write `W[t][old] = t` (plane independent; replaces anything).
- Initial (no-op) state: every entry returns `old`.
- Base defaults, for every type `s` with home plane `ps` and every type `t` with a home plane: `P[ps][t][s] = t` if `t` has home plane `ps`, else `s`; `E[ps][t][s] = s`; `E[ps][s][s] = space`; `P[ps][s][space] = s`.
- Built-in DRC types: on the DRC error plane the error types behave as a 2-bit set (`error_p`=1, `error_s`=2, `error_ps`=3: painting ORs, erasing clears bits); on the DRC check plane `checkpaint + checksubcell = checkpaint` in either order.
- The "affected planes" of an operand `t` are the planes `p >= 1` for which some entry `R[p][t][s] != s`. Painting `t` touches only those planes. Space's affected planes are all planes except plane 0.
- Technology-supplied rules override defaults; the precedence is: explicit `paint`/`erase`/`compose`/`decompose` rules for non-contacts (set while reading the technology) > the contact defaults of requirement "Default contact painting rules" (applied only to entries still at default) > base defaults. Saved compose/decompose rules whose result is a contact are applied last and unconditionally (see requirement "Technology compose rules at runtime").
- Hefesto MAY compute results lazily instead of tabulating them, but the results MUST be identical.

(Magic ref: database/DBtpaint.c:37-44,88-247,281-303, database/DBtpaint2.c:88-170, database/database.h.in:1255-1322, database/databaseInt.h:230-250)

#### Scenario: Painting over another type on the same plane replaces it
- **GIVEN** a technology where `ndiff` and `pdiff` share the active plane and no compose rule relates them
- **AND** `ndiff` painted at `(0,0)-(10,10)`
- **WHEN** `pdiff` is painted at `(5,0)-(15,10)`
- **THEN** the active plane SHALL hold `ndiff (0,0)-(5,10)` and `pdiff (5,0)-(15,10)`

#### Scenario: Erasing a different type is a no-op
- **GIVEN** `metal1` painted at `(0,0)-(10,10)`
- **WHEN** `metal2` is erased over `(0,0)-(10,10)`
- **THEN** the metal1 plane SHALL still hold `metal1 (0,0)-(10,10)`

### Requirement: Paint operation

Painting a type `t` (or one side of a triangle of type `t`) into a cell over a rectangle SHALL apply the paint table of `t` on every affected plane, re-merge split tiles, resolve contact images, and mark the cell modified, in exactly this order.
Compatibility: exact

Algorithm for `paint(cell, rect, t)`:
1. Mark the cell modified and needing a new timestamp (even if no geometry changes).
2. For each plane `p` from 1 to the last plane, if `t` affects `p`: apply `P[p][t]` over `rect` (or over the triangle, see requirement "Non-Manhattan painting"), then perform split-tile merging over `rect` expanded by 1 unit on each side.
3. Image resolution (only if `t` is a user type, not a generated stacked type): for every user type `c` from type 6 upward (excluding `t` itself) whose residue set contains `t`, find every area inside `rect` (or the triangle) where `c` is present on any plane of `c`, and repaint `c` over exactly that area with this same algorithm (recursively). This guarantees a contact created on one plane by a compose rule appears on all of its planes.

`erase(cell, rect, t)`:
1. Mark the cell modified and needing a new timestamp.
2. If `t` is space: apply the paint table of space `P[p][space]` on every plane >= 1 (Magic intends "erase everything", but with default tables every `P[p][space][s]` is `s`, so this is a no-op; see Q-MODEL-29; with `magic-compat` off, erasing space SHALL erase every type in the area). Otherwise apply `E[p][t]` on every plane `t` affects when erased.
3. Merge split tiles over `rect` expanded by 1, except when `rect` equals the whole-plane rectangle (global erase skips merging).
4. No image resolution is performed after erase.

Multi-type variants:
- paint-mask paints each type of the mask in increasing type order.
- erase-mask erases each type in decreasing type order, from the highest type down to `checkpaint` (type 1).

(Magic ref: database/DBpaint2.c:48-136,155-210,227-238,333-348)

#### Scenario: Painting a type that is already present still marks the cell modified
- **GIVEN** an unmodified cell containing `metal1 (0,0)-(10,10)`
- **WHEN** `metal1` is painted at `(0,0)-(10,10)`
- **THEN** the geometry SHALL be unchanged
- **AND** the cell SHALL be marked modified

#### Scenario: Erase-mask order
- **GIVEN** `metal1 (0,0)-(10,10)` and `poly (0,0)-(10,10)` and a mask `{poly, metal1}`
- **WHEN** the mask is erased over `(0,0)-(5,10)`
- **THEN** the higher-numbered type SHALL be erased first, and both planes SHALL hold only `(5,0)-(10,10)` of their type

### Requirement: Default contact painting rules

For every contact type the system SHALL derive the default paint and erase results so that painting a contact puts it on all of its planes, painting compatible material over a contact leaves it intact, and painting or erasing material that changes a residue dissolves the contact into its residues.
Compatibility: exact

Notation: `R(c)` residues of contact `c`; `Pl(c)` planes of `c`; `res(c,q)` the residue of `c` on plane `q`; "set" means "set only if the entry is still default and the existing type lives on that plane".

1. Paint contact over anything: for each primary contact `t` and each plane `q` in `Pl(t)`: `P[q][t][s] = t` for every type `s` on `q`, and `P[q][t][space] = t`.
2. Residue transparency: for each contact `c`, residue `r` on plane `q`, and user type `s`: if painting `s` over `r` leaves `r` unchanged on `q` (i.e. `P[plane(r)][s][r] == r`), set `P[q][s][c] = c`.
3. Painting user type `T` over contact image `I != T`, `S = Pl(I) ∩ Pl(T)`:
   - If `S` is empty and `T` is a contact: if some user contact `C` has `R(C) == R(I) ∪ R(T)`, set `P[q][T][I] = C` on every plane of `C`; otherwise defaults apply.
   - If `S` is non-empty and `res(I,q) == res(T,q)` for all `q` in `S` (compatible):
     - let `N` = the stacked type generated for the pair `(I,T)` if one exists; let `C` = a user contact with `R(C) == R(I) ∪ R(T)`.
     - if no `C` exists or `N` exists: if `N` exists, set `P[q][T][I] = N` for `q` in `S`; else if `T` is a contact and `I` a user contact, set on the plane of each residue `r` of `I`: `T` if `r` in `R(T)`, else `r`; else if `T` is a contact not among `R(I)`, set `T` on planes of residues of `I` that are in `R(T)`; otherwise (T is a compatible non-contact or a component of stacked `I`) set `P[home(I)][T][I] = I`.
     - else set `P[q][T][I] = C` for every plane of `C`.
   - If incompatible on a shared plane `q0`: on the plane of every residue `r` of `I` other than `res(I,q0)` set `P[plane(r)][T][I] = r` (the contact dissolves; on `q0` the default applies).
4. Erasing `E` from contact image `I`:
   - default `E[q][E][I] = space` for every `q` in `Pl(E)` (for a stacked `I` only on its home plane). Erasing `I` from itself yields space on all of its planes.
   - if `Pl(I) ∩ Pl(E)` is empty, nothing else changes.
   - otherwise let `K` = the user contacts whose residues are a subset of the (fully decomposed) residues of `I` but not a superset of `R(E)`. If the residue sets of `K` overlap each other, set `E[q][E][I] = I` on all planes of `I` (erasing has no effect). Otherwise set `E[q][E][I] = C` on every plane of each `C` in `K` (home plane only for stacked `I`), and on every remaining plane of `I` not in `Pl(E)` and not covered by `K` set `res(I,q)`.
5. Stacked operands: painting/erasing a stacked type `N` over any type is the sequential application of each of its residue contacts' entries, residue order ascending.
6. Locked contacts: a contact (or stacked type) that is not an active layer is made un-erasable through its residues: `E[q][n][c] = c` for every `n != c` and `q` in `Pl(c)` (for stacked types, active residues remain erasable).

(Magic ref: database/DBtpaint2.c:197-216,255-275,298-336,343-522,547-655,665-735)

#### Scenario: Painting a via puts it on both metal planes
- **GIVEN** contact `via` with residues `metal1`, `metal2` and `metal1` already painted at `(0,0)-(20,10)`
- **WHEN** `via` is painted at `(0,0)-(10,10)`
- **THEN** the metal1 plane SHALL hold `via (0,0)-(10,10)` and `metal1 (10,0)-(20,10)`
- **AND** the metal2 plane SHALL hold `via (0,0)-(10,10)`

#### Scenario: Painting a residue over a contact is harmless
- **GIVEN** `via (0,0)-(10,10)`
- **WHEN** `metal1` is painted at `(0,0)-(20,10)`
- **THEN** the metal1 plane SHALL hold `via (0,0)-(10,10)` and `metal1 (10,0)-(20,10)`
- **AND** the `.mag` output SHALL list `rect 10 0 20 10` under `<< metal1 >>` and `rect 0 0 10 10` under `<< via >>`

#### Scenario: Erasing a residue dissolves the contact
- **GIVEN** `via (0,0)-(10,10)`
- **WHEN** `metal1` is erased over `(0,0)-(10,10)`
- **THEN** the metal1 plane SHALL be empty there
- **AND** the metal2 plane SHALL hold `metal2 (0,0)-(10,10)`

#### Scenario: Incompatible residue breaks a diffusion contact
- **GIVEN** contact `pdc` with residues `pdiff` (active plane, home) and `metal1`, no rule composing `ndiff` with `pdiff`, and `pdc (0,0)-(4,4)`
- **WHEN** `ndiff` is painted at `(0,0)-(4,4)`
- **THEN** the active plane SHALL hold `ndiff (0,0)-(4,4)`
- **AND** the metal1 plane SHALL hold `metal1 (0,0)-(4,4)`

### Requirement: Stacked contacts

When two contact types declared stackable share a residue plane and no user contact covers their combined residues, painting one over the other SHALL produce the generated stacked type on the shared plane while each contact remains on its own other planes, and every file writer SHALL decompose stacked types back into their components.
Compatibility: exact

- A generated stacked type is named `<shortname1>+<shortname2>` (generation rules and limits: see `tech-*`). Its residues are the two contact types; its home plane is the plane of the first shared residue.
- Stacked types never appear in files: the `.mag` writer emits, for each user type `u`, the tiles of `u` and of every stacked type having `u` as a residue, on `u`'s home plane, under `<< u >>`. On reading, a stacked type name is painted as each of its residues.
- Stacked types are never chosen as a label's layer (see `layout-labels-ports`).
- `erase-valid` style erasing ignores stacked types in its mask (see Q-MODEL-22), while `paint-valid` decomposes them into residues. With `magic-compat` OFF, `erase-valid` SHALL decompose stacked types into residues too.

(Magic ref: database/DBtpaint2.c:404-425,298-336, database/DBtcontact.c:357-412,694-723, database/DBtechtype.c:506-528, database/DBio.c:4751-4774,900-912, database/DBpaint2.c:255-316,365-417)

#### Scenario: via2 over via1
- **GIVEN** stackable contacts `via1 (metal1,metal2)` and `via2 (metal2,metal3)` and `via1 (0,0)-(4,4)`
- **WHEN** `via2` is painted at `(0,0)-(4,4)`
- **THEN** metal1 SHALL hold `via1`, metal2 SHALL hold `via1+via2`, metal3 SHALL hold `via2` over `(0,0)-(4,4)`
- **AND** the `.mag` output SHALL contain `rect 0 0 4 4` under both `<< via1 >>` and `<< via2 >>` and SHALL NOT mention `via1+via2`

#### Scenario: Erasing one component of a stack
- **GIVEN** the stacked result of the previous scenario
- **WHEN** `via2` is erased over `(0,0)-(4,4)`
- **THEN** metal1 and metal2 SHALL hold `via1` and metal3 SHALL be empty

### Requirement: Technology compose rules at runtime

The runtime effect of technology composition rules SHALL be: for `compose T a b`, painting `a` over `b` or `b` over `a` yields `T`; for both `compose` and `decompose T a b`, painting `a` or `b` over `T` leaves `T`, erasing `a` from `T` leaves `b` and erasing `b` from `T` leaves `a`; explicit `paint`/`erase` rules set single entries.
Compatibility: exact

- For non-contact `T` the entries are on the plane of `a`.
- For contact `T` (saved rules applied after all defaults, overriding them): painting a component over `T` on `T`'s home plane is a no-op; erasing a component from `T` on the home plane yields `T`'s residue on that plane; `compose` additionally sets painting `b` over `a` (and `a` over `b`) to `T` on `T`'s home plane when the existing type lives there.
- Explicit `paint have t result [plane]` sets `P[q][t][have] = result` on every plane of `result`, and `space` on the remaining planes of the rule's plane mask; `erase` likewise. Several result types are allowed (one per plane).
- Syntax and error handling: see `tech-*`.

(Magic ref: database/DBtpaint.c:351-442,593-692, database/DBtpaint2.c:775-905)

#### Scenario: Compose rule for a non-contact
- **GIVEN** `compose nfet poly ndiff` with `poly` and `ndiff` on the active plane
- **AND** `ndiff (0,0)-(10,10)`
- **WHEN** `poly` is painted at `(4,-2)-(6,12)`
- **THEN** the active plane SHALL hold `nfet (4,0)-(6,10)`, `poly (4,-2)-(6,0)` and `poly (4,10)-(6,12)`, and `ndiff` elsewhere in `(0,0)-(10,10)`
- **AND** WHEN `poly` is then erased over `(4,0)-(6,10)` the area SHALL become `ndiff`

### Requirement: Non-Manhattan painting

Painting or erasing a triangle SHALL be applied by first fracturing existing split tiles that cross the triangle's bounding rectangle, then subdividing the bounding rectangle along existing tile boundaries into rectangles (painted as rectangles) and smaller triangles on the same diagonal (painted as triangles), using the rounding rule below, and finally re-merging split tiles.
Compatibility: exact

Let the triangle have bounding rectangle `A` (width `W`, height `H`), direction `dir` (0 = `/`, 1 = `\`) and side (0 = left, 1 = right).
1. Fracture: existing split tiles that cross the boundary of `A` and whose types interact with the operand (`R[old] != old` for either half) are split at the boundary, preserving geometry.
2. If exactly one tile lies under `A`: combine quadrant-wise. The old tile's types for N, S, E, W quadrants are taken from its halves (or its single type); the operand changes W (side 0) or E (side 1), and N if `dir == side` else S. If N==E and S==W the result is a split tile (`\` if N!=W) or a rectangle (if all equal); if N==W and S==E it is a `/` split tile; if the result equals the old type nothing happens. Otherwise (a triangle over a triangle with the opposite diagonal) `A` is quartered: with `xm = A.xtop - floor(W/2)` and `ym = A.ytop - floor(H/2)` the quarters are `(A.xbot,A.ybot)-(xm,ym)`, `(A.xbot,ym)-(xm,A.ytop)`, `(xm,A.ybot)-(A.xtop,ym)` and `(xm,ym)-(A.xtop,A.ytop)`, and each quarter is processed by step 3 against the original diagonal of `A`. If `W == 1` or `H == 1` no quartering is possible and the whole of `A` is painted as a rectangle instead (see Q-MODEL-4).
3. Otherwise, for each tile rectangle `r` under `A` (clipped to `A`), compute where the diagonal of `A` crosses `r`:
   - for `dir == 0`: `x(y) = A.xbot + round_half_up(W*(y - A.ybot)/H)`, `y(x) = A.ybot + round_half_up(H*(x - A.xbot)/W)`;
   - for `dir == 1`: `x(y) = A.xbot + floor(W*(A.ytop - y)/H)`, `y(x) = A.ybot + floor(H*(A.xtop - x)/W)` (truncation, never rounding up; see Q-MODEL-3; with `magic-compat` off, both diagonals SHALL round half-up as for `dir == 0`).
   - `r` is cut vertically at `x(r.ybot)`/`x(r.ytop)` and horizontally at `y(r.xbot)`/`y(r.xtop)` when those fall strictly inside `r`; pieces wholly on the operand's side are painted as rectangles, pieces wholly on the other side are skipped, and pieces still straddling the diagonal are processed recursively as triangles with bounding rectangle equal to the piece.
4. After painting, split-tile merging is applied over `A` expanded by 1 (see requirement "Split tile merging").

Consequently, when the diagonal crosses existing horizontal tile boundaries at non-integer x, the resulting geometry is an approximation built from triangles whose vertices are the rounded crossing points.

(Magic ref: database/DBpaint.c:788-1012,1360-1440,1476-1800, database/DBpaint2.c:61-74)

#### Scenario: Triangle on empty plane
- **GIVEN** an empty metal1 plane
- **WHEN** the left side of a `/` split of `(0,0)-(10,10)` is painted with `metal1`
- **THEN** the plane SHALL contain one split tile `(0,0)-(10,10)` with left type `metal1` and right type space

#### Scenario: Triangle over existing paint splits at integer crossings (verify against Magic)
- **GIVEN** `metal1 (0,5)-(10,10)`
- **WHEN** the left side of a `/` split of `(0,0)-(10,10)` is painted with `metal1`
- **THEN** the metal1 geometry SHALL be `rect 0 5 10 10` plus triangle `tri 0 0 5 5 nw` in `.mag` output

#### Scenario: Rounding of a non-integer crossing (verify against Magic)
- **GIVEN** a `/` triangle with bounding rectangle `(0,0)-(10,3)` crossing existing tile boundaries at `y = 1` and `y = 2`
- **WHEN** the crossings are computed
- **THEN** the cut at `y = 1` SHALL be at `x = 3` (10/3 = 3.33 rounded) and the cut at `y = 2` SHALL be at `x = 7` (20/3 = 6.67 rounded up)

### Requirement: Split tile merging

After any paint or erase that may involve split tiles, the system SHALL merge two split tiles into one larger split tile when they lie on a common diagonal, as defined below, so that repeated painting of the same triangle in pieces converges to one triangle.
Compatibility: exact

Two split tiles `T` (upper) and `U` (lower) merge when:
- they have identical split type (same left type, right type, direction);
- for `/`: `U`'s top-right corner equals `T`'s bottom-left corner; for `\`: `U`'s top-left corner equals `T`'s bottom-right corner;
- they have the same slope: `width(U) * height(T) == width(T) * height(U)`;
- the two rectangular gaps that the union bounding box leaves beside the two triangles are completely filled with the left type (the gap on the left-type side) and the right type (the gap on the right-type side) respectively.
The merge produces one split tile spanning both, with the gaps absorbed. Merging repeats while possible. Merging produces geometrically identical regions and is not recorded for undo.

(Magic ref: database/DBpaint.c:1018-1320)

#### Scenario: Two collinear triangles merge
- **GIVEN** an empty metal1 plane
- **WHEN** the left side of a `/` split of `(0,0)-(5,5)` is painted and then the left side of a `/` split of `(5,5)-(10,10)` together with `metal1 (0,5)-(5,10)`
- **THEN** the plane SHALL contain a single split tile `(0,0)-(10,10)` (`tri 0 0 10 10 nw`) (verify against Magic)

### Requirement: Copying paint between cells

Copying paint from a source cell (through a transform and optionally through hierarchy) into a target cell SHALL paint, on each plane separately, every non-space source tile of a selected type into the same plane of the target using the paint table of that type on that plane, clipped to the copy area, and SHALL NOT perform cross-plane image resolution.
Compatibility: exact

- Because a contact is present on each of its planes, per-plane copying reconstructs it on all planes; overlapping contacts from different sources combine through the paint table (possibly producing stacked types).
- If a tile's type is not in the copy mask but one of its residues on that plane is, the residue on that plane is copied instead (otherwise the tile is skipped).
- Split tiles are transformed (see `layout-geometry-model`) and clipped to the copy area; a clipped triangle is painted as the remaining triangle plus up to two rectangles.
- The target is marked modified.
- This is the mechanism used by flattening, yank/stuff and hierarchical selection copy (see `layout-hierarchy`).

(Magic ref: database/DBcellcopy.c:110-130,284-305,1518-1700)

#### Scenario: Flattening a child with a contact
- **GIVEN** child cell with `via (0,0)-(4,4)` placed with identity transform at `(100,0)` in a parent that has `metal2 (100,0)-(120,4)`
- **WHEN** the child's paint is copied into the parent
- **THEN** the parent metal1 plane SHALL hold `via (100,0)-(104,4)`
- **AND** the parent metal2 plane SHALL hold `via (100,0)-(104,4)` and `metal2 (104,0)-(120,4)`

### Requirement: Paint is local to the target cell

Painting or erasing in a cell SHALL affect only that cell's own planes and SHALL NOT alter any subcell definition, even where the area overlaps subcell instances.
Compatibility: behavioral

- Parent paint overlapping child paint is legal; the overlap is merged only by hierarchical consumers (DRC, extraction, mask generation, connectivity).
- After an edit, commands additionally mark the area for DRC re-check and adjust labels (see `cmd-paint-erase` and `layout-labels-ports`).

(Magic ref: database/DBpaint2.c:48-109, database/DBlabel.c:649-745)

#### Scenario: Paint over a subcell
- **GIVEN** a parent with an instance of child `inv` whose `metal1` covers `(0,0)-(10,10)` in parent coordinates
- **WHEN** `metal1` is erased in the parent over `(0,0)-(10,10)`
- **THEN** cell `inv` SHALL be unchanged and SHALL NOT be marked modified

### Requirement: Active (locked) layers

Painting and erasing through the user-level "valid" entry points SHALL affect only active layers; for a contact whose residues are not all active, painting SHALL paint only its active residues and erasing SHALL erase only its active residues.
Compatibility: behavioral

- Inactive layers are set via the technology/`tech lock` facility (see `tech-*` and `cmd-*`).
- Stacked types in a paint mask are replaced by their residues before filtering.
- Locked contacts cannot be removed by erasing one of their residues (rule 6 of "Default contact painting rules").

(Magic ref: database/DBpaint2.c:255-417, database/DBtpaint2.c:104-123,665-700)

#### Scenario: Painting a contact whose metal2 residue is locked
- **GIVEN** contact `via (metal1, metal2)` with `metal2` inactive and `via` therefore inactive
- **WHEN** the user paints `via` at `(0,0)-(4,4)`
- **THEN** only `metal1 (0,0)-(4,4)` SHALL be painted

### Requirement: Modification marking for paint and copy

Every paint, erase, paint-mask, erase-mask and paint-copy operation on a cell SHALL mark that cell modified and in need of a new timestamp, regardless of whether any geometry changed.
Compatibility: behavioral

- In Magic, undo/redo replay of paint does not itself set the flag. Hefesto SHALL mark the cell modified on every replay (see `layout-undo`, Q-MODEL-12).
- Bounding-box propagation to parents is not a modification (see `layout-hierarchy`).

(Magic ref: database/DBpaint2.c:65,177, database/DBcellcopy.c:1592, database/DBcellbox.c:178-186)

#### Scenario: Erasing nothing still marks modified
- **GIVEN** an unmodified empty cell
- **WHEN** `metal1` is erased over `(0,0)-(10,10)`
- **THEN** the cell SHALL be marked modified (see Q-MODEL-23)
