# Layout Labels and Ports

## Purpose

Specifies the label and port data model of a Magic-compatible layout database: label attributes (text, attached layer, rectangle, justification, outline font, size, rotation, offset, sticky flag), port attributes (index, connection directions, class, use, shape), how labels choose and keep their attached layer as paint changes, how labels behave on contacts and stacked contacts, how ports are renumbered, how hierarchical label searches decide which labels belong to an area, and how a cell is determined to be a subcircuit.

The `.mag` `rlabel`/`flabel`/`port` syntax belongs to capability `cell-file-format`; the `label`, `setlabel` and `port` commands belong to `cmd-labels-properties`; extraction use of labels belongs to `extract-nodes-and-labels`.

## Requirements

### Requirement: Label data model

Every label SHALL consist of a non-empty text string, an attached tile type (possibly space), a rectangle in cell coordinates (which may have zero width and/or height), a justification position, an optional outline font with size, rotation and offset, a flags word and a port index; labels of a cell SHALL form an ordered list in creation order.
Compatibility: exact

| Field | Domain | Notes |
|---|---|---|
| text | string | empty-text labels are never written |
| type | tile type or space | the layer the label is attached to |
| rect | integer rectangle | point labels: zero width and height; line labels: one zero dimension |
| justification | 0..8 | `0 center, 1 N, 2 NE, 3 E, 4 SE, 5 S, 6 SW, 7 W, 8 NW`: the side of the anchor on which the text is drawn |
| font | -1 or font index | -1 = default (non-scalable) font |
| size | integer, 1/8 database unit | 0 for the default font |
| rotation | degrees | normalized into `0..359` when the text box is computed |
| offset | point, 1/8 database unit | displacement of the text from the anchor |
| sticky | flag | the attached layer never changes automatically |
| port directions | N, E, S, W bits | a label is a port iff at least one bit is set |
| port class | default, input, output, tristate, bidirectional, feedthrough | |
| port use | default, signal, analog, power, ground, clock, reset, scan, tieoff | reset/scan/tieoff are not persisted by `.mag` (Q-MODEL-18) |
| port shape | default, abutment, ring, feedthrough | |
| port index | unsigned integer | meaningful only for ports |

- New labels are appended at the end of the list; this order is the `.mag` output order.
- If the font index is not a loaded font, the label uses the default font with size 0, rotation 0, offset `(0,0)`.

(Magic ref: database/database.h.in:219-319, database/DBlabel.c:135-232, database/DBio.c:4800-4905)

#### Scenario: Label appended at end
- **GIVEN** a cell with labels `a` then `b`
- **WHEN** label `c` is created
- **THEN** the label list SHALL be `a, b, c`

### Requirement: Automatic justification

When a label is created without a justification, the system SHALL choose one that keeps the text inside the cell boundary using a border of `min(5, floor(dimension/3))` units.
Compatibility: exact

With the cell bounding box `B`, `tx = min(5, floor((B.xtop - B.xbot)/3))`, `ty = min(5, floor((B.ytop - B.ybot)/3))`, `x1 = B.xbot + tx`, `x2 = B.xtop - tx`, `y1 = B.ybot + ty`, `y2 = B.ytop - ty`, and the label centre `lx = floor((r.xbot + r.xtop)/2)`, `ly = floor((r.ybot + r.ytop)/2)` (C integer division):
- `lx <= x1`: `ly <= y1` -> NE; `ly >= y2` -> SE; else E.
- `lx >= x2`: `ly <= y1` -> NW; `ly >= y2` -> SW; else W.
- otherwise: `ly <= y1` -> N; `ly >= y2` -> S; else N.

(Magic ref: database/DBlabel.c:160-196)

#### Scenario: Corner label
- **GIVEN** cell bounding box `(0,0)-(30,30)`
- **WHEN** a label is created at point `(2,2)` with no justification
- **THEN** its justification SHALL be NORTHEAST

#### Scenario: Central label
- **GIVEN** cell bounding box `(0,0)-(30,30)`
- **WHEN** a label is created at point `(15,15)` with no justification
- **THEN** its justification SHALL be NORTH

### Requirement: Rendered text box of outline-font labels

For labels with an outline font the system SHALL compute the rendered text bounding box, used for the extended bounding box and display searches, exactly as follows.
Compatibility: exact

1. Sum the advance widths of all characters except the last, plus the last character's glyph right extent, giving `w`; `top = max(glyph tops, font ascent extent)`, `bot = min(glyph bottoms)` (font units).
2. Scale: each of `0, w, bot, top` is multiplied by `size` and divided (C truncation) by the unscaled `top` (result in 1/8 units).
3. Vertical justification: S/SW/SE: `bot -= top; top = 0`. E/W/center: `top >>= 1; bot -= top`. Horizontal: W/NW/SW: `xbot -= xtop; xtop = 0`. N/S/center: `xtop >>= 1; xbot -= xtop`.
4. Add the offset; rotate the four corners by `rotation` degrees counter-clockwise using `round()` of the double-precision result; take their bounding box.
5. Add `4 * (r.xbot + r.xtop)` and `4 * (r.ybot + r.ytop)` (the anchor centre in 1/8 units); convert each coordinate to database units by arithmetic right shift by 3, decrementing a lower bound / incrementing an upper bound when `(value & 0x7f) != 0` (see Q-MODEL-6).

