# DRC Rule Semantics

## Purpose

Defines the exact geometric check performed by each primitive DRC rule produced by capability `drc-rule-language`, when a *basic check* is run over a single flat cell (or a flattened yank buffer, see `drc-engine`). It specifies which edges are examined, the check rectangle built for each rule, corner extensions, Manhattan and Euclidean corner handling, trigger rules, and every nonstandard rule type (area, maxwidth, rectangle size, off-grid, angles), together with the exact error rectangles produced. With the `magic-compat` setting ON (the default) Hefesto produces the same set of error rectangles (as a union on the error plane) and associates them with the same why index as Magic 8.3.694. Where an approved decision resolves a Magic quirk as *compat* (replicate under magic-compat, fix with it off), the requirement states both modes; quirks resolved as *replicate* hold in both modes.

Conventions: all coordinates are integers in internal units. Rectangles are `[xbot,xtop]×[ybot,ytop]`; a rectangle is *null* when `xbot ≥ xtop` or `ybot ≥ ytop`. `clip(R, C)` is the intersection. Each paint plane is a corner-stitched plane of maximal horizontal strips (see capability `layout-geometry-model`); "tile" means a tile of that canonical decomposition. Split (diagonal) tiles have a left type and a right type; direction d=1 is a `\` diagonal (top-left to bottom-right), d=0 is `/`; the *top type* of a split tile is its right type when d=1 and its left type when d=0, the *bottom type* is the other one. `type(x,y)` with a side qualifier means the type of the unit square `[x,x+1]×[y,y+1]` as seen from that side.

## Requirements

### Requirement: Basic check scope
A basic check of cell C over check rectangle K with clip rectangle L SHALL, for every paint plane in ascending plane order, examine every tile of the plane that overlaps K, evaluate rules on its left edge if `LEFT(tile) ≥ K.xbot` and on its bottom edge if `BOTTOM(tile) ≥ K.ybot`, and then run the CIF-layer rules (capability `drc-cif-rules`); every reported error rectangle is clipped to L unless stated otherwise.
Compatibility: exact

- Left edge of `tile`: walk the tiles `tl` that abut the left side of `tile` from bottom to top. For each, the *edge segment* is x = X = LEFT(tile), y ∈ [yb, yt] with `yb = max(BOTTOM(tl), BOTTOM(tile), K.ybot)` and `yt = min(TOP(tl), TOP(tile), K.ytop)`; segments with `yt ≤ yb` are skipped. A = right-side type of `tl`, B = left-side type of `tile`. If A = B the segment is skipped (no rule ever applies to same-type edges).
- Bottom edge of `tile`: walk the tiles `tb` below `tile` from left to right. Segment y = Y = BOTTOM(tile), x ∈ [xl, xr] with `xl = max(LEFT(tb), LEFT(tile), K.xbot)`, `xr = min(RIGHT(tb), RIGHT(tile), K.xtop)`; skip if `xr ≤ xl`. A = top type of `tb`, B = bottom type of `tile`; skip if A = B.
- The rules evaluated are those in the rule list for edge pair (A,B), in list order, skipping rules whose edge plane differs from the plane being scanned (a skipped trigger also skips its triggered rule).
- Hence vertical edges with X ∈ [K.xbot, K.xtop) and horizontal edges with Y ∈ [K.ybot, K.ytop) are examined.
- A tile of the subcell-error type (illegal overlap produced while flattening, see `drc-rule-language`) is itself reported as an error over `clip(rect(tile), K)` (clipped to K, not L) with why index 2.
- If the cell has no DRC style or K is null, no errors are produced.

(Magic ref: drc/DRCbasic.c:583-660, drc/DRCbasic.c:676-725, drc/DRCbasic.c:881-920, drc/DRCbasic.c:1339-1375)

#### Scenario: Edge at the check rectangle boundary
- **GIVEN** a cell with metal1 rectangles [0,10]×[0,10] and [12,20]×[0,10], rule `spacing m1 m1 3 touching_ok "s"`, check rectangle K=[10,30]×[0,10], clip L=K
- **WHEN** the basic check runs
- **THEN** the right edge of the first rectangle (x=10, the left edge of the space tile [10,12]×[0,10]) is examined because LEFT(space tile)=10 ≥ K.xbot
- **AND** the check rectangle is E=[10,13]×[0,13] (top corner extended because the type above-left of the top end is space) and the single error rectangle [12,13]×[0,10] is reported

### Requirement: Standard edge rule check region
For a standard rule (no AREA, MAXWIDTH, RECTSIZE, OFFGRID, ANGLES_90, SPLITTILE flags) on a vertical segment (X, [yb,yt]) the checker SHALL build the check rectangle E as follows and search the rule's check plane for tiles whose type is not in OK (the subcell-error type is never searched).
Compatibility: exact

Pseudo-code (vertical edge; d = dist, c = cdist):

```
E.ybot = yb; E.ytop = yt            # or the triggered sub-range, see Trigger rules
if FORWARD:
    # top corner (always considered)
    if edgeEnds(X, E.ytop) and BottomType(tileAt(X-1, E.ytop)) in CORNER:
        E.ytop += c ; mark NE
    if BOTHCORNERS and edgeEnds(X, E.ybot-1) and TopType(tileAt(X-1, E.ybot-1)) in CORNER:
        E.ybot -= c ; mark SE
    E.xbot = X + (OUTSIDE ? d : 0)
    E.xtop = X + d + (OUTSIDE ? 1 : 0)
