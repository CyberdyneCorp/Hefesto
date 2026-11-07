# Layout Geometry Model

## Purpose

Defines the geometric foundation of the Hefesto layout database so that it is data-compatible with Magic VLSI: the integer coordinate space and its limits, rectangles and their predicates, the plane model, Manhattan and non-Manhattan (split / triangle) geometry, the canonical decomposition of a plane into maximal horizontal strips (and every place where that decomposition becomes observable), and the eight Manhattan transforms with their matrix form, composition, inversion and effect on points, rectangles, positions, angles and triangles.

Hefesto is not required to store geometry as corner-stitched tiles. Requirements marked `exact` describe observable results; data-structure descriptions are `informative`. Paint operations themselves are specified in capability `layout-paint-semantics`; hierarchy in `layout-hierarchy`; `.mag` syntax in `cell-file-format`; technology types and planes in `tech-layers-and-planes`, contacts in `tech-contacts-and-stacking`.

## Requirements

### Requirement: Integer database coordinate space

The system SHALL represent all layout geometry in signed integer database units ("internal units") on a 2-D plane with x increasing to the right and y increasing upward, and SHALL NOT use floating point for stored geometry.
Compatibility: exact

- Coordinates are 32-bit signed integers. The representable plane is bounded by `INFINITY = 2^30 - 4 = 1073741820` and `MINFINITY = -INFINITY`.
- The usable plane area ("whole plane rectangle") is `[MINFINITY+2, INFINITY-2]` in both x and y, i.e. `(-1073741818, -1073741818) .. (1073741818, 1073741818)`. Searches "over the whole plane" (file output, bounding-box computation, global erase) use exactly this rectangle.
- Products of two coordinates (slope tests on triangles, array-overlap computations) SHALL be computed in at least 64-bit precision.
- Behaviour for geometry outside the usable area is undefined in Magic (no clipping is performed). Hefesto SHALL reject such geometry with an error; see Q-MODEL-24.
- The relationship between internal units and lambda / microns (scale factors, `scalegrid`, `.mag` `magscale`) is specified in `tech-loading-and-scaling`, `cmd-coordinates-and-units` and `cell-file-format`, not here.

(Magic ref: tiles/tile.h:149-165, tiles/tile.c:67, database/DBtiles.c:567-574)

#### Scenario: Whole-plane rectangle
- **GIVEN** a freshly created empty cell
- **WHEN** the whole-plane rectangle is queried
- **THEN** it SHALL be `(-1073741818,-1073741818)-(1073741818,1073741818)`

#### Scenario: Coordinates outside the usable area are refused
- **GIVEN** a paint request with rectangle `(0,0)-(1073741820,10)`
- **WHEN** Hefesto processes it
- **THEN** it SHALL either clip it to `x <= 1073741818` or reject it with an error, and SHALL NOT corrupt the plane

### Requirement: Rectangle semantics and predicates

The system SHALL model a rectangle as a lower-left point `(xbot,ybot)` and an upper-right point `(xtop,ytop)` describing the half-open point set `xbot <= x < xtop, ybot <= y < ytop` for area purposes, and SHALL implement the following predicates exactly.
Compatibility: exact

| Predicate | Definition |
|---|---|
| null (no area) | `xbot >= xtop` or `ybot >= ytop` |
| overlap(r1,r2) | `r1.xbot < r2.xtop && r2.xbot < r1.xtop && r1.ybot < r2.ytop && r2.ybot < r1.ytop` (shared area > 0) |
| touch(r1,r2) | same with `<=` (overlap, shared edge or shared corner) |
| surround(r1,r2) | r2 inside r1, touching allowed (`>=`/`<=`) |
| strong-surround(r1,r2) | r2 strictly inside r1 (`>`/`<`) |
| point-enclosed(p,r) | `xbot <= px <= xtop && ybot <= py <= ytop` (closed) |
| label-in-area(lab,area) | `surround(area,lab)` OR (`null(area)` AND `touch(lab,area)` AND NOT `strong-surround(lab,area)`) |
| canonical(r) | swap coordinates so that `xbot <= xtop`, `ybot <= ytop` |

- Zero-width or zero-height rectangles are legal values (used for point and line labels) but contribute no area.
- "Include r into bbox" ignores a null source rectangle; if the destination is null it is replaced by the source (bounding-box accumulation).
- "Expand by n" subtracts n from `xbot,ybot` and adds n to `xtop,ytop`.

(Magic ref: utils/geometry.h:103-171, utils/geometry.c:642-677, utils/geometry.c:966-991)