(Magic ref: database/DBlabel.c:1352-1500)

#### Scenario: Default-font label has no text box
- **GIVEN** a label with font -1
- **WHEN** the extended bounding box is computed
- **THEN** only the label rectangle SHALL contribute

### Requirement: Choosing a label's layer

The system SHALL choose the layer a label is attached to by examining the paint under the label and preferring, in order, the label's current layer, layers connected to it, and layers on its plane, with the exact priority below.
Compatibility: exact

Compute three type sets over all planes from the first selectable plane upward:
- point label: every non-space type touching the label rectangle expanded by 1 goes into all three sets;
- line label: the two 1-unit strips on either side are evaluated as for rectangles and the results ORed;
- rectangular label: `covers` = types that cover the whole label area on their plane (any space tile under the label on a plane removes all types of that plane); `components` = types that are residues of the material covering the whole area; `touches` = types present anywhere under it.

Then: if the current type is in `covers`, keep it. Otherwise scan planes from low to high and, within a plane, user types from low to high, keeping the last match of each category:
1. same plane as the current type, connects to it, in `covers`;
2. same plane, connects, in `components`;
3. connects, in `covers`;
4. connects, in `components`;
5. on the same plane (or any plane if the current type is space), in `covers` — a candidate that connects to some type other than itself replaces an earlier one, otherwise the earlier one is kept;
6. same plane (or any if space), in `components`;
7. space.
The first non-space category wins. Generated stacked types are never chosen.

A GDS/LEF-read variant uses: (1) same-plane covering non-device type, (2) same-plane covering device type, (3) same-plane component, else "none" (see `gds-read`, `lef-read`).

(Magic ref: database/DBlabel.c:864-1110,1117-1190)

#### Scenario: Label migrates onto a contact
- **GIVEN** a label `A` on `metal1` with rectangle `(2,2)-(4,4)` and a tech where `via` connects to `metal1`
- **WHEN** `via` is painted over `(0,0)-(6,6)` and labels are adjusted
- **THEN** the label SHALL be attached to `via`

### Requirement: Label adjustment after paint changes

After paint in an area of a cell changes, the system SHALL re-evaluate every non-sticky label whose rectangle touches that area, either re-attaching it to the chosen layer or, if no layer covers it but some of its old layer remains under it, shrinking the label rectangle onto the remaining material, and SHALL mark the cell modified if any label changed.
Compatibility: exact

For each label touching the changed area:
1. `new = choose_layer(label)`. If `new == type`, or the label is sticky, nothing happens.
2. If `new` is space and some tile of the label's old type remains inside the label rectangle on the old type's home plane: start from the label rectangle and, for each tile of any other type on that plane overlapping it (in tile enumeration order), apply the first applicable clip: if the tile's left edge is right of the label's left edge, set `xtop = tile.xbot`; else if the tile's right edge is left of the label's right edge, set `xbot = tile.xtop`; else if its bottom is above the label bottom, set `ytop = tile.ybot`; else if its top is below the label top, set `ybot = tile.ytop`. If the result is non-null, the label keeps its type and takes the new rectangle ("Adjusting size of label").
3. Otherwise the label's type becomes `new` ("Moving label ... from X to Y").
Each change is recorded for undo. Messages are informative and printed only in verbose mode for non-internal cells.

(Magic ref: database/DBlabel.c:585-745)

#### Scenario: Label shrinks onto remaining metal
- **GIVEN** `metal1 (0,0)-(10,10)` and non-sticky label `A` on metal1 with rectangle `(2,2)-(8,8)`
- **WHEN** `metal1` is erased over `(0,0)-(5,10)` and labels are adjusted
- **THEN** label `A` SHALL stay on metal1 with rectangle `(5,2)-(8,8)`

#### Scenario: Label falls to space
- **GIVEN** `metal1 (0,0)-(10,10)` and non-sticky label `A` on metal1 at point `(5,5)`
- **WHEN** `metal1` is erased over `(0,0)-(10,10)` and labels are adjusted
- **THEN** label `A` SHALL be attached to space

#### Scenario: Sticky label never moves
- **GIVEN** the same layout with label `A` sticky
- **WHEN** the metal is erased and labels are adjusted
- **THEN** label `A` SHALL remain attached to metal1 with its rectangle unchanged

### Requirement: Erasing labels together with paint

When labels are erased in an area for a set of layers, the system SHALL delete a label only if its rectangle is in the area (label-in-area predicate), its type is in the set, and either the set explicitly includes "labels", the label is on space, or the layer chosen for it no longer connects to its current type; an optional glob pattern SHALL further restrict deletion by text.
Compatibility: exact

- Deleting up to the edge of a label does not delete it while its layer still covers it.
- The cell is marked modified only if at least one label was deleted.

