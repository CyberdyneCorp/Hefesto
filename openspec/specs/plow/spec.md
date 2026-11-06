# Plowing (Stretching and Compaction)

## Purpose

Defines Magic's plowing facility: the `plow` and `straighten` commands, the `plowing` technology section, the plowing rule tables derived from the DRC section, and the complete edge-propagation algorithm (initial edges, edge queue, every search rule with its exact search geometry, real-width computation, penumbra outline following, slivers, illegal edges, covered/fixed/drag/contact types, cells, jog-horizon extension, write-back of paint, labels and cells, plow boundaries, and jog straightening). Plowing results are layout geometry; for the same input, technology and parameters Hefesto must produce the same geometry, including where Magic inserts jogs.

Terminology used throughout:

- **Canonical frame.** Plowing is always computed in a frame where the plow moves to the right (+x). Plowing in direction D rotates the edit cell's geometry by T_D (east: identity, north: 90°, west: 180°, south: 270°, Manhattan rotations about the origin as defined in capability `layout-geometry-model`), plows, and maps the result back with the inverse transform. All coordinates in this spec are canonical-frame coordinates unless stated.
- **Working planes.** The computation operates on a copy of the edit cell's own paint (no subcell contents; subcells appear only as use bounding boxes on a separate cell plane), represented per plane as the canonical maximal-horizontal-strip decomposition (capability `layout-geometry-model`). Every tile T carries a *final left coordinate* `trail(T)`, initially `left(T)`. The *final right coordinate* of T is `lead(T) = trail(TR(T))`, where `TR(T)` is the tile adjacent to T's right side at its top-right corner. Tiles never change their original coordinates during the computation; only `trail` values change.
- **Edge.** A vertical boundary segment `E = (plane, x, ybot, ytop, ltype, rtype, newx, flags)` between a tile of type `ltype` (LHS) on its left and `rtype` (RHS) on its right, uniform in both types over `[ybot, ytop)`. `x` is the original position and `newx >= x` the required final position. A cell edge has `ltype = rtype = CELL`, the cell plane, `x` = right side of the use's bounding box and `ybot..ytop` = the bounding box's y-extent.
- `H` = the DRC style halo (maximum distance of any DRC rule, see capability `drc-engine`).
- `W[l][r]` / `S[l][r]` = the list of width / spacing plow rules in bin (LHS type l, RHS type r) (see "Plowing rules derived from DRC"). A plow rule ρ has distance `d(ρ)`, ok-types `OK(ρ)`, outline types `LT(ρ)`, check plane, and flags (width, penumbra-only).
- `maxDist[t]` = the largest distance of any rule in any bin `W[t][*]` or `S[t][*]` (0 if none).
- **Fixed**, **covered**, **drag** and **contact** types are the sets from the `plowing` section (contacts are always fixed).
- *Tile-enumeration order* is the area-enumeration order of capability `layout-geometry-model` ("Tile enumeration order").

## Requirements

### Requirement: plowing technology section
The `plowing` section SHALL contain lines of exactly two fields:

```ebnf
plowing_section = "plowing" , NL , { plow_stmt , NL } , "end" ;
plow_stmt       = ( "fixed" | "covered" | "drag" ) , type_list ;
```

`fixed` types keep their width in the plow direction (e.g. transistors); `covered` types must stay covered by the material attached to their right corners (e.g. a gate must stay covered); `drag` types drag along trailing minimum-width material. Space is removed from every list. After the section, every contact type is automatically added to the fixed set. Errors: wrong field count `Malformed line`, unknown keyword `Illegal keyword "%s".`. Defaults: all three sets empty.

Example (scmos): `fixed allFet,glass,pad` / `covered allFet` / `drag allFet`.

Compatibility: exact
(Magic ref: plow/PlowTech.c:769-895, scmos/scmos.tech.in plowing section)

#### Scenario: Contacts are fixed
- **GIVEN** a technology whose plowing section lists only `fixed allFet`
- **WHEN** the technology is loaded
- **THEN** every contact type (e.g. m2contact) is also treated as fixed-width

### Requirement: Plowing rules derived from DRC
While the DRC section is read, plowing SHALL derive width and spacing rule tables indexed by (LHS type, RHS type) from `width`, `spacing`, `edge` and `edge4way` statements only; every other DRC statement, and these statements with a field count outside width 4, spacing 6, edge/edge4way 8–9, are ignored by plowing. Let `P(X)` be the plane on which all types of set X lie (contacts mapped to their image on that plane; the statement is ignored if there is none) and `T(p)` all types of plane p.

- `width types d`: p = P(types); for every i in `T(p) \ types` and every j in `types` on the same plane as i: rule in `W[i][j]` with distance d, `OK = types`, `LT = T(p) \ types`, check plane p, width flag.
- `spacing A B d touching_ok`: only if P(A) = P(B) = p. Let `R = T(p) \ (A ∪ B)`. For i in A and j in R (i ≠ j, same plane): rule in `S[i][j]` with `OK = T(p) \ B`, `LT = T(p) ∩ (A ∪ B)`; unless A = B, also for i in B and j in R: `OK = T(p) \ A`, same `LT`.
- `spacing A B d touching_illegal`: `R = complement(A)`, `R' = complement(B)`. For i in A, j in R (i ≠ j, same plane), one rule per plane q of P(B), with check plane q, `OK = T(q) \ B`, `LT = T(q) ∩ B` (see Q-LEGACY-101); unless A = B, for i in B, j in R', one rule per plane q of P(A) with `OK = T(q) \ A`, `LT = T(q) ∩ B`.
- `edge A B dist OK C cdist [plane]` and `edge4way ...`: requires P(A) = P(B); `d = max(dist, cdist)`; `p0 = P(C)`; check plane `pc` = the named plane if given, else `p0`; `OKset` = the image of OK. For i in A, j in B: `S[i][j]` with `OK = OKset`, `LT = T(p0) \ C`, check plane `pc`. If `B ≠ C`, for i in A, j in C an extra **penumbra-only** rule in `S[i][j]` with `OK = OKset`, `LT = T(p0) \ B`, check plane `pc`. For `edge4way` additionally, for every i in `T(pc) \ OKset` and j in `OKset`: `S[i][j]` with `LT = {i}`, `OK = T(p0) \ A`, check plane `p0`.
- New rules are prepended to their bin, so a bin lists rules in reverse order of creation.
- After the DRC section, each bin is reduced: scanning candidates from the head, a candidate is deleted when another rule still in the bin has distance >= its distance, equal flags, equal check plane, equal `LT`, and `OK` that is a subset of the candidate's `OK` (of two identical rules the earlier one in the list is deleted). `maxDist[t]` is then computed.
- When the grid is rescaled by n/d, rule distances are scaled with remainder tracking so that repeated scaling is reversible (distance rounded up when not exact).

Compatibility: exact
(Magic ref: plow/PlowTech.c:180-640, plow/PlowTech.c:643-766, plow/PlowTech.c:898-998)