#### Scenario: Overlap versus touch
- **GIVEN** `r1 = (0,0)-(10,10)` and `r2 = (10,0)-(20,10)`
- **WHEN** the predicates are evaluated
- **THEN** `overlap(r1,r2)` SHALL be false
- **AND** `touch(r1,r2)` SHALL be true

#### Scenario: Corner-only contact touches
- **GIVEN** `r1 = (0,0)-(10,10)` and `r2 = (10,10)-(20,20)`
- **WHEN** the predicates are evaluated
- **THEN** `touch(r1,r2)` SHALL be true and `overlap(r1,r2)` SHALL be false

#### Scenario: Point label inside a zero-size search area
- **GIVEN** a point label at `(5,5)-(5,5)` and a search area `(5,5)-(5,5)`
- **WHEN** label-in-area is evaluated
- **THEN** the result SHALL be true

### Requirement: Plane model

Each cell definition SHALL hold one independent paint plane per technology plane, where every point of every plane carries exactly one tile type (space when nothing is painted), and SHALL additionally hold the DRC bookkeeping planes.
Compatibility: behavioral

- Plane numbering is defined by the technology (see `tech-layers-and-planes`): planes 0..5 are built in (`router` / unused, `designRuleCheck`, `designRuleError`, and three router hint planes `mhint`, `fhint`, `rhint`); technology planes start at 6; at most 64 planes exist in total.
- Tile types are small integers below 256; types 0..8 are built in (`space`, `checkpaint`, `checksubcell`, `error_p`, `error_s`, `error_ps`, `magnet`, `fence`, `rotate`), technology types start at 9, and automatically generated stacked-contact types are numbered after all user types.
- A non-contact type lives on exactly one plane (its home plane). A contact type is present, with the same type number, on every plane of each of its residues; its home plane is the lowest-numbered of those planes. A generated stacked-contact type has as home plane the plane of the residue shared by its two component contacts.
- Space has no home plane and may appear on every plane except plane 0.
- Paint on different planes never interacts directly; interaction between planes happens only through contacts (see `layout-paint-semantics`) and through connectivity (see `layout-connectivity`).

(Magic ref: database/database.h.in:46,85-143,191-205, database/DBtcontact.c:108-117,400-412, database/DBtpaint2.c:147-170, database/DBcellname.c:2008-2055)

#### Scenario: Contact occupies all its residue planes
- **GIVEN** a technology with contact `via` whose residues are `metal1` (plane metal1) and `metal2` (plane metal2)
- **WHEN** `via` is painted at `(0,0)-(4,4)`
- **THEN** the metal1 plane SHALL hold type `via` over `(0,0)-(4,4)`
- **AND** the metal2 plane SHALL hold type `via` over `(0,0)-(4,4)`

### Requirement: Canonical maximal-horizontal-strip decomposition

For every plane, the system SHALL be able to produce the unique decomposition of the plane's Manhattan regions into maximal horizontal strips ("tiles"), and SHALL use exactly this decomposition wherever the decomposition is observable.
Compatibility: exact

Definition (for planes containing no split tiles): for every horizontal unit row `[y, y+1)`, the row is cut into maximal runs `[x1, x2)` of identical tile type (the exact type number, so different contact or stacked types are distinct). Two runs in rows `y` and `y+1` belong to the same tile if and only if they have the same `x1`, the same `x2` and the same type. A tile is the union of a maximal vertical chain of such runs. Consequently:

1. No two tiles that share a vertical edge segment have the same type.
2. Two vertically adjacent tiles with the same type and the same left and right x coordinates do not exist (they would be one tile).
3. The decomposition depends only on the geometry, never on the order in which paint operations were performed.
4. Space is decomposed the same way (space tiles extend to the plane limits).

Observable consequences (Hefesto MUST reproduce them):
- Every `rect` line written for a type in a `.mag` file is one tile of that type (see requirement "Tile enumeration order").
- `drc count` and any other count of DRC error areas counts error tiles of this decomposition.
- Any other output that lists "tiles" (e.g. `.ext` or debugging output enumerating paint) uses this decomposition.

Hefesto MAY store geometry in any structure; it MUST be able to regenerate this decomposition for output.

(Magic ref: database/DBpaint.c:63-90,242-700, database/DBio.c:4751-4774,5940-6004, drc/DRCmain.c:940-987)

#### Scenario: Adjacent rectangles merge horizontally
- **GIVEN** an empty cell
- **WHEN** `metal1` is painted at `(0,0)-(10,10)` and then at `(10,0)-(20,10)`
- **THEN** the metal1 plane SHALL contain exactly one metal1 tile `(0,0)-(20,10)`
- **AND** the `.mag` output SHALL contain the single line `rect 0 0 20 10` under `<< metal1 >>`