else REVERSE:
    if edgeEnds(X, E.ybot-1) and TopType(tileAt(X, E.ybot-1)) in CORNER:
        E.ybot -= c ; mark SW
    if BOTHCORNERS and edgeEnds(X, E.ytop) and BottomType(tileAt(X, E.ytop)) in CORNER:
        E.ytop += c ; mark NW
    E.xbot = X - d - (OUTSIDE ? 1 : 0)
    E.xtop = X - (OUTSIDE ? d : 0)
edgeEnds(X, y) := RightType(tileAt(X-1, y)) != A  or  LeftType(tileAt(X, y)) != B
```

`tileAt(x,y)` is the tile containing the unit square at (x,y) on the edge plane. Horizontal segments (Y, [xl,xr]) use the transposed construction with these differences: FORWARD checks upward (`E.ybot = Y (+d if OUTSIDE)`, `E.ytop = Y + d (+1 if OUTSIDE)`), its default corner is the left end (`TopType(tileAt(xl-1, Y-1))` ∈ CORNER ⇒ `E.xbot -= c`, NW) and the BOTHCORNERS corner is the right end (`TopType(tileAt(xr, Y-1))` ⇒ `E.xtop += c`, NE); REVERSE checks downward (`E.ybot = Y - d (-1 if OUTSIDE)`, `E.ytop = Y (-d if OUTSIDE)`), default corner the right end (`BottomType(tileAt(xr, Y))` ⇒ `E.xtop += c`, SE), BOTHCORNERS the left end (`BottomType(tileAt(xl-1, Y))` ⇒ `E.xbot -= c`, SW). Horizontal edges do NOT apply the `edgeEnds` test, in either `magic-compat` setting (see Q-DRC-6, resolution: replicate). For RUNLENGTH rules, c is 0 on horizontal edges in both modes; on vertical edges c equals the runlength with `magic-compat` ON, as in Magic (it widens the triggered sub-range, extends the corners and feeds the Euclidean radius), and is 0 with it OFF, as on horizontal edges (see Q-DRC-7, resolution: replicate under magic-compat, fix with it off). The run-length filter on candidates applies in both orientations and both modes.

Every tile F on the check plane overlapping E whose type (for split tiles: the type of a side whose triangle intersects E) is not in OK yields a candidate rectangle `R = clip(clip(rect(F), L), E)` (for TRIGGER rules clip to K instead of L); null R is ignored; then the RUNLENGTH and Euclidean filters apply, and the result is reported (or recorded, for triggers). The error rectangle is the full intersection of F's bounding rectangle with E — not the circular or triangular shape.

(Magic ref: drc/DRCbasic.c:1110-1300, drc/DRCbasic.c:1555-1700, drc/DRCbasic.c:285-440)

#### Scenario: Width violation
- **GIVEN** `width poly 2 "w"` and a poly rectangle [0,1]×[0,10] surrounded by space
- **WHEN** the basic check runs with K and L covering everything
- **THEN** at the left edge x=0 (A=space, B=poly) the forward region E=[0,2]×[0,10] is searched for non-poly; the space tile right of x=1 yields the error [1,2]×[0,10]
- **AND** no corner extension occurs because the type above-left of the top end (space) is not in CORNER={poly}

#### Scenario: Spacing with Manhattan corner extension
- **GIVEN** `spacing m1 m1 3 touching_illegal "s"`, m1 at [0,10]×[0,10] and m1 at [11,20]×[12,20]
- **WHEN** the basic check runs
- **THEN** at the right edge of the first shape (x=10, A=m1, B=space, FORWARD) the top end y=10 is an edge end and the type above-left (space) is in CORNER (~m1), so E=[10,13]×[0,13]
- **AND** the second shape's tile overlaps E at [11,13]×[12,13], which is reported as the error rectangle

#### Scenario: Touching OK suppresses check
- **GIVEN** `spacing m1 m1 3 touching_ok "s"` and m1 at [0,10]×[0,10] abutting m1 at [10,20]×[0,5]
- **WHEN** the basic check runs
- **THEN** the plane holds the merged strips [0,20]×[0,5] and [0,10]×[5,10], so no m1|m1 edge exists at x=10
- **AND** no spacing error is reported between the two abutting shapes

#### Scenario: Run-length corner on a vertical edge with magic-compat ON (verify against Magic)
- **GIVEN** `magic-compat` ON, `widespacing m1 10 20 m1 4 touching_ok "wide"` (run-length 20), an m1 block [0,12]×[0,30] and an m1 block [14,40]×[30,60] diagonally above-right of it
- **WHEN** the cell is checked
- **THEN** at the block's right edge x = 12 the triggered spacing rule's top corner is extended by c = 20, so E = [12,16]×[−20,50] and the candidate [14,16]×[30,50] (20 high, passing the run-length filter) is reported
- **AND** at the upper block's left edge x = 14 the bottom corner is extended by 20, so [10,12]×[10,30] is reported

#### Scenario: Run-length corner on a vertical edge with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the layout and rule of the previous scenario
- **WHEN** the cell is checked
- **THEN** c = 0 on the vertical edges, as on horizontal ones, so the check rectangles end at the segment ends and no error is reported, as for the same layout rotated by 90° (golden DRC-WS-RL-H)

### Requirement: Euclidean corner filter
When Euclidean checking is enabled (`drc euclidean on`) and the rule lacks MANHATTAN, every corner that was extended SHALL add a circular exemption: a candidate rectangle R that lies entirely in the outer quadrant of the corner point P, at Euclidean distance ≥ s from P, is not an error, where P is derived from the check rectangle E and s is the corner distance c: with `magic-compat` ON s = c mod 4096, as in Magic, which packs it into 12 bits; with it OFF s = c (see Q-DRC-20, resolution: replicate under magic-compat, fix with it off).
Compatibility: exact

Corner points and tests (E after extension):

| mark | P | applies when | sqx, sqy |
|---|---|---|---|
| NE | (E.xtop − s, E.ytop − s) | | sqx = R.xbot − P.x, sqy = R.ybot − P.y |
| NW | (E.xbot + s, E.ytop − s) | | sqx = P.x − R.xtop, sqy = R.ybot − P.y |
| SW | (E.xbot + s, E.ybot + s) | | sqx = P.x − R.xtop, sqy = P.y − R.ytop |
| SE | (E.xtop − s, E.ybot + s) | | sqx = R.xbot − P.x, sqy = P.y − R.ytop |

R is exempt if for any marked corner `sqx ≥ 0`, `sqy ≥ 0` and `(sqx·k + b)² + (sqy·k + b)² ≥ (s·k + b)²`, where b/k is the contact-cut border (below), normally b=0,k=1. If R is not exempt by that test but F is a split tile whose relevant triangle faces the corner (NW: d=0 left side; NE: d=1 right side; SW: d=1 left side; SE: d=0 right side), F is exempt when the squared distance from P' to F's diagonal segment exceeds s², where P' = (E.xbot+s, E.ytop−s) for NW, (E.xtop−s, E.ytop−s) for NE, (E.xbot+s, E.ybot+s) for SW, (E.xtop−s, E.ybot+s) for SE; the point-to-segment distance uses single-precision arithmetic for the projection term exactly as `a − (c + a − b)²/(4c)` with a, b the squared distances to the endpoints and c the squared segment length (endpoint distances used when the projection falls outside).

Derived borders: if the rule has DERIVED_EDGE and the tile on the inner side of the edge (left/below for FORWARD, right/above for REVERSE) is a non-split contact type with a positive `squares` border in the current mask output style, b += border (in mask units) and k = mask units per internal unit; if the rule has DERIVED_TARGET and F is a non-split contact whose residue on the check plane is in OK and has a positive border, b += its border. See capability `mask-output-operators` for the squares border.

(Magic ref: drc/DRCbasic.c:180-200, drc/DRCbasic.c:221-270, drc/DRCbasic.c:320-410, drc/DRCbasic.c:111-140)

#### Scenario: Euclidean corner pass
- **GIVEN** Euclidean on, `spacing m1 m1 3 touching_illegal "s"`, m1 [0,10]×[0,10] and m1 [12,20]×[12,20]
- **WHEN** checked from the right edge of the first shape (E=[10,13]×[0,13], NE marked, s=3, P=(10,10))
- **THEN** the second shape gives R=[12,13]×[12,13], sqx=2, sqy=2, 4+4=8 < 9, so the error [12,13]×[12,13] is reported

#### Scenario: Euclidean corner exempt
- **GIVEN** Euclidean on, `spacing m1 m1 5 touching_illegal "s"`, m1 [0,10]×[0,10] and m1 [14,30]×[13,30]
- **WHEN** checked from the right edge of the first shape (E=[10,15]×[0,15], NE marked, s=5, P=(10,10))
- **THEN** the candidate R=[14,15]×[13,15] has sqx=4, sqy=3 and 16+9 ≥ 25, so it is exempt and no error is reported

#### Scenario: Manhattan corner with Euclidean disabled
- **GIVEN** Euclidean off and the layout and rule of "Euclidean corner exempt"
- **WHEN** checked
- **THEN** the error [14,15]×[13,15] is reported (Manhattan corner square)

#### Scenario: Corner distance above 4095 with magic-compat ON
- **GIVEN** `magic-compat` ON, Euclidean on, `spacing m1 m1 5000 touching_illegal "s"`, m1 [0,10]×[0,10] and m1 [3600,3700]×[3600,3700]
- **WHEN** checked from the right edge of the first shape (E = [10,5010]×[0,5010], NE marked, c = 5000)
- **THEN** s = 5000 mod 4096 = 904 and P = (4106,4106), so the candidate R = [3600,3700]×[3600,3700] has sqx < 0, is not exempt, and is reported (golden DRC-EUC-5)

#### Scenario: Corner distance above 4095 with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the layout and rule of the previous scenario
- **WHEN** checked
- **THEN** s = 5000 and P = (10,10); the candidate has sqx = sqy = 3590 and 2·3590² ≥ 5000², so it is exempt and no error is reported

### Requirement: Trigger rules
A rule with the TRIGGER flag SHALL be evaluated like a standard rule but, instead of reporting, record each candidate rectangle (clipped to K and E); if none are recorded the following (triggered) rule is skipped, otherwise the triggered rule is evaluated once per recorded rectangle with its segment range replaced by that rectangle's extent along the edge, expanded by the triggered rule's c and clipped to the original segment.
Compatibility: exact

For a vertical edge and recorded rectangle T: `E.ybot = max(T.ybot − c, yb)`, `E.ytop = min(T.ytop + c, yt)`; then corner extensions are evaluated at these ends (they extend only where the sub-range reaches a true edge end). Horizontal edges use x. Recorded rectangles are processed last-recorded first. Errors of the triggered rule are reported normally. Triggers are not evaluated on diagonal edges.

(Magic ref: drc/DRCbasic.c:395-425, drc/DRCbasic.c:1108-1125, drc/DRCbasic.c:1300-1335)

#### Scenario: surround_ok spacing
- **GIVEN** `spacing nwell pdiff 5 surround_ok "s"` with nwell and pdiff on different planes, an nwell edge where, for y∈[0,4], pdiff continues across the edge, and for y∈[4,20] there is no pdiff at the edge
- **WHEN** the edge is checked
- **THEN** the trigger records the non-pdiff part of the 1-unit strip beyond the edge, y∈[4,20]
- **AND** the triggered spacing rule checks y∈[max(4−5,0), 20] = [0,20] for pdiff within 5 units beyond the nwell edge

### Requirement: Wide-spacing trigger (maximal rectangle search)
A trigger rule with MAXWIDTH|BENDS SHALL compute the set of maximal rectangles of OK material of size at least w×w (w = dist, or dist+1 if `option wide-width-noninclusive`) that touch the edge on the material side, and trigger the following spacing rule once per such rectangle; for runlength rules the trigger fires only if the run-length condition holds.
Compatibility: exact

Maximal-rectangle search from a start tile S in direction D with limit w (also used by non-trigger maxwidth-with-bends rules, see below): start with one candidate `Q` derived from rect(S):
- EAST (start = tile right of the edge): Q = [LEFT(S), LEFT(S)+w] × [BOTTOM(S)−(w−1), TOP(S)+(w−1)]
- WEST (start = tile left of the edge): Q = [RIGHT(S)−w, RIGHT(S)] × [BOTTOM(S)−(w−1), TOP(S)+(w−1)]
- NORTH: Q = [LEFT(S)−(w−1), RIGHT(S)+(w−1)] × [BOTTOM(S), BOTTOM(S)+w]
- SOUTH: Q = [LEFT(S)−(w−1), RIGHT(S)+(w−1)] × [TOP(S)−w, TOP(S)]

Then for every tile G on the edge plane overlapping the original Q whose type is not in OK, in area-enumeration order, replace each candidate that overlaps rect(G) by up to four pieces: the part above G if its height ≥ w, below G if height ≥ w, left of G if width ≥ w, right of G if width ≥ w (each piece keeps the candidate's other extents). Candidates not overlapping G are kept. The final candidates are the result (possibly overlapping); an empty result means "none".

Trigger use: REVERSE trigger on a vertical edge searches WEST from the left tile; FORWARD trigger searches EAST from the right tile; horizontal edges use SOUTH/NORTH. If the result is non-empty: when the trigger's c (runlength) ≤ its dist the triggered rule runs for every candidate; otherwise it runs only if the segment length ≥ c and some candidate's extent along the edge is > c. The triggered spacing rule then uses each candidate's extent along the edge (± its own c, clipped to the segment) as its range.

(Magic ref: drc/DRCbasic.c:989-1060, drc/DRCextend.c:513-600, utils/maxrect.c:98-180)

#### Scenario: Wide metal spacing
- **GIVEN** `widespacing m1 10 m1 4 touching_ok "wide"`, an m1 block [0,12]×[0,12] and an m1 line [14,30]×[0,2]
- **WHEN** the right edge x=12 of the block is checked
- **THEN** the block is stored as tiles [0,12]×[0,2] and [0,12]×[2,12]; for the segment y∈[0,2] the WEST search from [0,12]×[0,2] starts with Q=[2,12]×[−9,11] and yields the candidate [2,12]×[0,11]
- **AND** the triggered spacing rule uses y∈[0,2], extends the bottom corner to E=[12,16]×[−4,2], and reports the error [14,16]×[0,2]

#### Scenario: Narrow metal does not trigger
- **GIVEN** the same rule and an m1 block [0,8]×[0,30]
- **WHEN** its right edge is checked
- **THEN** no candidate of width ≥ 10 exists and the wide-spacing rule is not evaluated

### Requirement: Maxwidth rules
A MAXWIDTH rule SHALL be evaluated only on vertical left edges (and, for the BENDS variant, also on bottom edges), and report errors as follows.
Compatibility: exact

- MAXWIDTH|BENDS (non-trigger), w = dist + 1: run the maximal-rectangle search (EAST from the tile right of a vertical edge, NORTH from the tile above a horizontal edge). If the rule has exclude types (check plane ≠ edge plane), discard every candidate overlapping any exclude type on the check plane. Each remaining candidate, clipped to L, is reported (one report per candidate per edge segment). A material region therefore violates when it contains a (dist+1)×(dist+1) square touching the edge.
- MAXWIDTH without BENDS (`bend_illegal`): only on the first examined segment of each tile's left edge: flood-fill from the tile over 4-connected tiles whose facing-side type is in OK, accumulating the bounding box (early stop when both box dimensions exceed dist). Report `clip(rect(tile), L)` if width > dist and height > dist.
- MAXWIDTH_BOTH: as bend_illegal but report if width > dist or height > dist.
- The flood fill is not limited to K; exclude types are ignored for these two variants in both `magic-compat` settings (see Q-DRC-19, resolution: replicate; the tech-file lint warning is specified in `drc-rule-language`).

"First examined segment" means the lowest segment of the tile's left edge whose two types differ and that lies within the scan range; rules are evaluated only if the segment's edge pair carries the rule. A tile whose first such segment borders another type of the same layer set is therefore never a start tile; every region is still evaluated from its leftmost tile, and only start tiles are reported. This holds in both `magic-compat` settings (see Q-DRC-21, resolution: replicate).

(Magic ref: drc/DRCbasic.c:989-1092, drc/DRCbasic.c:1444-1527, drc/DRCextend.c:260-395)

#### Scenario: Slotting rule
- **GIVEN** `maxwidth m1 10 bend_ok "slot"` and an m1 square [0,12]×[0,12]
- **WHEN** the left edge x=0 is checked
- **THEN** w=11 and the EAST candidate [0,11]×[−10,22] is cut by surrounding space to [0,11]×[0,12], which is reported
- **AND** the bottom edge y=0 reports the NORTH candidate [0,12]×[0,11], so the error plane holds their union

#### Scenario: bend_illegal L-shape
- **GIVEN** `maxwidth pmc 4 bend_illegal "pmc"` and pmc forming an L of a 4×10 bar and a 10×4 bar joined at a corner
- **WHEN** checked
- **THEN** the bounding box is 10×10 > 4 in both directions and every pmc tile whose first left-edge segment carries the rule is reported over its whole rectangle

### Requirement: Area rule
An AREA rule SHALL be evaluated once per tile, on the first examined segment of the tile's left edge, by summing the areas of the 4-connected region of tiles whose facing-side type is in OK, and SHALL report `clip(rect(tile), L)` when the total is less than the area limit (cdist, in internal units²).
Compatibility: exact

- Areas are full tile rectangle areas (split tiles count their whole rectangle).
- The fill stops early, reporting nothing, as soon as the running sum reaches the limit or any visited tile has an edge exactly equal to an edge of K (`RIGHT=K.xtop`, `LEFT=K.xbot`, `BOTTOM=K.ybot` or `TOP=K.ytop`); tiles extending beyond K without coinciding are traversed normally. This holds in both `magic-compat` settings (see Q-DRC-8, resolution: replicate).
- The horizon (dist) only affects halo and ordering.

(Magic ref: drc/DRCbasic.c:980-987, drc/DRCextend.c:131-255)

#### Scenario: Small metal island
- **GIVEN** `area m1 20 4 "area"` and an isolated m1 rectangle [0,4]×[0,4] (area 16), K=[−10,14]×[−10,14]
- **WHEN** checked
- **THEN** the error [0,4]×[0,4] is reported

#### Scenario: Island at check boundary
- **GIVEN** the same rule and rectangle but K=[0,20]×[−10,14]
- **WHEN** checked
- **THEN** no area error is reported because the tile's left edge coincides with K.xbot

### Requirement: Rectangle size rule
A RECTSIZE rule SHALL be evaluated on the first examined left-edge segment of a tile only when the type right of the tile's bottom-left left-neighbor and the type above its left-bottom neighbor are not in OK (the tile is the lower-left tile of the region), measuring width W by walking right along the bottom row and height H by walking up the left column while types are in OK, and SHALL report the error rectangle below, clipped to L.
Compatibility: exact

With M = dist and parity p = cdist (−1 any, 0 even, 1 odd), and the tile's lower-left corner (x0,y0):
- W > M ⇒ error [x0, x0+(W−M)] × [y0, y0+H]
- else H > M ⇒ error [x0, x0+W] × [y0, y0+(H−M)]
- else p ≥ 0 and W mod 2 ≠ p ⇒ error [x0, x0+1] × [y0, y0+H]
- else p ≥ 0 and H mod 2 ≠ p ⇒ error [x0, x0+W] × [y0, y0+1]

Non-rectangular shapes are flagged by the companion 1-unit edge rules of `rectangle`.

(Magic ref: drc/DRCbasic.c:1080-1090, drc/DRCextend.c:398-455)

#### Scenario: Odd size required
- **GIVEN** `rectangle via 10 odd "via size"` and a via [0,4]×[0,3]
- **WHEN** checked
- **THEN** W=4 is even, so the error [0,1]×[0,3] is reported

### Requirement: Off-grid rule
An OFFGRID rule with pitch p > 1 SHALL be evaluated on every examined vertical and horizontal segment for which it is listed, reporting the segment snapped outward to the pitch grid when the result is non-null.
Compatibility: exact

Take the degenerate segment rectangle (x-extent zero for vertical edges), clip it to L, then for each coordinate v of each side compute `t = trunc(v / p)·p` (C integer division, truncating toward zero) and set `xbot = min(xbot, t_xbot)`, `xtop = max(xtop, t_xtop)` and likewise for y. Report if non-null. Consequently an edge lying on a grid line (vertical or horizontal) keeps zero width and produces nothing, even if its endpoints are off-grid (the perpendicular edges flag those); an off-grid edge produces an error whose width spans to the nearest grid line toward zero, and whose length is the segment extended to grid lines toward zero. Pitch ≤ 1 never errs. Errors are not re-clipped after snapping. The rounding depends on the `magic-compat` setting (see Q-DRC-9, resolution: replicate under magic-compat, fix with it off): with it ON (the default) `t` truncates toward zero as above, so for negative coordinates the error extends toward the origin; with it OFF `t = floor(v / p)·p`, so negative coordinates mirror positive ones and every error extends toward −∞.

(Magic ref: drc/DRCbasic.c:1093-1101, drc/DRCbasic.c:1528-1536, drc/DRCextend.c:87-125)

#### Scenario: Off-grid vertical edge
- **GIVEN** `off_grid m1 5 "grid"` and m1 [3,10]×[0,10]
- **WHEN** checked
- **THEN** the left edge x=3, y∈[0,10] yields the error [0,3]×[0,10]
- **AND** the right edge x=10 (on grid) yields nothing

#### Scenario: Off-grid edge at negative coordinates with magic-compat ON
- **GIVEN** `magic-compat` ON, `off_grid m1 5 "grid"` and m1 [−8,−3]×[−7,2]
- **WHEN** checked
- **THEN** the left edge x = −8, y∈[−7,2] yields the error [−8,−5]×[−7,2] (xtop widened to `trunc(−8/5)·5 = −5`, toward the origin; `trunc(−7/5)·5 = −5` does not lower ybot), as in golden DRC-OG-1

#### Scenario: Off-grid edge at negative coordinates with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the layout of the previous scenario
- **WHEN** checked
- **THEN** the left edge yields the error [−10,−8]×[−10,2] (with `floor`: xbot = min(−8, −10), xtop = max(−8, −10), ybot = min(−7, −10), ytop = max(2, 0))

### Requirement: Angle rules
For a split tile, if the rule list of (right type, left type) contains an ANGLES_90 rule, the checker SHALL evaluate the first such rule: with ANGLES_45 absent it reports `clip(rect(tile), L)`; with ANGLES_45 present it reports the same rectangle only when the tile's width differs from its height. Rules with ANGLES_45 only SHALL behave as standard edge rules whose candidate tiles are ignored when the candidate tile is split.
Compatibility: exact

(Magic ref: drc/DRCbasic.c:731-745, drc/DRCextend.c:51-80, drc/DRCbasic.c:426-433)

#### Scenario: 45-degree check
- **GIVEN** `angles m1 45 "45 only"` and an m1 split tile 4 wide and 3 high
- **WHEN** checked
- **THEN** the whole tile rectangle is reported as an error
- **AND** a 3×3 split tile is not reported

### Requirement: Diagonal edge checks
For each split tile, processed once, the checker SHALL evaluate the standard and SPLITTILE rules of pair (right type, left type) on the diagonal edge using an offset-triangle search, skipping trigger pairs and nonstandard rules.
Compatibility: exact

Offset: with w, h the tile size, `dx = trunc(d · sqrt(1/(1 + w²/h²)))` (double precision), `dy = trunc(dx·w/h)` (integer), dy negated for d=0 (`/`) tiles, and both negated for FORWARD rules. The search region is the tile rectangle translated by (dx,dy), restricted to the triangle on the side opposite the rule's side; the initiating tile is excluded and candidates are then accepted only if they also intersect the initiating tile's rectangle on its other side. Tiles do not overlap, so this acceptance test never succeeds and diagonal-edge checks never report errors in Magic 8.3.694. The result depends on the `magic-compat` setting (see Q-DRC-10, resolution: replicate under magic-compat, fix with it off, once golden DRC-NM-1..3 confirm that Magic reports nothing across diagonals):
- With it ON (the default), Hefesto reproduces Magic's observed output: no error from the diagonal edge itself.
- With it OFF, Hefesto performs the intended check: every tile F other than the initiating tile whose type on the side facing the diagonal is not in OK and that intersects the translated search triangle is an error, reported as the intersection of F's bounding rectangle with the translated tile rectangle, clipped to L, and filtered as a standard candidate.

Standard cells are Manhattan, so the M3 standard-cell exit check does not depend on this choice. Orthogonal sides of split tiles are checked by the regular left/bottom edge processing using the side types.

(Magic ref: drc/DRCbasic.c:731-850, drc/DRCbasic.c:492-545)

#### Scenario: Diagonal spacing with magic-compat ON (verify against Magic)
- **GIVEN** `magic-compat` ON, `spacing m1 m1 3 touching_ok "s"`, an m1 split tile `/` [0,10]×[0,10] with m1 on its lower-right side, and an m1 square [−2,0]×[3,5] whose corner (0,3) is 3/√2 ≈ 2.1 units from the diagonal
- **WHEN** checked
- **THEN** the reported errors equal those reported by Magic for the same layout (golden DRC-NM-1; per code reading none comes from the diagonal itself)

#### Scenario: Diagonal spacing with magic-compat OFF
- **GIVEN** `magic-compat` OFF and the layout and rule of the previous scenario
- **WHEN** checked
- **THEN** the diagonal edge reports the square, which lies within 3 units of the diagonal, as a spacing error

### Requirement: Exceptions and exemptions at check time
For a rule tagged with an exception or exemption name N, the checker SHALL determine per edge segment whether the segment is inside the mask-hint area stored in the checked cell's property `MASKHINTS_N`, and SHALL skip an exception rule when the segment is not inside and an exemption rule when it is inside; if the property does not exist the segment is not inside.
Compatibility: exact

The inside test depends on the `magic-compat` setting (see Q-DRC-11, resolution: replicate under magic-compat, and Q-MISC-13, which fixes the test with it off). The hint region is held in Magic's canonical decomposition into maximal horizontal strips (hint tiles and space tiles), which the compat test depends on.

With `magic-compat` ON (the default), Magic's search of the hint plane with the zero-width segment is reproduced:
- Vertical segment (X, [yb,yt]): inside when some hint tile covers a unit square (X, y) with y ∈ [yb, yt), i.e. a hint rectangle [l,r)×[b,t) with l ≤ X < r, b < yt and t > yb. Hints whose left boundary is X count; hints whose right boundary is X do not.
- Horizontal segment (Y, [xl,xr]): let T be the hint-plane tile, hint or space, that contains the unit square (xl, Y−1). If TOP(T) ≤ Y (T ends at the segment) the segment is not inside. Otherwise the segment is inside when some hint tile covers a unit square (x, Y−1) with x ∈ [xl, xr); a hint tile whose top is exactly Y counts in this second step. Hence a hint touching the segment only from above never counts, an edge on a hint's bottom or top boundary is normally outside, and the result depends on how the hint plane is cut into tiles: a hint that fully covers the segment can miss it when its strips break at Y at x = xl, and an unrelated hint rectangle with a corner at height Y left of xl can change T.

With it OFF, a segment is inside when the closed hint region contains a part of positive length of the segment, i.e. a hint touching it from either side or straddling it counts, whatever the strip decomposition (the per-orientation rule of Q-DRC-11's compat option is superseded by the Q-MISC-13 fix).

The test is made before the rule's flags are inspected, so it also gates nonstandard rules. In flattened checks the hints of all subcells are flattened into the check buffer (see `drc-engine`).

(Magic ref: drc/DRCbasic.c:921-962, drc/DRCbasic.c:1381-1420)

#### Scenario: Exempted spacing inside SRAM
- **GIVEN** `exemption SRAM` + `spacing m1 m1 3 touching_ok "s"` + `exemption none`, and a cell with property MASKHINTS_SRAM covering [0,100]×[0,100] and two m1 shapes 1 unit apart inside it
- **WHEN** checked
- **THEN** no spacing error is reported
- **AND** the same shapes outside the hint area report the spacing error

#### Scenario: Horizontal edge depends on the hint tiling with magic-compat ON
- **GIVEN** `magic-compat` ON, `exception X` + `width m1 3 "w"` + `exception none`, an m1 wire [0,20]×[0,2], and MASKHINTS_X = [5,15]×[−5,5]
- **WHEN** checked
- **THEN** the bottom edge (Y = 0, x ∈ [0,20]) is inside (T = the space tile [−∞,5]×[−5,5] extends above 0 and the hint covers (5,−1)) and the error [0,20]×[2,3] is reported
- **AND** when MASKHINTS_X also holds [−100,−90]×[0,10], T becomes the space tile [−∞,5]×[−5,0], which ends at Y = 0, and no error is reported (golden MISC-MH-08, DRC-EXC-4)

#### Scenario: Horizontal edge independent of the hint tiling with magic-compat OFF
- **GIVEN** `magic-compat` OFF and both hint variants of the previous scenario
- **WHEN** checked
- **THEN** both report the error [0,20]×[2,3]

#### Scenario: Vertical edge on a hint boundary
- **GIVEN** the rule of the previous scenarios, an m1 wire [0,2]×[0,20], and MASKHINTS_X = [−10,0]×[−10,30] (touching the wire's left edge from the left)
- **WHEN** checked with `magic-compat` ON
- **THEN** no error is reported (the hint does not cover the unit column [0,1))
- **AND** with `magic-compat` OFF the left edge is inside and the error [2,3]×[0,20] is reported (golden DRC-EXC-3)

### Requirement: Error rectangle reporting
Each reported error SHALL consist of a rectangle and the rule that produced it; for continuous checking the rectangle is painted into the cell's error plane (union, so overlapping and duplicate reports merge), and for query commands it is passed with its why index to the reporting function.
Compatibility: exact

Error counts in reports therefore count report events; error-tile counts (see `drc-commands`) count tiles of the resulting error plane in maximal-horizontal-strip form.

(Magic ref: drc/DRCmain.c:148-160, drc/DRCbasic.c:426-433)

#### Scenario: Duplicate reports merge
- **GIVEN** a spacing violation detected from both shapes' edges with overlapping error rectangles
- **WHEN** errors are painted
- **THEN** the error plane contains their union as a single region