#### Scenario: Width rule bin
- **GIVEN** the DRC rule `width metal1 3` on a plane holding only space, metal1 and m2contact
- **WHEN** plowing rules are derived
- **THEN** bin (space, metal1) contains a width rule of distance 3 whose ok-types are {metal1} and whose outline types are {space, m2contact}

#### Scenario: Duplicate rules collapse
- **GIVEN** `spacing metal1 metal1 3 touching_ok` appearing twice
- **WHEN** the bins are reduced
- **THEN** bin (metal1, space) contains exactly one spacing rule of distance 3

### Requirement: plow command
The `plow` command SHALL take a first argument matched case-insensitively as a unique prefix of: `boundary`, `help`, `horizon`, `jogs`, `selection`, `straighten`, `noboundary`, `nojogs`, `nostraighten`; if it matches none it is parsed as a Manhattan direction name (`north`, `south`, `east`, `west`, `up`, `down`, `left`, `right` and their accepted abbreviations), transformed from root to edit-cell orientation, meaning "plow in that direction". Behaviour:

| Form | Effect / output |
|---|---|
| `plow` *dir* `[layers]` | Plow the box (see "Box plow initial edges"); `layers` default `*,l,subcell,space`. Prints `Reduced plow size to stay within the boundary.` if a boundary limited it and moves the box accordingly. |
| `plow boundary` | Set the plow boundary to the box (one boundary at a time). |
| `plow noboundary` | Remove the boundary. |
| `plow horizon [n]` | Set (if n given) and print the jog horizon: `Jog horizon set to %d units.` or `Jog horizon set to infinity.` |
| `plow jogs` | Horizon = 0, prints `Jog insertion re-enabled (horizon 0).` |
| `plow nojogs` | Horizon = infinity, prints `Jog insertion disabled.` |
| `plow straighten` / `nostraighten` | Enable/disable automatic straightening after each plow; prints `Jogs will be straightened after each plow.` / `Jogs will not be straightened automatically.` |
| `plow selection [dir [amount]]` | Plow the selection (see "Selection plow"). |
| `plow help` | Lists the options and `Option may also be any Manhattan direction, which` / `causes the plow to be moved in that direction.` |

Defaults: horizon 0 (edges are never extended, so jogs are inserted freely), straightening off, no boundary. Infinity is the integer `2^30 - 4`. Errors: ambiguous option `Ambiguous plowing option: "%s"`; invalid `"%s" isn't a valid plow option.` + `  Type "plow help" for help.`; wrong argument count `Wrong number of arguments to %s option.`; no window `Point to a window first`; no edit cell `There is no edit cell!`.

Compatibility: behavioral
(Magic ref: plow/PlowCmd.c:84-330, plow/PlowMain.c:55-62)

#### Scenario: Disable jogs
- **WHEN** `plow nojogs` is executed
- **THEN** `Jog insertion disabled.` is printed and subsequent plows extend every moving edge to its natural jog (see "Jog horizon extension")

#### Scenario: Ambiguous option
- **WHEN** `plow no` is executed
- **THEN** `Ambiguous plowing option: "no"` is reported

### Requirement: Working representation and final geometry
The plow computation SHALL operate on working planes as defined in Terminology and derive the result from the final coordinates:

1. Initially every tile has `trail(T) = left(T)` and every cell use has its move *unset*.
2. Moving an edge E (after its rules have been applied): if the topmost left-side tile along E has `lead < E.newx`, the topmost left-side and right-side tiles are split horizontally at `E.ytop` where they extend above it; every right-side tile along `[E.ybot, E.ytop)` with `trail < E.newx` gets `trail = E.newx` (the bottom right-side tile is first split at `E.ybot` if it extends below it); each touched tile is merged vertically with its upper/lower neighbour when both have the same type, left, right, `trail` and `lead`.
3. The final geometry of a plane is the union over non-space tiles T of `[trail(T), lead(T)] × [bottom(T), top(T)]` with type T.
4. Every search primitive below operates on the current (split) tiling, not on a re-canonicalized one, and uses `trail`/`lead` where stated and original coordinates otherwise.

Compatibility: exact
(Magic ref: plow/PlowMain.c:1832-2042, plow/plowInt.h:160-170, plow/PlowYank.c:537-570)

#### Scenario: Moving edges stretch and shift a tile
- **GIVEN** metal1 `(10,0)-(13,20)`, the edge space|metal1 at x=10, y 0..20 moved to 20, and the edge metal1|space at x=13 moved to 23
- **WHEN** both edges are moved
- **THEN** the metal1 tile has trail 20 and lead 23 and the final geometry is metal1 `(20,0)-(23,20)`

### Requirement: Shadow search and atomize primitives
The forward shadow search of area A with transparent set OKT on plane p SHALL visit the tiles containing points of A's left side (`x = A.xbot`) from top to bottom; for such a tile T, walk the tiles adjacent to T's right side from top to bottom, restricted to the y-interval of A not yet covered; for each such right neighbour R:

- if `type(R) ∉ OKT`, report the edge `(x = left(R), ltype = type(T), rtype = type(R), newx = trail(R))` over the y-overlap and mark that interval covered (the region behind it is in shadow);
- else if `right(R) >= A.xtop`, mark the interval covered;
- else continue the same walk from R over that interval.

A tile T whose right side is at or beyond `A.xtop` covers its interval without reporting. Hence only edges with `A.xbot < x < A.xtop` are reported, at most one per y, the first non-transparent edge seen from the left. The client may change the reported `newx` and may stop the search.

The **initial** shadow search differs only in its report condition: report when `type(T) ≠ type(R)` and (`type(R) ∉ OKT` or `type(T) ∉ OKT`).

The **backward** shadow search mirrors the forward one: it visits the tiles containing points of `x = A.xtop - 1` from bottom to top, walks left neighbours from bottom to top and reports the first edge whose LHS type is not in OKT (`x = right(L)`, `ltype = type(L)`, `rtype` = type of the tile being walked, `newx = trail` of that tile); only edges with `A.xbot < x < A.xtop` are seen.

**Atomizing** a rectangle Q (geometric edge at `x = Q.xbot`, target `Q.xtop`) on plane p SHALL walk the tiles R whose left side lies on `x = Q.xbot` from top to bottom over `[Q.ybot, Q.ytop)`, and for each R with `trail(R) < Q.xtop` report one edge per left neighbour L of R overlapping the interval (bottom to top), with `newx = Q.xtop`.

Compatibility: exact
(Magic ref: plow/PlowSearch.c:150-645)

#### Scenario: Shadow stops at the first opaque edge
- **GIVEN** on one plane metal1 `(10,0)-(13,10)` and metal1 `(20,0)-(23,10)`, and a forward shadow search of `(5,0)-(30,10)` with transparent set {space}
- **WHEN** the search runs
- **THEN** exactly one edge is reported, at x = 10 (space|metal1), because everything to its right is in its shadow