#### Scenario: Stacked rectangles merge vertically only when aligned
- **GIVEN** an empty cell
- **WHEN** `metal1` is painted at `(0,0)-(10,10)` and at `(0,10)-(10,20)`
- **THEN** there SHALL be one metal1 tile `(0,0)-(10,20)`
- **AND** WHEN instead the second rectangle is `(5,10)-(15,20)`, there SHALL be two metal1 tiles `(5,10)-(15,20)` and `(0,0)-(10,10)`

#### Scenario: Decomposition is independent of paint history
- **GIVEN** cell A painted with `metal1 (0,0)-(20,10)` then `metal1 (0,10)-(10,20)`
- **AND** cell B painted with `metal1 (0,0)-(10,20)` then `metal1 (10,0)-(20,10)`
- **WHEN** both are written as `.mag`
- **THEN** both SHALL contain, in this order, `rect 0 10 10 20` and `rect 0 0 20 10`

### Requirement: Tile enumeration order

When tiles of a plane are enumerated over an area (for file output, counting, or searches whose order is observable), the system SHALL visit them in Magic's area-enumeration order, defined as a depth-first pre-order traversal of the "lower-left parent" forest.
Compatibility: exact

Given a search area `A` and the canonical decomposition:
- A tile `T` is visited if it overlaps `A` (positive-area overlap); the type filter only decides whether `T` is reported, never the traversal.
- Roots are the tiles that contain points on the left edge of `A` (`x = A.xbot`), visited from top to bottom.
- The parent of a non-root tile `R` is the tile containing the point `(R.xbot - 1, max(R.ybot, A.ybot))`.
- After visiting a tile, its children (tiles whose parent it is) are visited, ordered from top to bottom, each followed immediately by its own subtree.
- A split (triangle) tile is reported at most twice in one visit: first its left part, then its right part, each only if its type matches the filter and the part intersects `A` (see requirement "Non-Manhattan (split tile) geometry").

For `.mag` output the area is the whole plane and the enumeration is performed separately for every user type in technology order (see capability `cell-file-format`, requirement "Writer paint sections").

(Magic ref: database/DBtiles.c:521-640, database/DBio.c:4751-4774)

#### Scenario: Enumeration is a tree walk, not a raster scan
- **GIVEN** an empty cell with `metal1` painted at `(20,0)-(30,30)` (X), `(0,20)-(10,30)` (Y) and `(0,0)-(10,10)` (Z)
- **WHEN** the cell is written as `.mag`
- **THEN** the metal1 rectangles SHALL appear in the order `rect 0 20 10 30`, `rect 0 0 10 10`, `rect 20 0 30 30`
- **AND** X SHALL appear last because its parent is the space tile `(10,0)-(20,10)` whose parent is Z

#### Scenario: Simple raster-like case
- **GIVEN** `metal1` at `(0,20)-(10,30)`, `(0,0)-(10,10)` and `(20,0)-(30,10)`
- **WHEN** the cell is written as `.mag`
- **THEN** the order SHALL be `rect 0 20 10 30`, `rect 0 0 10 10`, `rect 20 0 30 10`

### Requirement: Non-Manhattan (split tile) geometry

The system SHALL support right-triangle geometry where a rectangle is split along one of its diagonals into two triangles, each carrying its own tile type, and SHALL represent each triangle by its bounding rectangle plus the compass corner at which the right angle lies.
Compatibility: exact