(Magic ref: database/DBlabel.c:263-352, utils/geometry.h:148-152)

#### Scenario: Label survives partial erase
- **GIVEN** `metal1 (0,0)-(20,10)` and a point label `A` on metal1 at `(10,5)`
- **WHEN** paint and then labels of `metal1` are erased over `(0,0)-(10,10)`
- **THEN** label `A` SHALL NOT be deleted, because metal1 at `(10,0)-(20,10)` still touches it

### Requirement: Labels on contacts and stacked contacts

Labels SHALL never be attached to a generated stacked-contact type: a label read or created with a stacked type SHALL be attached to the highest-numbered user residue contact of that stacked type.
Compatibility: exact

- Labels on a contact type attach to the contact as a whole (all its planes) for connectivity purposes.
- The layer-choice algorithm only considers user types, so a label on a stacked area is attached to a component contact or residue.

(Magic ref: database/DBio.c:4141-4155, database/DBlabel.c:1020-1100)

#### Scenario: Stacked label type on read
- **GIVEN** a `.mag` label line naming type `via1+via2`
- **WHEN** the cell is read
- **THEN** the label SHALL be attached to `via2` (assuming `via2` has the larger type number)

### Requirement: Ports

A label SHALL be a port exactly when at least one of its port direction bits is set; ports SHALL carry an index, and several port labels with the same index in one cell SHALL denote the same electrical terminal.
Compatibility: behavioral

- Port index uniqueness across different names is not enforced by the data model (commands enforce it; see `cmd-labels-properties`).
- During hierarchical net tracing, reaching one port label also reaches the material under every other port label of the same cell with the same index and a non-space type (see `layout-connectivity`).
- Removing port status clears all flags (directions, class, use, shape and sticky).

(Magic ref: database/database.h.in:276-317, database/DBconnect.c:767-845, commands/CmdLQ.c:2061-2068)

#### Scenario: Equivalent ports
- **GIVEN** cell `pad` with two port labels `VDD` index 1 on disjoint metal1 rectangles
- **WHEN** a net is traced from one of them in a parent
- **THEN** the material under the other `VDD` label SHALL also be part of the net

### Requirement: Port renumbering

Renumbering ports SHALL sort all labels of the cell with a case-insensitive natural-order comparison (digit runs compared numerically) and assign consecutive indices starting at 0 to port labels in sorted order, giving the same index to consecutive port labels with identical text.
Compatibility: exact

- Non-port labels are skipped but participate in sorting.
- "Identical text" for index sharing is a case-sensitive comparison with the previously numbered port (see Q-MODEL-10).
- The natural comparison: compare character by character case-insensitively; when both strings are at a digit, compare the maximal digit runs as unsigned integers. If one string is a prefix of the other the comparator is not antisymmetric in Magic (Q-MODEL-9); Hefesto SHALL sort the shorter string first.

(Magic ref: commands/CmdLQ.c:1430-1460,2070-2120, database/DBcellname.c:676-713)

#### Scenario: Natural ordering of port indices
- **GIVEN** port labels created in the order `b`, `A10`, `A2`, and a non-port label `c`
- **WHEN** ports are renumbered
- **THEN** `A2` SHALL get index 0, `A10` index 1 and `b` index 2

#### Scenario: Duplicate port text shares an index
- **GIVEN** port labels `VDD`, `GND`, `VDD`
- **WHEN** ports are renumbered
- **THEN** `GND` SHALL get index 0 and both `VDD` labels index 1

### Requirement: Subcircuit determination

A cell definition SHALL be a subcircuit if and only if at least one of its labels is a port.
Compatibility: exact

- Subcircuit status controls extraction/netlisting (cell emitted as a subcircuit) and the "no-subcircuit" descend mode (see `layout-hierarchy`).

(Magic ref: database/DBlabel.c:71-79, database/DBcellsubr.c:76-81)

#### Scenario: Port makes a subcircuit
- **GIVEN** a cell with one plain label and one port label
- **WHEN** subcircuit status is queried
- **THEN** it SHALL be true

### Requirement: Hierarchical label search

A hierarchical label search over an area SHALL report labels of the root cell whose rectangles touch the area, and labels of descended subcells whose rectangles overlap the area expanded by one unit, each with its hierarchical path, in the order root labels first then each subcell recursively (or children first in "reverse" mode used by display).
Compatibility: behavioral

- Outline-font labels match on their rendered text box only in display searches; attachment searches use the label rectangle.
- When the search area is one side of a split tile, labels touching only the far corner of the triangle are excluded (the area is split into a horizontal and a vertical strip along the two legs of the triangle).
- Path prefixes are element names (`layout-hierarchy`) followed by `/`; the root of a window contributes no prefix.

(Magic ref: database/DBcellsrch.c:547-760, database/database.h.in:659-671)

#### Scenario: Label in a subcell found with path
- **GIVEN** `top` containing use `u_0` whose child has label `out` at `(1,1)`, use at identity
- **WHEN** labels touching `(0,0)-(2,2)` are searched hierarchically
- **THEN** the result SHALL include `out` with path `u_0/`