### Requirement: Outline tracing primitive
Outline tracing of the region of inside-types IN on plane p, from a start point on the region's boundary with an initial direction north or south, SHALL follow the boundary counter-clockwise around the IN region, i.e. with the inside always on the left of the direction of travel, producing a sequence of segments. A segment ends wherever the inside tile or the outside tile along it changes (tiles of the current tiling) or the boundary turns. At the end of a segment, let `aheadL` be the tile just ahead on the left (inside) side and `aheadR` the tile just ahead on the right (outside) side: if `aheadL` is not inside the boundary turns left; else if `aheadR` is inside it turns right; else it continues straight. The first segment: heading north from P, the left tile is the one at `(P.x - 1, P.y)` and the right tile the one at `P`; heading south, the right tile is at `(P.x - 1, P.y - 1)` and the left tile at `(P.x, P.y - 1)`; if the left tile is not inside, the first segment turns left (north→west, south→east); else if the right tile is inside it turns right (north→east, south→west); else it goes straight. For each segment the client sees its rectangle (zero width or zero height), its direction, the previous and next directions, and the inside and outside tiles; clients are called only for directions in their direction mask, but every segment is traced. Tracing continues until the client stops it.

To trace clockwise around a set X (lower penumbra and similar), the outline of the complement of X is traced counter-clockwise, so the X material is the outside tile.

Compatibility: exact
(Magic ref: plow/PlowSearch.c:660-1317)

#### Scenario: Turn west at the top of a wire
- **GIVEN** metal1 `(0,0)-(3,10)` alone and an outline trace of {metal1} starting at `(3,10)` heading north
- **WHEN** the first segment is produced
- **THEN** it heads west along y = 10 because the tile at `(2,10)` is space

### Requirement: Box plow initial edges
Plowing the box in direction D SHALL treat the box side opposite to D as the plow and sweep it to the side in direction D. In the canonical frame let the box be `B = [bx0, bx1] × [by0, by1]`; if `bx0 = bx1` nothing happens. Let `R = [bx0 - 1, bx1] × [by0, by1]`. The working planes are loaded with R expanded by H on every side. The *plow distance* used for clamping is `bx1 - (bx0 - 1)`, i.e. the box width plus 1 (see Q-LEGACY-98). Initial edges, all flagged *initial*:

- On every paint plane, an initial shadow search of R with transparent set = the complement of the layer set queues each reported edge with `newx = bx1`. With the default layers (all paint types and space) every type is opaque, so for each y only the first type change at `bx0 <= x < bx1` is queued.
- If `subcell` is in the layer set, for every use whose bounding box U overlaps B: if `U.xbot < bx0` and `U.xtop >= bx1` it is not moved; else if `U.xbot < bx0` it is dragged by its front, `move = bx1 - U.xtop`; otherwise it is pushed, `move = bx1 - U.xbot`. Its cell edge is queued with `newx = U.xtop + move`.

Compatibility: exact
(Magic ref: plow/PlowMain.c:491-556, plow/PlowMain.c:660-800, plow/PlowMain.c:1425-1520, plow/PlowSearch.c:293-422)

#### Scenario: Plow right across a wire
- **GIVEN** a metal1 rectangle (10,0)-(13,20) in an otherwise empty cell, DRC `width metal1 3`, and the box (5,0)-(20,20)
- **WHEN** `plow right` is executed
- **THEN** the initial edge space|metal1 at x = 10 is queued with newx 20, the umbra width rule queues the edge at x = 13 with `20 + min(3, 13 - 10) = 23`, and the metal1 rectangle becomes (20,0)-(23,20)

### Requirement: Edge queue ordering and merging
The edge queue SHALL hold one bin per (plane, original x). Queuing an edge E first clamps `E.newx` to at most `E.x + plowDistance`. Then:

- **Cell edges** (cell plane): if the bin already holds an edge for the same use, its `newx` becomes the maximum of both; otherwise E is prepended to the bin.
- **Paint edges**: a bin is kept sorted by increasing `ybot` with disjoint y-ranges. Pieces of E overlapping an existing edge with the same (ltype, rtype) split both so that every overlapped y-interval keeps the larger `newx`; pieces not overlapped become new edges carrying E's flags; overlapped pieces keep the existing edge's flags. Vertically touching neighbours with the same (ltype, rtype) and equal `newx` are merged as they are encountered during insertion.

Removal for plowing takes the non-empty bin with the smallest x over all planes; ties between planes go to the lowest plane index (the cell plane precedes all paint planes, paint planes in technology order); within a bin the first edge is taken (lowest `ybot` for paint; most recently prepended for cells). A popped edge with `newx = x` is ignored. Straightening uses the same structure but removes from the bin with the largest x (ties: lowest plane index).

Compatibility: exact
(Magic ref: plow/PlowQueue.c:75-120, plow/PlowQueue.c:154-445, plow/PlowQueue.c:458-588)

#### Scenario: Overlapping queued edges split by maximum
- **GIVEN** a queued edge space|metal1 at x = 10, y 0..10, newx 15
- **WHEN** an edge space|metal1 at x = 10, y 5..20, newx 18 is queued
- **THEN** the bin holds y 0..5 with newx 15 followed by y 5..20 with newx 18

#### Scenario: Already moved edge skipped
- **GIVEN** an edge queued twice with newx 20 and 18
- **WHEN** the queue is processed
- **THEN** the edge moves once to x = 20

### Requirement: Edge processing
The main loop SHALL pop edges as specified and, for each edge E with `newx > x` that is not skipped by the boundary check ("Plow boundary"), process it:

```
if E is a cell edge:
    m = E.newx - E.x
    if m > move(use)            (an unset move compares as in Q-LEGACY-97)
        move(use) = m; changed ∪= rect(E) ∪ (use bbox translated by m)
        apply the cell rule ("Cell rules")
    return
if every right-side tile along E already has trail >= E.newx: return
if jogHorizon > 0: extend E ("Jog horizon extension")
changed ∪= [E.x, E.newx] × [E.ybot, E.ytop]
apply the search rules in table order ("Plow search rules")
move E ("Working representation and final geometry")
```

When the queue is empty, the changed area (canonical frame) is grown by 1 on every side and written back ("Result write-back"); nothing is written if it is empty.

Compatibility: exact
(Magic ref: plow/PlowMain.c:1521-1650, plow/PlowMain.c:558-660)

#### Scenario: Edge with no remaining work
- **GIVEN** an edge whose right-side tile already has trail 25 and a popped copy with newx 22
- **WHEN** it is processed
- **THEN** no rule is applied and the geometry is unchanged

### Requirement: Plow search rules
For each processed paint edge E (LHS l, RHS r) the following rules SHALL be applied in exactly this order, each only when l is in its LHS set and r in its RHS set; the rule list each rule receives is in the last column (`Wreal` = the real-width list of "Real width", `Wmin = W[l][r]`, `S = S[l][r]`):