- A split rectangle `(xbot,ybot)-(xtop,ytop)` has a direction: `/` (diagonal from `(xbot,ybot)` to `(xtop,ytop)`) or `\` (from `(xbot,ytop)` to `(xtop,ybot)`), and two sides: left and right of the diagonal.
- Corner naming (used by `.mag` `tri` records and by commands):

| side | direction | right-angle corner |
|---|---|---|
| left | `/` | `nw` |
| left | `\` | `sw` |
| right | `/` | `se` |
| right | `\` | `ne` |

- For direction `/` the left part is the upper-left triangle (north = west type) and the right part the lower-right (south = east type); for `\` the left part is the lower-left and the right part the upper-right.
- A split rectangle whose two sides carry the same type is not a split tile: it is the plain rectangle of that type.
- A triangle intersects a rectangle `S` (for searches) iff `S` overlaps the bounding rectangle and `S` is not entirely on the far side of the diagonal; the test uses 64-bit cross-multiplication: for the left part with `/`, `(S.ytop - ybot) * width > (S.xbot - xbot) * height`; for the left part with `\`, `(ytop - S.ybot) * width > (S.xbot - xbot) * height`; symmetric formulas hold for the right part.
- Diagonals are only those of a rectangle with integer corners; arbitrary-angle polygons are decomposed into rectangles and such triangles by the commands that create them (see `cmd-paint-erase`).
- The maximal-horizontal-strip canonical form does NOT uniquely determine non-Manhattan geometry: the set of split tiles depends on painting history (fracturing and merging rules in `layout-paint-semantics`). See Q-MODEL-5.

(Magic ref: database/database.h.in:139-143,539-554, tiles/tile.h:205-254, database/DBtiles.c:557-597, database/DBio.c:5951-6000, database/DBio.c:927-933)

#### Scenario: Triangle written with its right-angle corner
- **GIVEN** an empty cell
- **WHEN** `metal1` is painted as the left part of a `/` split of `(0,0)-(10,10)`
- **THEN** the `.mag` output SHALL contain `tri 0 0 10 10 nw` under `<< metal1 >>`

#### Scenario: Both halves equal collapse to a rectangle
- **GIVEN** a split tile `(0,0)-(10,10)` with left type `metal1` and right type space
- **WHEN** `metal1` is painted on the right part of the same split
- **THEN** the plane SHALL contain a plain `metal1` rectangle `(0,0)-(10,10)` and no split tile

### Requirement: Manhattan transforms

The system SHALL represent placement transforms as integer affine maps `x' = a*x + b*y + c`, `y' = d*x + e*y + f` where `(a,b,d,e)` is one of the eight Manhattan orientation matrices, and SHALL compose, invert and apply them exactly as specified.
Compatibility: exact

The eight orientations (with the DEF-style orientation name returned for each):

| a b d e | meaning | DEF name |
|---|---|---|
| 1 0 0 1 | identity | N |
| 0 1 -1 0 | rotate 90 deg clockwise (`x'=y, y'=-x`) | E |
| -1 0 0 -1 | rotate 180 | S |
| 0 -1 1 0 | rotate 90 deg counter-clockwise (270 clockwise) | W |
| 1 0 0 -1 | mirror top-to-bottom (`y'=-y`), "upside down" | FS |
| -1 0 0 1 | mirror left-to-right (`x'=-x`), "sideways" | FN |
| 0 1 1 0 | reflect about the 45-degree line (swap x,y) | FW |
| 0 -1 -1 0 | reflect about the 135-degree line | FE |

Rules:
- Point: apply the formula. Delta (offset/vector): apply without `c,f`.
- Rectangle: transform `(xbot,ybot)` and `(xtop,ytop)` and re-sort each axis; no rounding is ever involved.
- Composition "first then second" = `second ∘ first`: `a = a1*a2 + d1*b2`, `b = b1*a2 + e1*b2`, `c = c1*a2 + f1*b2 + c2`, `d = a1*d2 + d1*e2`, `e = b1*d2 + e1*e2`, `f = c1*d2 + f1*e2 + f2`.
- "Translate then T" (`T(p + (x,y))`): `c' = x*a + y*b + c`, `f' = x*d + y*e + f`. "T then translate": `c' = c + x`, `f' = f + y`.
- Inverse: rotation part transposed (`a'=a, b'=d, d'=b, e'=e`), translation `-(R^T)(c,f)`.
- Scale (display only) multiplies all six entries by an integer; the scale of a transform is `|a|` or, if `a = 0`, `|b|`.
- Decomposition into (upside-down flag, angle), used by display and output writers: upside-down iff `(a == 0) XOR (b == d) XOR (a == e)`; if upside-down, the rotation-only matrix R is the transform composed with "upside down" (`y'=-y`) applied second, else R is the transform's matrix. `angle = 0`; if `R.b != 0` add 90 and, when upside-down, add another 180; if `R.a < 0` or `R.b < 0` add 180; if the result exceeds 270 subtract 360.

(Magic ref: utils/geometry.h:63-84, utils/geometry.c:39-51,95-141,205-335,547-624,1007-1047,1129-1165)

#### Scenario: Rotating a rectangle
- **GIVEN** transform `(0,1,0,-1,0,0)` (90 clockwise)
- **WHEN** rectangle `(0,0)-(10,20)` is transformed
- **THEN** the result SHALL be `(0,-10)-(20,0)`

#### Scenario: Composition and inverse
- **GIVEN** `T = (0,1,100,-1,0,0)`
- **WHEN** point `(10,0)` is transformed by T
- **THEN** the result SHALL be `(100,-10)`
- **AND** the inverse of T SHALL be `(0,-1,0,1,0,-100)`, mapping `(100,-10)` back to `(10,0)`