| # | Rule | LHS set | RHS set | Rules |
|---|---|---|---|---|
| 1 | Clear umbra | all | all but space | none |
| 2 | Umbra width | LHS of any width bin | RHS of any width bin | Wreal |
| 3 | Umbra spacing | LHS of any spacing bin | RHS of any spacing bin | S |
| 4 | Top penumbra width | as 2 | as 2 | Wreal |
| 5 | Top penumbra spacing | as 3 | as 3 | S |
| 6 | Bottom penumbra width | as 2 | as 2 | Wreal |
| 7 | Bottom penumbra spacing | as 3 | as 3 | S |
| 8 | Top fixed-RHS penumbra | all | fixed | runs only if S is empty |
| 9 | Bottom fixed-RHS penumbra | all | fixed | runs only if S is empty |
| 10 | Top width slivers | as 2 | as 2 | Wmin |
| 11 | Top spacing slivers | as 3 | as 3 | S |
| 12 | Bottom width slivers | as 2 | as 2 | Wmin |
| 13 | Bottom spacing slivers | as 3 | as 3 | S |
| 14 | Inside slivers | not fixed | not fixed | none |
| 15 | Top illegal edges | all | all | none |
| 16 | Bottom illegal edges | all | all | none |
| 17 | Top covering | covered | all | none |
| 18 | Bottom covering | covered | all | none |
| 19 | LHS fixed | fixed | all | none |
| 20 | RHS fixed | all | fixed | none |
| 21 | Drag stubs | neither drag nor space | drag | none |
| 22 | LHS contact | contact | all | none |
| 23 | RHS contact | all | contact | none |
| 24 | Find cells | all | all | none |

A rule whose list is empty does nothing. Unless stated otherwise, impacted edges are queued with the **impact formula**: for an edge F reported by a search on behalf of E with rule distance d (0 for rules without a list),

```
newx(F) = E.newx + min(d, F.x - E.x)
queue F only if newx(F) > F.newx (the trail of F's right tile)
```

so existing separations smaller than d are preserved, never increased. The penumbra-only flag is honoured only by the inside-sliver rule (see Q-LEGACY-100).

Compatibility: exact
(Magic ref: plow/PlowMain.c:2045-2200, plow/PlowMain.c:1652-1743, plow/PlowRules1.c:1005-1028)

#### Scenario: Spacing preserved, not increased
- **GIVEN** DRC `spacing metal1 metal1 3 touching_ok`, metal1 boxes A=(0,0)-(3,10) and B=(5,0)-(8,10) (existing separation 2), and A's right edge moving from x=3 to x=10
- **WHEN** the umbra spacing rule is applied
- **THEN** B's left edge is queued with `newx = 10 + min(3, 5 - 3) = 12`

### Requirement: Real width
For rules 2, 4 and 6 the rule list SHALL be a copy of `W[l][r]` in which each rule's distance is replaced by `max(d, w)`, where w is the width of material of the rule's ok-types found from E by this greedy procedure (A is a working rectangle; `bboxR` is the right side of the edit cell's bounding box in the canonical frame):

```
A = [E.x, bboxR + 1] × [E.ybot, E.ytop]
T = first tile not of OK in A, in tile-enumeration order
if T exists:
    u = left(T) - E.x
    A.xtop = left(T); A.ytop = max(E.ybot + u, E.ytop); A.ybot = min(E.ytop - u, E.ybot)
loop:
    T = first tile not of OK in A, in tile-enumeration order; stop if none
    xw = left(T) - A.xbot
    if bottom(T) < E.ytop and top(T) > E.ybot:     A.xtop = left(T); go to clipV
    if bottom(T) >= E.ytop:
        if xw >= bottom(T) - A.ybot:                A.xtop = left(T); go to clipV
        else:                                      A.ytop = bottom(T); go to clipR
    else:
        if xw >= A.ytop - top(T):                   A.xtop = left(T); go to clipV
        else:                                      A.ybot = top(T); go to clipR
  clipV: yt = min(E.ybot + xw, A.ytop); yb = max(E.ytop - xw, A.ybot)
         if yt > E.ytop: A.ytop = yt
         if yb < E.ybot: A.ybot = yb
         go to next
  clipR: if A.ytop - A.ybot < A.xtop - A.xbot: A.xtop = A.xbot + (A.ytop - A.ybot)
  next:  stop if A.xbot = A.xtop
w = min(A.xtop - A.xbot, A.ytop - A.ybot)
```

The leftward variant used by straightening mirrors this procedure: start rectangle `[bboxL - 1, E.x] × [E.ybot, E.ytop]` where `bboxL` is the left side of the bounding rectangle given, using right sides of offending tiles and clipping `A.xbot`.

Compatibility: exact
(Magic ref: plow/PlowMain.c:1745-1830, plow/PlowWidth.c:65-558)

#### Scenario: Wide wire keeps its width
- **GIVEN** DRC `width metal1 3`, metal1 `(10,0)-(18,20)` (8 wide) alone, and its left edge moving from 10 to 20
- **WHEN** the umbra width rule is applied
- **THEN** the real width is `min(8, 20) = 8`, the search reaches x 28, and the right edge at 18 is queued with `newx = 20 + min(8, 8) = 28`

### Requirement: Umbra rules
Rule 1 (clear umbra) SHALL forward-shadow-search `[E.x, E.newx] × [E.ybot, E.ytop]` on E's plane with transparent set `{r}` and apply the impact formula with d = 0: anything in the swept area that is not the RHS type is pushed to `E.newx`.

Rules 2 and 3 SHALL, for each rule ρ in their list in list order, forward-shadow-search `[E.x, E.newx + d(ρ)] × [E.ybot, E.ytop]` on ρ's check plane with transparent set `OK(ρ)` and apply the impact formula with `d(ρ)`.

Compatibility: exact
(Magic ref: plow/PlowRules1.c:74-150)

#### Scenario: Clear umbra pushes what is swept on the same plane
- **GIVEN** metal1 `(10,0)-(13,10)`, poly `(15,0)-(17,10)` on another plane, and the edge space|metal1 at x = 10 moving to 20
- **WHEN** the clear umbra rule runs
- **THEN** the metal1|space edge at 13 is queued with newx 20 and the poly is not affected by this rule

### Requirement: Penumbra rules
Rules 4–7 SHALL search above and below E, following the outline of each rule's outline types. For each rule ρ (distance d) in list order:

**Top (rules 4, 5).** Clip point `(cx, cy) = (E.newx + d, E.ytop + d)`. Trace the outline of `LT(ρ)` from `(E.x, E.ytop)` heading north; the client is called for west, north and south segments s:

```
if s.dir = south or s.xbot >= cx: stop
A = rect(s); last = false
if A.ytop >= cy: A.ytop = cy; last = true
A.xtop = E.newx + d
if s.dir = west:
    if s.ytop < cy:
        forward-shadow-search [s.xtop - 1, E.newx + d] × [s.ytop, cy] with OK(ρ), applying the
        impact formula only to impacted edges whose LHS type is in OK(ρ)
    stop
forward-shadow-search A with OK(ρ), impact formula with d
if last: stop
```

**Bottom (rules 6, 7).** Clip point `(E.newx + d, E.ybot - d)`. Trace the outline of the complement of `LT(ρ)` from `(E.x, E.ybot)` heading south (client for west, north and south segments), with the mirrored client: stop on a north segment or when `s.xbot >= cx`; clip `A.ybot` at `cy` (last when `s.ybot <= cy`); on a west segment with `s.ybot > cy` search `[s.xtop - 1, E.newx + d] × [cy, s.ybot]` with the same LHS-type filter, then stop.

Compatibility: exact
(Magic ref: plow/PlowRules1.c:150-470)

#### Scenario: Diagonal neighbour inside the top penumbra
- **GIVEN** `spacing metal1 metal1 3 touching_ok`, metal1 A `(0,0)-(3,10)` whose right edge moves from 3 to 10, and metal1 C `(5,11)-(8,20)`
- **WHEN** the top penumbra spacing rule runs
- **THEN** the outline turns west at `(3,10)`, the search `[2,13] × [10,13]` finds C's left edge, and only the piece y 11..13 of it is queued with `newx = 10 + min(3, 5 - 3) = 12`

### Requirement: Fixed-RHS penumbra rules
Rules 8 and 9 SHALL run only when r is fixed and `S[l][r]` is empty. Top: let t be the type of the tile at `(E.x - 1, E.ytop)`; for each rule ρ in `S[l][t]`, forward-shadow-search `[E.x - 1, E.newx] × [E.ytop, E.ytop + d(ρ)]` on ρ's check plane with `OK(ρ)` and apply the impact formula with d = 0 (impacted material is pushed to `E.newx`). Bottom: t is the type at `(E.x - 1, E.ybot - 1)` and the area is `[E.x - 1, E.newx] × [E.ybot - d(ρ), E.ybot]`.

Compatibility: exact
(Magic ref: plow/PlowRules2.c:190-270)

#### Scenario: Material above the LHS of a moving fixed type
- **GIVEN** one plane with types ndiff, nfet; `spacing ndiff ndiff,nfet 3 touching_ok` (so `S[ndiff][nfet]` is empty and `S[ndiff][space]` holds one rule of distance 3 with ok-types excluding ndiff); `fixed nfet`; ndiff `(0,0)-(8,4)`, nfet `(8,0)-(12,4)`, ndiff `(11,6)-(20,9)`; and the edge ndiff|nfet at x = 8, y 0..4 moving to 13
- **WHEN** the top fixed-RHS penumbra rule runs
- **THEN** the area `[7,13] × [4,7]` is searched and the piece y 6..7 of the space|ndiff edge at x = 11 is queued with newx 13

### Requirement: Sliver rules
Rules 10–13 SHALL prevent the sliding LHS from leaving a sliver narrower than a rule distance between E's LHS type and material above (top) or below (bottom) the edge. They do nothing if `maxDist[l] = 0`. Top version, for each rule ρ of the list (only `LT(ρ)` is used from ρ):

```
clip = (E.newx, E.ytop + maxDist[l]); slivType = none; lastx = mustMove = E.x
pass 1: trace the outline of LT(ρ) from (E.x, E.ytop) heading north; client for N, E, S segments:
  south: stop
  north: nx = trail(outside tile); if nx < lastx: stop
         last = (nx >= clip.x or s.ytop >= clip.y)
         if s.ybot = E.ytop: slivType = type(outside tile)
         if prevDir = west or (prevDir = north and nx > lastx):
             lastx = nx; xmove = min(nx, clip.x); go to check
         lastx = nx; continue (stop if last)
  east:  last = (s.xtop >= clip.x)
         if slivType = none: continue (stop if last)
         xmove = clip.x
         if nextDir = north and trail(next outside tile) < xmove: xmove = that trail
  check: farDist = s.ybot - E.ytop; far = type(inside tile)
         if some rule in W[l][slivType] or in S[l][slivType] has distance > farDist and far ∉ its OK:
             mustMove = xmove
         stop if last
pass 2 (only if mustMove > E.x): trace the same outline again; client for N and S segments:
  if s.dir = south or trail(outside tile) >= mustMove: stop
  queue edge (rect(s), ltype = type(inside), rtype = type(outside), newx = mustMove, but at most
              s.x + (E.newx - E.x) when rtype is fixed)
```

The bottom version traces the complement of `LT(ρ)` from `(E.x, E.ybot)` heading south with clip `(E.newx, E.ybot - maxDist[l])`, mirrored: stop on north; the vertical-segment final x is `trail(inside tile)`; slivType is the inside tile's type when `s.ytop = E.ybot`; `last` when `s.ybot <= clip.y`; the special case uses `prevDir = south`; on east segments the next-direction test uses south and the next inside tile; `farDist = E.ybot - s.ytop` and far = the outside tile's type; pass 2 stops on north or `trail(inside) >= mustMove` and queues ltype = outside, rtype = inside.

Compatibility: exact
(Magic ref: plow/PlowRules1.c:470-1000)

#### Scenario: No sliver when the outline goes straight up (verify against Magic)
- **GIVEN** `width metal1 3`, metal1 C `(5,11)-(8,20)` whose left-edge piece y 11..13 moves from 5 to 12
- **WHEN** the top width sliver rule runs for that piece
- **THEN** pass 1 sees the north segment x = 5, y 13..20 with trail 5 = lastx, records slivType metal1, finds no sliver, and nothing is queued, so C keeps x 5..8 above y 13 (a jog is inserted)

### Requirement: Inside sliver rule
Rule 14 SHALL apply only to edges flagged initial whose height `h = E.ytop - E.ybot` is less than H. Top case: enumerate, in tile-enumeration order, the tiles of the one-unit strip `[E.x - 1, E.newx] × [E.ytop, E.ytop + 1]`:

```
t0 = none
for each tile T:
  if t0 = none:
      t0 = type(T); X = [E.x, min(E.newx, lead(T))]
      if lead(T) >= E.newx: scan(t0); stop
  elif type(T) = t0:
      X.xtop = max(X.xtop, min(E.newx, lead(T))); if lead(T) >= E.newx: scan(t0); stop
  else:
      t1 = type(T)
      if (l ≠ space and r ≠ space) or t0 covered or t1 covered or t0 ≠ l or t1 ≠ r:
          scan(t0); stop
      scan(t0); X = [X.xtop, E.newx]; scan(t1); stop
scan(t): for each rule ρ in S[t][l], then in W[t][l], skipping penumbra-only rules and rules with d(ρ) <= h:
      Y = [E.ytop - d(ρ), E.ybot]
      for each tile T' not of OK(ρ) overlapping X × Y on ρ's check plane (ORIGINAL coordinates,
      see Q-LEGACY-102), in tile-enumeration order:
          if left(T') >= X.xbot: atomize [left(T'), E.newx] × ([bottom(T'), top(T')] ∩ Y)
```

The bottom case uses the strip `[E.x - 1, E.newx] × [E.ybot - 1, E.ybot]` and `Y = [E.ytop, E.ybot + d(ρ)]`.

Compatibility: exact
(Magic ref: plow/PlowRules3.c:70-401)

#### Scenario: Tall initial edge is exempt
- **GIVEN** H = 6 and an initial edge of height 10
- **WHEN** the inside sliver rule is considered
- **THEN** it does nothing

### Requirement: Illegal edge rules
Rules 15–16 SHALL prevent E's LHS type from becoming horizontally adjacent to a type it may not touch. A pair (l, b) is *illegal* if some DRC rule triggered by an l|b edge in the current DRC style has an ok-set not containing b. Top version:

```
clip.x = E.newx; bad = none
trace the outline of (all types except r) from (E.x, E.ytop) heading north; client for all directions:
   if s.dir ≠ east or s.xbot >= clip.x: stop
   b = type(inside tile)                     # material lying on top of the RHS material
   if (l, b) is not illegal: continue
   if left(inside tile) < E.x: continue        # the violation already existed
   bad = b; mustMove = s.xbot
   lt = type of the left neighbour of the inside tile at its bottom
   wdt = max(1, max d(ρ) over ρ in S[l][lt] with b ∉ OK(ρ)); clip.y = E.ytop + wdt; stop
if bad ≠ none:
   trace the outline of (all types except bad) from (mustMove, E.ytop) heading north; client for W, N, S:
      if s.dir ≠ north or s.xbot >= clip.x: stop
      piece = rect(s) with ytop clipped to clip.y (stop after this piece if clipped)
      if piece is nonempty and trail(outside tile) < E.newx:
          queue (piece, ltype = type(inside), rtype = type(outside), newx = E.newx)
```

The bottom version traces the outline of `{r}` from `(E.x, E.ybot)` heading south, takes b = the outside tile's type, lt = the type of the outside tile's left neighbour at the segment's y, `clip.y = E.ybot - wdt`; its second phase traces the outline of `{bad}` heading south, handles south segments with `ybot` clipped at `clip.y`, tests `trail(inside tile)` and queues ltype = outside, rtype = inside.

Compatibility: exact
(Magic ref: plow/PlowRules2.c:545-850)

#### Scenario: Legal neighbour is ignored
- **GIVEN** an edge whose RHS material is covered above only by types that are legal next to its LHS type
- **WHEN** the top illegal edge rule runs
- **THEN** no edge is queued

### Requirement: Covered, fixed, drag and contact rules
The remaining paint rules SHALL behave as follows, with `δ = E.newx - E.x`:

- **Top covering (17)**: let t be the type at `(E.x - 1, E.ytop)`; if t is space do nothing; for each rule ρ in `W[l][t]` then `S[l][t]`, forward-shadow-search `[E.x - 1, E.newx] × [E.ytop, E.ytop + d(ρ)]` on E's plane with `OK(ρ)` and apply the impact formula with d = 0. **Bottom covering (18)**: t at `(E.x - 1, E.ybot - 1)`, area `[E.x - 1, E.newx] × [E.ybot - d(ρ), E.ybot]`.
- **LHS fixed (19)**: for each left-side tile L along E (bottom to top), atomize `[left(L), left(L) + δ] × [bottom(L), top(L)]` if `trail(L) < left(L) + δ`.
- **RHS fixed (20)**: for each right-side tile R along E (top to bottom): atomize `[right(R), right(R) + δ] × [bottom(R), top(R)]` if `lead(R) < right(R) + δ`; then for every fixed-type tile F touching R's top (right to left) and then R's bottom (left to right), atomize `[left(F), left(F) + δ] × [bottom(F), top(F)]` if `trail(F) < left(F) + δ`.
- **Drag stubs (21)**: for each left-side tile L along E (bottom to top) with `trail(L) < left(L) + δ`, atomize `[left(L), left(L) + δ] × [bottom(L), top(L)]`, queuing an atomized edge G only if `G.ltype` is space, `G.x + H >= E.x`, and `E.x - G.x <= w`, where w is the minimum of the distances of the rules in `W[space][G.rtype]` and of the rules ρ in `S[r][l]` with space ∉ `OK(ρ)` (nothing is queued if there is no such rule).
- **Contact LHS/RHS (22, 23)**: for every other plane connected by the contact type (l for 22, r for 23), atomize `[E.x, E.newx] × [E.ybot, E.ytop]` on that plane.

Compatibility: exact
(Magic ref: plow/PlowRules2.c:60-190, plow/PlowRules2.c:270-545)

#### Scenario: Covered gate drags its poly
- **GIVEN** one plane with types poly, ndiff, nfet; `width poly 2`; `covered nfet`; nfet `(8,0)-(10,4)` with poly `(8,4)-(10,6)` above it; and the edge nfet|ndiff at x = 10, y 0..4 moving to 15
- **WHEN** the top covering rule runs
- **THEN** the search `[9,15] × [4,6]` with ok-types {poly} finds the poly|space edge at x = 10 and queues it with newx 15

#### Scenario: Drag stub
- **GIVEN** `width poly 2` (on the plane of poly and nfet), `drag nfet`, poly `(6,0)-(8,4)` left of nfet `(8,0)-(12,4)`, and the edge poly|nfet at x = 8 moving to 13
- **WHEN** the drag stubs rule runs
- **THEN** the space|poly edge at x = 6 is queued with newx 11 because `8 - 6 = 2 <= 2`

#### Scenario: Contact couples planes
- **GIVEN** an m2contact `(10,0)-(14,4)` and its right edge on the metal1 plane moving from 14 to 20
- **WHEN** rules 19 and 23 run
- **THEN** the contact's left edge at 10 is atomized with newx 16 on the metal1 plane and the edges at x = 14 on the metal2 plane are queued with newx 20

### Requirement: Cell rules
Subcell uses SHALL be moved as rigid units:

- **Find cells (rule 24)**: for each use U overlapping `[E.x - 1, E.newx + H] × [E.ybot - H, E.ytop + H]`: if `U.xbot <= E.x`, `m = E.newx - E.x` (dragged); else `m = E.newx + min(U.xbot - E.x, H) - U.xbot` (pushed, keeping H or the smaller original gap). Queue U's cell edge with `newx = U.xtop + m` only if U's move is set and smaller than m (see Q-LEGACY-97).
- **Cell rule** (when a cell edge E of use U is processed and U moves by m): on every paint plane, (a) every tile T overlapping `[U.xbot - 1, U.xtop + H] × [E.ybot - H, E.ytop + H]` is dragged: if `left(T) <= U.xbot - 1`, atomize `[right(T), right(T) + m]` unless `lead(T) >= U.xtop + H` or `lead(T) >= right(T) + m`; otherwise atomize `[left(T), left(T) + m]` unless `trail(T) >= left(T) + m`; the y-range is the tile's clipped to the search area; (b) a forward shadow search of `[E.x - 1, E.newx + H] × [E.ybot - H, E.ytop + H]` with no transparent types queues each edge F with `newx = E.newx + min(F.x - E.x, H)` when larger than F's trail. Then (c) uses overlapping `[U.xbot - 1, E.newx + H] × [E.ybot - H, E.ytop + H]` are handled as in "find cells".

Compatibility: exact
(Magic ref: plow/PlowRules2.c:856-1104, plow/PlowMain.c:1540-1580)

#### Scenario: Cell pushed by the plow (verify against Magic)
- **GIVEN** a subcell use with bbox (10,0)-(20,10) and the box (5,0)-(15,10)
- **WHEN** `plow right` is executed
- **THEN** the use is queued with move 5 and is translated by +5 in x

### Requirement: Jog horizon extension
When the jog horizon h is greater than 0 (including infinity), each processed paint edge E SHALL, before its rules are applied, be extended to the nearest natural jog of its LHS outline within h:

```
hTop = E.ytop + h; hBot = E.ybot - h
# upward
yb = y = E.ytop; L = tile at (E.x - 1, E.ytop); changedTop = none
while right(L) = E.x and type(L) = E.ltype and bottom(L) < hTop:
    y = top(L)
    if changedTop is none: for the right neighbours R of L with top(R) > yb, top to bottom:
        if type(R) ≠ E.rtype: changedTop = bottom(R)
    L = tile above L at its left side; yb = y
newTop = E.ytop
if E.ytop < y <= hTop: newTop = y; E.ytop = changedTop if set else y
# downward
yt = y = E.ybot; R = tile at (E.x, E.ybot - 1); changedBot = none
while left(R) = E.x and top(R) > hBot:
    y = bottom(R)
    if changedBot is none and type(R) ≠ E.rtype: changedBot = top(R)
    for the left neighbours L of R with bottom(L) < yt, bottom to top: if type(L) ≠ E.ltype: y = top(L)
    if y > bottom(R): break
    R = tile below R at its left side; yt = y
newBot = E.ybot
if hBot <= y < E.ybot: newBot = y; E.ybot = changedBot if set else y
atomize [E.x, E.newx] × [E.ytop, newTop] and [E.x, E.newx] × [newBot, E.ybot] when nonempty
```

A straight boundary whose last LHS tile ends exactly at `hTop` is therefore extended to `hTop` without a jog there (see Q-LEGACY-103).

Compatibility: exact
(Magic ref: plow/PlowMain.c:1140-1320)

#### Scenario: Whole wire moves with a large horizon
- **GIVEN** a lone metal1 wire `(10,-20)-(13,30)`, `plow horizon 50`, and the box (5,0)-(20,10)
- **WHEN** `plow right` is executed
- **THEN** the initial edge y 0..10 is extended to y -20..30 and the whole wire becomes (20,-20)-(23,30)

#### Scenario: Default horizon inserts a jog
- **GIVEN** the same wire with horizon 0
- **WHEN** `plow right` is executed with the box (5,0)-(20,10)
- **THEN** the part y 0..10 moves to x 20..23 while the parts beyond the penumbra pieces stay at x 10..13

### Requirement: Result write-back
After propagation the edit cell SHALL be updated from the working planes, in this order, using the changed area C grown by 1 and mapped back to edit-cell coordinates (C'):

1. The cell is marked modified (new timestamp).
2. Every use whose move is set and non-zero is translated by its move in the plow direction (east +x, west −x, north +y, south −y).
3. Labels: for each label of the edit cell whose type is not space and whose rectangle touches C', search the working plane of its type over the label rectangle (canonical frame) grown by 1 for tiles of exactly the label's type; for each such tile T, `adjust = lead(T) - label.xtop` if `label.xtop = right(T)`, else `trail(T) - label.xbot`; the label is translated by the largest positive adjust (unchanged if none).
4. Paint: on every paint plane, C' is erased; then every non-space working tile overlapping C (grown) is painted in full as `[trail, lead] × [bottom, top]`, mapped back, on every plane of its type.
5. DRC is re-run over C'; if straightening is enabled, C' is then straightened in the plow direction.

Compatibility: exact
(Magic ref: plow/PlowMain.c:558-660, plow/PlowYank.c:353-570)

#### Scenario: Label on a moved wire
- **GIVEN** a point label of type metal1 at (11,5) on metal1 `(10,0)-(13,20)` and the box plow of "Box plow initial edges"
- **WHEN** the plow completes
- **THEN** the tile's trail is 20, the adjust is `20 - 11 = 9`, and the label is at (20,5)

### Requirement: Selection plow
`plow selection` SHALL plow the selected paint and cells of the edit cell as if each selected piece were itself a plow, in direction `dir` by `amount` (default 1 unit) when given; without arguments the displacement is from the box lower-left to the cursor point, using the larger-magnitude axis (ties choose the x axis: `|dy| <= |dx|`) and its sign. Initial edges (canonical frame, distance δ, paint edges flagged initial): for each selected paint rectangle `[x0, x1] × [y0, y1]` of type t, on t's home plane, a forward shadow search of `[x0 - 1, x0 + δ] × [y0, y1]` with no transparent types queues the reported edges with `newx = x0 + δ`, and a forward shadow search of `[x1 - 1, x1 + δ] × [y0, y1]` with transparent set {t} queues them with `newx = x1 + δ`; each selected use gets a cell edge with `newx = U.xtop + δ` (not flagged initial). The working planes are loaded with the selection's bounding box extended by δ to the right and by H on all sides; the plow distance for clamping is δ. Requirements and errors: the box must be in a window containing the selection (`"plow selection" uses the box lower-left corner as a place to pick up the selection for plowing, but the box isn't in a window containing the selection`, printed on four lines) and the point must be over the edit cell. After plowing, the box is translated by the (possibly reduced) displacement and the selection is cleared; if a boundary reduced the distance, `Reduced distance to stay in the boundary.` is printed. A distance <= 0 or a selection without edit-cell material does nothing.

Compatibility: behavioral
(Magic ref: plow/PlowCmd.c:210-300, plow/PlowMain.c:438-470, plow/PlowMain.c:828-1140)

#### Scenario: Default distance
- **WHEN** `plow selection right` is executed
- **THEN** the selection is plowed 1 unit to the right and the box moves right by 1

### Requirement: Plow boundary
When a boundary rectangle Bd (canonical frame) is set for the edit cell, every popped edge E SHALL be checked before processing and the overshoot recorded:

```
if E.x < Bd.xbot:                          delta = max(E.newx, Bd.xbot) - E.x       (Q-LEGACY-99)
elif E.newx > Bd.xtop:                     delta = E.newx - max(E.x, Bd.xtop); skip E if E.x > Bd.xtop
elif E.ytop > Bd.ytop or E.ybot < Bd.ybot: delta = E.newx - E.x
else:                                      delta = 0
tooFar = max(tooFar, delta)
```

Skipped edges are not processed, so their effects do not propagate. When the queue is empty and `tooFar > 0`, the box plow's far side is reduced by tooFar (`bx1 -= tooFar`), or the selection distance by tooFar, and the whole plow is recomputed from the original layout; this repeats until a pass ends with `tooFar = 0` (a box plow whose width becomes 0 does nothing). The user is informed as in "plow command". The boundary is displayed as a dotted highlight in windows of its root cell. Straightening ignores the boundary.

Compatibility: exact
(Magic ref: plow/PlowMain.c:189-330, plow/PlowMain.c:1364-1420, plow/PlowMain.c:760-800, plow/PlowMain.c:960-975)

#### Scenario: Plow reduced by boundary
- **GIVEN** `width metal1 3`, `spacing metal1 metal1 3 touching_ok`, boundary (0,0)-(28,20), metal1 A `(10,0)-(13,20)` and B `(24,0)-(27,20)`, and the box (5,0)-(20,20)
- **WHEN** `plow right` is executed
- **THEN** the first pass moves A to 20..23, B's left edge to `23 + 3 = 26` and B's right edge to 29, giving tooFar = 1; the plow is redone with the box (5,0)-(19,20), yielding A (19,0)-(22,20) and B (25,0)-(28,20), and `Reduced plow size to stay within the boundary.` is printed

### Requirement: Straightening
`straighten <dir>` SHALL remove jogs inside the box area A (canonical frame for `dir`) on working planes loaded with A expanded by H, with the jog horizon forced to 0 and boundary checking disabled, using a recording propagation: impacted edges are never queued; any impacted edge other than the allowed ones makes the current attempt fail.

```
for every paint plane: processJog(edge at x = A.xtop, y A.ybot..A.ytop)
while the queue is not empty: processJog(pop the rightmost edge)
processJog(E):
   Q0 = [A.xbot, E.x] × [E.ybot, E.ytop]
   repeat: backward shadow search of Q0 with transparent {space}, calling tryJog(F) for each
           material|space edge F, restarting after every eliminated jog, until a search eliminates none
   backward shadow search of Q0 with transparent = all but space; queue each space|material edge (newx = x)
tryJog(F):  L = F.ltype
   top: trace the outline of {L} from (F.x, F.ytop) heading north (N, W, E segments);
        stop when the outside tile is not space; west → code NW, stop;
        north → point = segment top, code N (if beyond A.ytop: point.y = A.ytop, stop);
        east → point = segment end, code NE (if at/beyond A.xtop: point.x = A.xtop, stop);
               then code NEN if the next direction is north, NES if south, stop
   bottom: trace the outline of the complement from (F.x, F.ybot) heading south (S, W, E segments);
        stop when the inside tile is not space; symmetric codes with A.ybot, and with
        next direction north → NES, south → NEN
   reject unless both codes ≠ N and at least one is NEN
   reject if (top = NES and top.x <= bot.x) or (bot = NES and bot.x <= top.x)
   G = F with y from bot.y to top.y, newx = max(top.x, bot.x) if either code is NW else min(top.x, bot.x)
   reject unless G lies inside A
   apply all search rules to G; reject if any edge other than a sub-piece of G (same plane and x) moved
   w = leftward real width of {L} from G (bounded by A)
   Q = [G.x - w - 1, G.x] × [G.ybot, G.ytop], plus w above unless top = NW and w below unless bot = NW
   reject unless Q lies inside A
   allowed = edges at x = G.x - w within Q (shifted right by 1), space on the left, L on the right
   backward shadow search of Q with transparent {L}: for each space|L edge K: K.newx = G.newx - w;
       apply all search rules to K; reject if anything other than G's pieces or allowed edges moved
   success: paint L over [G.x, G.newx] × [G.ybot, G.ytop]; erase [K.x, K.newx] × K's y-range for every K
```

The changed area is then erased in the edit cell (paint planes only) and repainted from the working planes; labels are not moved (see Q-LEGACY-104), and DRC is re-run. Errors: `Usage: straighten manhattan-direction`, `The box is not in a window over the edit cell.`. Automatic straightening after a plow uses the plow's written-back area and direction.

Compatibility: exact
(Magic ref: plow/PlowCmd.c:330-385, plow/PlowMain.c:332-436, plow/PlowJogs.c:100-618)

#### Scenario: Straighten a single jog (verify against Magic)
- **GIVEN** a metal1 wire with a 2-unit jog and free space around it
- **WHEN** `straighten left` is executed with the box over the jog
- **THEN** the jog is removed by pulling the offset segment left, provided no other material is forced to move

### Requirement: Incremental loading of the working planes
Magic loads only part of the edit cell into the working planes and grows the loaded area on demand (when a rule's area plus a margin reaches the loaded boundary the loaded area grows by half its size on the touching sides, clipped to the cell's bounding box). Hefesto SHALL produce the same geometry as if the whole edit cell had been loaded and MAY load it all at once (see Q-LEGACY-105).

Compatibility: informative
(Magic ref: plow/PlowYank.c:107-350, plow/PlowMain.c:2170-2200)

#### Scenario: Far material still constrains the plow
- **GIVEN** a chain of metal1 wires at minimum spacing extending far beyond the initially loaded area
- **WHEN** the first wire is plowed
- **THEN** every wire in the chain moves as if the whole cell had been loaded

### Requirement: Plowing limitations
Hefesto SHALL reproduce these documented limitations rather than silently improving them: plowing operates only on the edit cell's paint and its direct subcell uses (subcell contents are never modified); existing design-rule violations are preserved (spacing is never increased beyond the original separation); edges cannot move farther than the plow distance; labels move with the paint they are attached to (plow only, not straighten); non-Manhattan (split) tiles are treated by their bounding tiles as in the corner-stitched representation (see Q-LEGACY-50).

Compatibility: behavioral
(Magic ref: plow/PlowRules1.c:1005-1028, plow/PlowQueue.c:170-180, plow/TODO)

#### Scenario: Existing violation preserved
- **GIVEN** two metal1 boxes 1 unit apart while the rule requires 3
- **WHEN** the left box is plowed right by 4
- **THEN** the right box moves by 4 and their separation remains 1

### Requirement: Plow diagnostics
The wizard command `*plow` and the random plow tester SHALL be treated as developer diagnostics.

Compatibility: informative
(Magic ref: plow/PlowTest.c, plow/PlowRandom.c, plow/PlowCmd.c:385-403)

#### Scenario: Diagnostics have no compatibility obligation
- **WHEN** `*plow` is executed
- **THEN** Hefesto MAY print different diagnostics or none