#### Scenario: Orientation name
- **GIVEN** transform `(1,0,5,0,-1,7)`
- **WHEN** its orientation is queried
- **THEN** it SHALL be `FS`

### Requirement: Transforming positions, angles and triangles

The system SHALL transform text positions (justifications), label rotation angles and triangle corners through Manhattan transforms with the exact rules below.
Compatibility: exact

Positions are integers: `0 center, 1 N, 2 NE, 3 E, 4 SE, 5 S, 6 SW, 7 W, 8 NW`; values outside 1..8 are returned unchanged.
1. Rotation: let `p = pos - 1`. If `a < 0` add 4; else if `a == 0` add 6 when `b < 0`, else add 2; reduce modulo 8; add 1.
2. Mirror: if `a != e`, or (`a == 0` and `b == d`), swap N<->S, NE<->SE, NW<->SW (E and W unchanged).

Opposite positions: N<->S, NE<->SW, E<->W, SE<->NW, center<->center.

Angles (degrees, label rotation): if `a == 0 && e == 0`: add 90 when `b > 0`, else add 270, and the transform is a flip if `b == d`; otherwise add 180 when `a < 0`, and it is a flip if `a != e`. Then if the sum `> 360` subtract 360. If it is a flip: when the original angle is strictly between 90 and 270 the result is `360 - sum`, else `-sum`. Finally add 360 if negative. Note: a sum of exactly 360 is not reduced (see Q-MODEL-2), and the sense of rotation is questioned in Q-MODEL-1. With `magic-compat` OFF, a sum of exactly 360 SHALL be reduced to 0 (Q-MODEL-2), and the clockwise-90 transform SHALL subtract 90 instead of adding it (Q-MODEL-1).

Triangles: the triangle's bounding rectangle transforms as a rectangle and its right-angle corner transforms as a position (rule above); the direction/side encoding is then derived from the new corner using the table of requirement "Non-Manhattan (split tile) geometry".

Position names accepted on input (unique-prefix matching; `manhattan` restricts to the starred entries): `bl, bottom*, br, center, d*, dl, down*, dr, e*, east*, left*, n*, ne, north*, northeast, northwest, nw, right*, s*, se, south*, southeast, southwest, sw, tl, top*, tr, u*, ul, up*, ur, w*, west*`.

(Magic ref: utils/geometry.c:58-69,154-189,353-446,497-532, database/DBconnect.c:73-125)

#### Scenario: Justification under rotation
- **GIVEN** transform `(0,1,0,-1,0,0)`
- **WHEN** position NORTH (1) is transformed
- **THEN** the result SHALL be EAST (3)

#### Scenario: Justification under mirror
- **GIVEN** transform `(1,0,0,0,-1,0)` (upside down)
- **WHEN** position NORTHEAST (2) is transformed
- **THEN** the result SHALL be SOUTHEAST (4)

#### Scenario: Triangle corner under rotation
- **GIVEN** a triangle `nw` on `(0,0)-(10,10)` and transform `(0,1,0,-1,0,0)`
- **WHEN** it is transformed
- **THEN** the result SHALL be triangle `ne` on `(0,-10)-(10,0)`

#### Scenario: Angle wrap quirk (verify against Magic)
- **GIVEN** transform `(0,1,0,-1,0,0)` and label rotation 270
- **WHEN** the angle is transformed
- **THEN** the result SHALL be 360

### Requirement: Geometry data structure is informative

Magic stores each plane as a corner-stitched tile plane and the subcells of a cell in a binned "bplane"; the system MAY use any representation provided every `exact` requirement of this capability is met.
Compatibility: informative

- Magic tile: lower-left point, type word (bit 30 = split, bit 28 = `\` direction, bits 0..13 left type, bits 14..27 right type; bit 29 is a transient "side" flag), four corner stitches. Top/right coordinates are derived from neighbours. Each plane carries a "hint" tile for locality of searches.
- Paint is performed by splitting tiles at the paint boundary, changing types via a result table, and re-merging (see `layout-paint-semantics`).
- Interrupting a long paint or search (a user interrupt in Magic) may leave a partially applied operation; Hefesto SHALL make paint operations atomic. See Q-MODEL-27.

(Magic ref: tiles/tile.h:65-99,205-254, database/DBpaint.c:242-330, bplane/bplane.h)

#### Scenario: Alternative representation
- **GIVEN** Hefesto stores paint as an R-tree of rectangles
- **WHEN** a cell is written as `.mag`
- **THEN** the rectangles SHALL first be converted to the canonical maximal-horizontal-strip decomposition and written in tile enumeration order
