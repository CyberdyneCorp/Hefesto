# Mask Output Operators

## Purpose

Defines exactly how the mask layers of an output style (capability `mask-output-styles`) are computed from layout geometry: the geometric model, the layer evaluation pipeline for one cell area, and the precise semantics of every `cifoutput` operator, including rounding, grid snapping and tie-breaking. Hefesto MUST generate geometry that is point-set identical to Magic's for the same technology and layout, because this geometry is written to GDS (capability `gds-write`) and CIF (capability `cif-format-io`). How cells are combined hierarchically is in capability `mask-hierarchical-generation`.

## Requirements

### Requirement: Geometric model of mask planes

Each mask layer SHALL be represented as a region of the integer plane in mask units (mu), and every per-tile operator SHALL be evaluated over the canonical maximal-horizontal-strip decomposition of the relevant region, because several operators' results depend on that decomposition.

Compatibility: exact

Definitions used by all requirements below:

- **Mask plane:** a planar subdivision with two kinds, *solid* and *space*. Space extends to "infinity" (the database plane limits); coordinates at those limits are never scaled, grown or snapped.
- **Canonical decomposition:** the corner-stitched form of the plane: every tile is a rectangle (or a right-triangle split rectangle for non-Manhattan geometry); no two horizontally adjacent tiles have the same kind; vertically adjacent tiles of the same kind with identical x-extent are merged. This is the "maximal horizontal strips" decomposition (see capability `layout-geometry-model`). Database paint planes follow the same rule per tile type.
- **Split tile:** a rectangle divided by its diagonal into two triangles of (possibly) different kinds; its *direction* says which diagonal and its *side* which triangle is meant.
- **Neighbours:** tiles sharing a boundary segment of positive length. Corner contact is not adjacency.
- **Region:** a maximal set of solid tiles connected through neighbour relations (4-connectivity through edges of positive length).
- **Overlap test:** "tiles overlapping a rectangle R" means tiles whose interior intersects the interior of R (touching only does not count); for split tiles the triangle test of the database is used.
- **Scaling paint into mask units:** a database tile rectangle `[x1,x2]×[y1,y2]` in internal units becomes `[x1*s, x2*s]×[y1*s, y2*s]` mu with `s = scaleFactor`; mask-layer (temporary) planes are already in mu (scale 1).
- **Paint/erase:** OR-ing a rectangle (or triangle) into a plane, or removing it.

(Magic ref: cif/CIFgen.c:52-131, database/DBtiles.c:530-600)

#### Scenario: Paint scaling

- **GIVEN** scaleFactor 10 and a `metal1` tile `[3,7]×[0,5]` internal units
- **WHEN** a layer `or metal1` is evaluated
- **THEN** the mask plane contains `[30,70]×[0,50]` mu

### Requirement: Layer evaluation pipeline for an area

Generating the mask layers of a cell for an area A (internal units) SHALL evaluate the requested layers in declaration order, each by applying its operations in order to an initially empty plane, and SHALL then clip every result to `A * scaleFactor`.

Compatibility: exact

```
generate(cell, A, requestedLayers, replace, genAll, hier):
  E = A expanded by radius R on each finite side      (internal units)
  C = A * scaleFactor on each finite side; infinite sides stay infinite
  for i in 0 .. nLayers-1:
     if i in requestedLayers: new[i] = evalLayer(layer[i].ops, E, cell, new, hier)
                              if unfracture option: merge non-Manhattan tiles of new[i]
     else if genAll: new[i] = empty plane
     else new[i] = none
  for each i: clip new[i] to C (erase everything outside C)
              if replace: out[i] = new[i]
              else: out[i] = out[i] ∪ new[i]
evalLayer(ops, E, cell, temps, hier):
  cur = empty
  for op in ops: cur = apply(op, cur)  (may stop early in hierarchical mode)
  return cur
```

- Operations referencing mask layers read `temps[j]`, the *unclipped* result of an earlier layer of this same evaluation.
- Operations selecting database types search only tiles overlapping E, but each found tile contributes its whole (unclipped) area.
- R is the style's interaction radius (capability `mask-output-styles`).
- Temporary layers are evaluated like other layers; whether a layer is written is decided by the writers.

(Magic ref: cif/CIFgen.c:5735-5845, cif/CIFgen.c:5847-5951, cif/CIFgen.c:3974-4055)

#### Scenario: Whole tile contributes then clipped

- **GIVEN** scaleFactor 1, R = 2, a metal1 tile `[0,100]×[0,10]`, layer `or metal1` then `shrink 4`, area A = `[0,50]×[0,10]`
- **WHEN** generated
- **THEN** the shrink sees the whole tile `[0,100]×[0,10]`, giving `[4,96]×[4,6]`, and after clipping to A the result is `[4,50]×[4,6]`

### Requirement: or, and, and-not

`or L`, `and L` and `and-not L` SHALL compute `cur ∪ U`, `cur ∩ U` and `cur \ U` respectively, where U is the union of all tiles of the listed database types overlapping the search area (scaled to mu) and all solid geometry of the listed mask layers.

Compatibility: exact

Non-Manhattan triangles are carried exactly. A layer declared with an inline list starts with an implicit `or`.

(Magic ref: cif/CIFgen.c:5113-5149, cif/CIFgen.c:104-131)

#### Scenario: and-not

- **GIVEN** cur = `[0,100]×[0,100]` and `and-not` of a mask layer containing `[40,60]×[-10,200]`
- **WHEN** applied
- **THEN** the result is `[0,40]×[0,100] ∪ [60,100]×[0,100]`

### Requirement: grow and shrink (default, square metric)

`grow d` SHALL replace cur by its Minkowski sum with the axis-aligned square `[-d,d]²`, and `shrink d` SHALL replace cur by the complement of (complement(cur) ⊕ `[-d,d]²`), i.e. erosion by the same square, unless the style option `grow-euclidean` is set (then see "Euclidean grow and shrink").

Compatibility: exact

Per-tile definition (equivalent to the above and to be used for non-Manhattan tiles):

- Rectangular solid tile `[x1,x2]×[y1,y2]`: paint `[x1-d, x2+d]×[y1-d, y2+d]`; sides at infinity are not moved.
- Split tile with the solid triangle having its right angle at corner K and hypotenuse facing direction (sx, sy) ∈ {±1}²: paint the rectangle of width 2d centred on the vertical leg spanning `[y1-d, y2+d]`, the rectangle of height 2d centred on the horizontal leg spanning `[x1-d, x2+d]`, and the same-shaped triangle translated by `(sx*d, sy*d)`.
- `shrink` applies the same construction to space tiles (including the space triangles of split tiles) into a plane that starts fully solid and erases.

The result is computed on the whole plane (no clipping until the end of the pipeline).

(Magic ref: cif/CIFgen.c:793-912, cif/CIFgen.c:5151-5205)

#### Scenario: Grow merges, shrink keeps the bridge

- **GIVEN** cur = `[0,10]×[0,10] ∪ [14,24]×[0,10]`
- **WHEN** `grow 2` then `shrink 2` are applied
- **THEN** after grow the region is `[-2,26]×[-2,12]` and after shrink it is `[0,24]×[0,10]`

#### Scenario: Grow a 45-degree triangle

- **GIVEN** cur = triangle with vertices (0,0),(10,0),(0,10)
- **WHEN** `grow 2`
- **THEN** the result is the polygon (-2,-2),(12,-2),(12,2),(2,12),(-2,12)

### Requirement: Euclidean grow and shrink

When the style option `grow-euclidean` is set, `grow d`, `shrink d` and the diagonal case of `bloat-or` SHALL use the per-tile construction below, which equals the square-metric result on Manhattan geometry and moves diagonal edges by the smallest on-grid offset giving normal distance ≥ d.

Compatibility: exact

```
for each tile T (solid for grow, space for shrink), rect [x1,x2]×[y1,y2] in mu:
  dirs = {N,S,E,W} minus sides at infinity
  if T is split:
     remove from dirs the leg sides opposite the hypotenuse (i.e. keep only
     the two directions the hypotenuse faces)
     w = x2-x1; h = y2-y1
     r = h/w; dx = ceil(d * r / (1 + sqrt(1 + r*r)))
     r = w/h; dy = ceil(d * r / (1 + sqrt(1 + r*r)))
     dx = roundUpToGridLimit(dx); dy = roundUpToGridLimit(dy)
     paint rect: x-span = [x1, x1+dx] if E∉dirs-of-hypotenuse ... (as code):
        A = T; if E not in dirs: A.x2 = A.x1 + dx; if W not in dirs: A.x1 = A.x2 - dx
               if S not in dirs: A.y1 -= d;       if N not in dirs: A.y2 += d
        B = T; if E not in dirs: B.x2 += d;       if W not in dirs: B.x1 -= d
               if S not in dirs: B.y1 = B.y2 - dy; if N not in dirs: B.y2 = B.y1 + dy
        Tri = T with: y2 += d if N∉dirs else y2 -= dy; y1 -= d if S∉dirs else y1 += dy;
                      x2 += d if E∉dirs else x2 -= dx; x1 -= d if W∉dirs else x1 += dx
        paint A, B, and the triangle Tri with T's split orientation
     reference type = kind on the hypotenuse-facing side
  else paint T
  corner flags: for each of the 8 corner neighbour positions (NE via the top-
     most right neighbour, SE via the bottom-most right neighbour, EN/WN via
     the right-most/left-most top neighbours, SW/NW via the bottom-most/top-most
     left neighbours, WS/ES via the left-most/right-most bottom neighbours),
     set STOP_x if that neighbour's facing type equals the reference type
  if N in dirs: paint [x1,x2]×[y2,y2+d], extended by d to the east if E in dirs
     and neither EN nor NE is stopped, and to the west if W in dirs and
     neither WN nor NW is stopped
  E, S, W strips analogously (E: corners EN/NE and SE/ES; S: SE/ES and SW/WS;
     W: NW/WN and SW/WS)
```

`roundUpToGridLimit(v)` returns v rounded up to a multiple of `gridLimit` when `gridLimit > 1`, else v.

(Magic ref: cif/CIFgen.c:579-790, cif/CIFgen.c:239-260)

#### Scenario: 45-degree edge offset (verify against Magic)

- **GIVEN** `options grow-euclidean`, gridLimit 0 and a solid triangle (0,0),(10,0),(0,10)
- **WHEN** `grow 10`
- **THEN** dx = dy = ceil(10 * 1/(1+√2)) = ceil(4.142) = 5, so the hypotenuse moves outward by 5 in x and y (normal distance ≈ 7.07), while the legs move by 10

### Requirement: grow-grid

`grow-grid g` SHALL replace cur by the union over solid tiles of each tile's bounding rectangle with each finite side moved outward by `|c| mod g`, where c is that side's coordinate.

Compatibility: exact

`x1' = x1 - (|x1| mod g)`, `y1' = y1 - (|y1| mod g)`, `x2' = x2 + (|x2| mod g)`, `y2' = y2 + (|y2| mod g)`. Split tiles are treated as their full rectangle. This snaps lower/left edges with non-negative coordinates and upper/right edges with non-positive coordinates to the g-grid; other edges generally end off-grid. See Q-MASK-1.

(Magic ref: cif/CIFgen.c:502-545, cif/CIFgen.c:5179-5191)

#### Scenario: Positive coordinates

- **GIVEN** cur = `[3,12]×[3,12]`
- **WHEN** `grow-grid 5`
- **THEN** the result is `[0,14]×[0,14]` (the upper edges become 12+2 = 14, not 15)

#### Scenario: Negative coordinates

- **GIVEN** cur = `[-7,-3]×[0,5]`
- **WHEN** `grow-grid 5`
- **THEN** x1 = -7 - 2 = -9 and x2 = -3 + 3 = 0, giving `[-9,0]×[0,5]`

### Requirement: grow-min

`grow-min g` SHALL replace cur by the union of the rectangles produced by the per-tile algorithm below, which widens features narrower than g.

Compatibility: exact

```
for each solid tile T = [x1,x2]×[y1,y2] (mu):
  A = T; changed = false; W = x2-x1
  if W < g:
     k = (g - W) / 2 (truncating); A.x1 -= k; A.x2 += k
     H = A.y2 - A.y1
     if H < g:
        solidBelow = some bottom neighbour's top kind is solid
        solidAbove = some top neighbour's bottom kind is solid
        h = (int) sqrt(g*g - 0.25*(g+W)*(g+W) + 0.5)
        if !solidAbove and solidBelow: A.y1 -= h
        else if solidAbove and !solidBelow: A.y2 += h
        else: k = (g - H)/2; A.y1 -= k; A.y2 += k
        changed = true
  if changed: minBoxGrid(A, g)
  paint A
  for each bottom neighbour P (left to right), t = P's top kind:
     for each top neighbour Q (right to left) whose bottom kind == t:
        if x1(Q) in [x1(P), x2(P)]: xs = [max(x1(Q),x1), min(x2(P),x2)]
        elif x2(Q) in [x1(P), x2(P)]: xs = [max(x1(P),x1), min(x2(Q),x2)]
        else continue
        w = |xs|; H = y2-y1
        B = xs × [y1,y2]; if w < g: widen B by (g-w)/2 each side in x
                          if H < g: widen B by (g-H)/2 each side in y
        if w < g or H < g: minBoxGrid(B, g); paint B
minBoxGrid(A, g):
  if width(A) < g: s = A.x1 + A.x2; A.x1 = (s - g)/2; A.x2 = (s + g)/2   (C truncation)
  if height(A) < g: same in y
  snapOutward(A)
snapOutward(A): if gridLimit > 1, move x1,y1 down and x2,y2 up to multiples of gridLimit
```

The per-tile result depends on the canonical decomposition. See Q-MASK-15.

(Magic ref: cif/CIFgen.c:163-237, cif/CIFgen.c:272-318, cif/CIFgen.c:326-485)

#### Scenario: Narrow isolated line

- **GIVEN** gridLimit 0 and cur = `[0,4]×[0,100]` (single isolated tile)
- **WHEN** `grow-min 10`
- **THEN** the first rectangle is `[-3,7]×[0,100]`
- **AND** the neighbour loop (bottom and top neighbours are space, spanning the tile) adds `[-3,7]×[0,100]` again, so the result is `[-3,7]×[0,100]`

### Requirement: bloat-or

`bloat-or T (T2 d)+` SHALL OR into cur, for every database tile of a type in T (on the bloat plane, overlapping the search area), the tile itself plus, along each side, outward strips whose width equals the distance assigned to the neighbouring tile's type, as defined by the per-tile algorithm below.

Compatibility: exact

`dist(t)` is the per-type table (types not listed have 0; types in T have 0; `*` assigns all types including space). Distances are in mu; tile coordinates are scaled by `s = scaleFactor`.

```
for each tile X of a type in T, rect [x1,x2]×[y1,y2] (scaled), own type τ:
  paint X
  # left side, bottom to top
  yb = y1 - dist(type of the bottom neighbour at X's left end)
  for each left neighbour P (bottom to top):
     yt = min(top(P), y2); b = dist(type(P))
     if b != 0 and type(P) != τ: paint [x1-b, x1]×[yb, yt]
     yb = yt
  tl = type of the top-most left neighbour
  # top side, right to left
  xr = x2
  for each top neighbour Q (right to left):
     b = dist(type(Q)); xl = (left(Q) <= x1) ? x1 - dist(tl) : left(Q)
     if b != 0 and type(Q) != τ: paint [xl, xr]×[y2, y2+b]
     xr = xl
  # right side, top to bottom
  yt = y2 + dist(type of the top neighbour at X's right end)
  for each right neighbour P (top to bottom):
     yb = max(bottom(P), y1); b = dist(type(P))
     if b != 0 and type(P) != τ: paint [x2, x2+b]×[yb, yt]
     yt = yb
  br = type of the bottom-most right neighbour
  # bottom side, left to right
  xl = x1
  for each bottom neighbour Q (left to right):
     b = dist(type(Q)); xr = (right(Q) >= x2) ? x2 + dist(br) : right(Q)
     if b != 0 and type(Q) != τ: paint [xl, xr]×[y1-b, y1]
     xl = xr
```

(Neighbour coordinates are scaled before use.) For a split tile, sides on which the tile's own type is absent are skipped, and the triangle is bloated only along its hypotenuse direction by `dist` of the type on the other side of the diagonal: two rectangles of thickness `dist` along the legs and the triangle translated by `dist` outward (or, with `grow-euclidean`, the Euclidean grow of the tile). See Q-MASK-20.

(Magic ref: cif/CIFgen.c:923-1268, cif/CIFtech.c:1295-1362)

#### Scenario: Selective bloat with corner fill

- **GIVEN** scaleFactor 1; plane contains `ndiff` at `[0,10]×[0,10]`, `poly` immediately to the left `[-5,0]×[0,10]`, space elsewhere
- **WHEN** `bloat-or ndiff poly 3 space 1`
- **THEN** the painted rectangles are the tile `[0,10]×[0,10]`, the left strip `[-3,0]×[-1,10]` (extended down by dist(space) = 1 of the bottom neighbour at the left end), the top strip `[-3,10]×[10,11]` (extended left by dist(poly) = 3 of the top-most left neighbour), the right strip `[10,11]×[0,11]` (extended up by dist(space) = 1) and the bottom strip `[0,11]×[-1,0]` (extended right by dist(space) = 1)

### Requirement: bloat-max and bloat-min

`bloat-max` / `bloat-min` SHALL OR into cur, for every database tile of a type in T, the tile rectangle with each side moved outward by the maximum (respectively minimum) of `dist(type(P))` over that side's neighbours P whose type differs from the tile's own type, leaving a side unmoved if it has no such neighbour; negative values move the side inward.

Compatibility: exact

If the resulting rectangle is inverted (`x1 > x2` or `y1 > y2`) nothing is painted and the error "tile inverted by shrink" is reported. A degenerate (zero-width) result is painted as nothing. Split tiles use the moved rectangle with the original split orientation. Neighbour types are taken without regard to split halves.

(Magic ref: cif/CIFgen.c:3440-3555)

#### Scenario: Max vs min

- **GIVEN** scaleFactor 1, a `pdiff` tile `[0,10]×[0,10]` whose right neighbours are space over y∈[0,4] and `poly` `[10,20]×[4,10]`, space on all other sides, and table `space 2 poly 6`
- **WHEN** `bloat-max pdiff space 2 poly 6` is applied
- **THEN** the result is `[-2,16]×[-2,12]` (right side moved by max(2,6) = 6)
- **AND** `bloat-min` with the same arguments gives `[-2,12]×[-2,12]`

### Requirement: bloat-all

`bloat-all T T2 [d]` SHALL OR into cur, for every trigger tile of T, the connected region of T2 geometry reached from that trigger, optionally clipped to the trigger tile expanded by d.

Compatibility: exact

```
connect = T2 (database types on one plane) or {solid} of the T2 mask layers
for each trigger tile X (type in T found in search area, or solid of a T mask layer):
  if X's type is a database type on the T2 plane (or a contact touching it):
       seed = X itself (X is painted even if its type is not in T2)
  else: seed = the first not-yet-processed connect tile overlapping X's area,
        converting X's area to the T2 units (×scaleFactor or ÷scaleFactor
        with C truncation)
  clip = X's area (in T2-scaled mu) expanded by d   (only if d > 0)
  flood fill from seed through neighbours whose facing kind is in connect;
  each reached tile is painted (scaled to mu); if d > 0, rectangular tiles are
  painted clipped to clip (split tiles unclipped) and propagation in a
  direction stops once the tile reaches the clip boundary in that direction
  tiles reached are marked processed for the rest of this operation when d = 0;
  with d > 0 marks are reset after each trigger
```

Only one connected region is seeded per trigger tile when the trigger is not on the T2 plane. See capability `mask-output-styles` for argument rules.

(Magic ref: cif/CIFgen.c:1374-1625, cif/CIFgen.c:5282-5330)

#### Scenario: Implant covering device and contacts

- **GIVEN** scaleFactor 1; `nfet` `[10,20]×[0,10]`, `ndiff` `[0,10]×[0,10]` and `[20,30]×[0,10]`, `ndc` `[30,35]×[0,10]`, all on the active plane
- **WHEN** `bloat-all nfet ndiff,ndc`
- **THEN** the result is `[0,35]×[0,10]` (the nfet tile itself plus all connected ndiff/ndc)

#### Scenario: Clipped bloat-all (verify against Magic)

- **GIVEN** the same layout and `bloat-all nfet ndiff,ndc 5`
- **WHEN** applied
- **THEN** the result is `[5,25]×[0,10]` (clip `[5,25]×[-5,15]` around the nfet; both ndiff tiles are clipped and reach the clip boundary, so ndc is not reached)

### Requirement: Contact strip pre-pass for squares and slots

Before placing cuts in general regions, `squares`, `squares-grid` and `slots` SHALL find "strips" — parts of cur wide enough for exactly one row of cuts — generate cuts in each strip, and remove the strips from the input.

Compatibility: exact

Let `S = cutSize + 2*border` and `P = cutSize + sep` (for slots: short-side values). For each non-split solid tile T of cur with no split tile among its left neighbours, `w = width`, `h = height`, short = min(w,h) (vertical if `h > w`). If `S <= short < S + P`:

- vertical tile: one strip = T; `shrinkHi` = the top neighbour at T's right end is solid; `shrinkLo` = the bottom neighbour at T's left end is solid.
- horizontal tile: strips are the maximal x-sub-ranges of T where the material directly above AND directly below is space; `shrinkHi` = the sub-range ends before T's right edge; `shrinkLo` = it starts after T's left edge.

For each strip (in the order they were found, most-recent first): `D = sep - 2*border` (squares) or `D = lsep - lborder - sborder` (slots); if `D > 0`, shorten the strip by D at each end whose shrink flag is set (along its long axis); compute the cut array for the shortened strip with the placement function of the operator and paint all cuts. If no cut row results: if neither end was shrunk and the strip is exactly `cutSize × cutSize` (slots: `ssize × lsize` in either orientation), the strip itself is painted as a cut; otherwise, if neither end was shrunk, report "no room for contact cuts in area!"; if an end was shrunk and the strip area exceeds `2*P²` (slots: `2*lpitch*spitch`), report "contact strip with no room for cuts!". Finally erase the strip from cur.

(Magic ref: cif/CIFgen.c:2420-2546, cif/CIFgen.c:2771-2845, cif/CIFgen.c:3098-3170)

#### Scenario: Guard-ring strip

- **GIVEN** squares border 5, size 20, sep 20 (S = 30, P = 40) and a horizontal isolated tile `[0,200]×[0,30]`
- **WHEN** the pre-pass runs
- **THEN** the tile is a strip (30 ≤ 30 < 70); columns = (200+20-10)/40 = 5, first cut x = (0+200+20-200)/2 = 10, rows = (30+20-10)/40 = 1, y = (0+30+20-40)/2 = 5
- **AND** cuts `[10,30]`, `[50,70]`, `[90,110]`, `[130,150]`, `[170,190]` × `[5,25]` are generated

### Requirement: squares

`squares` SHALL replace cur by contact cuts: after the strip pre-pass, for each remaining region, cuts are laid out on the region's bounding box with the centring formula below and each cut is kept only if the cut grown by `border` lies entirely within the region.

Compatibility: exact

Placement function for a rectangle `[x1,x2]×[y1,y2]` (P = size + sep):

```
cols = (x2 - x1 + sep - 2*border) / P            (C truncating division)
if cols == 0: rows = 0; return
cx = (x1 + x2 + sep - cols*P) / 2               (C truncating division)
if gridLimit > 1 and (|cx| mod gridLimit) = δ > 0: x2 -= 2δ; recompute cols/cx
rows, cy likewise in y
first cut = [cx, cx+size] × [cy, cy+size]; cuts at cx + i*P, cy + j*P
```

Region processing: for each region of the (post-pre-pass) input, compute its bounding box B. Up to three attempts: place cuts for B; keep each cut whose `border`-grown square overlaps only solid, non-split tiles of this region (always kept if the region is a single rectangle with no solid neighbours); if at least one cut was kept, stop; otherwise if `rows == 2` shrink B in y by `b = 1 + height(B) - (2*(size+border) + sep)`, rounded up to even, halved, from each side; else if `cols == 2` the same in x; else stop. If no cut was generated, report "no room for contacts in area!". The cuts are the new cur. See Q-MASK-27, Q-MASK-28.

(Magic ref: cif/CIFgen.c:3769-3880, cif/CIFgen.c:2848-2990, cif/CIFgen.c:2548-2563, cif/CIFgen.c:5332-5347)

#### Scenario: 2x2 array centred

- **GIVEN** gridLimit 0, `squares 5 20 20` and cur = `[0,90]×[0,90]`
- **WHEN** applied
- **THEN** cols = rows = (90+20-10)/40 = 2, cx = cy = (0+90+20-80)/2 = 15
- **AND** the cuts are `[15,35]×[15,35]`, `[55,75]×[15,35]`, `[15,35]×[55,75]`, `[55,75]×[55,75]`

#### Scenario: Grid-limit shift

- **GIVEN** gridLimit 5, `squares 5 20 20`, cur = `[0,92]×[0,30]` (a strip)
- **WHEN** the x placement is computed
- **THEN** first cols = 2, cx = (0+92+20-80)/2 = 16, δ = 1, x2 becomes 90, recomputed cx = 15 (on grid)

### Requirement: squares-grid

`squares-grid border size sep gx gy` SHALL place cuts as `squares` does, but with the placement function below that aligns cut lower-left corners to the (gx, gy) grid.

Compatibility: exact

```
L = [x1+border, x2-border] × [y1+border, y2-border]
left   = ceilToMultiple(L.x1, gx); bottom = ceilToMultiple(L.y1, gy)
cols = (L.x2 - left + sep) / P;   if cols == 0: rows = 0; return
rows = (L.y2 - bottom + sep) / P; if rows == 0: return
right = left + cols*size + (cols-1)*sep; top = bottom + rows*size + (rows-1)*sep
mx = ((L.x2 - right) - (left - L.x1)) / (2*gx)      (C truncating)
my = ((L.y2 - top) - (bottom - L.y1)) / (2*gy)
first cut lower-left = (left + mx*gx, bottom + my*gy)
```

`ceilToMultiple(v, g)` = smallest multiple of g ≥ v (correct for negative v).

(Magic ref: cif/CIFgen.c:3882-3972)

#### Scenario: Grid-aligned cuts

- **GIVEN** `squares-grid 5 20 20 10 10` and cur = `[0,93]×[0,30]`
- **WHEN** the x placement is computed
- **THEN** L.x = [5,88], left = 10, cols = (88-10+20)/40 = 2, right = 10+40+20 = 70, mx = ((88-70)-(10-5))/20 = 0, first cut x = 10

### Requirement: slots

`slots sb ss ssep [lb [ls lsep [off [start]]]]` SHALL replace cur by slot cuts using the strip pre-pass and then per-region placement oriented by the region's bounding box, with short-side parameters across the short dimension and long-side parameters along the long dimension.

Compatibility: exact

Placement for a rectangle with orientation `vertical` (short axis = x if vertical, y otherwise; written below for vertical):

```
xp = ss + ssep
cols = (x2 - x1 + ssep - 2*sb) / xp;   if cols == 0: rows = 0; return
sx = (x1 + x2 + ssep - cols*xp) / 2;  cut x = [sx, sx+ss]
if gridLimit > 1 and δ = |sx| mod gridLimit > 0:
    x2 -= 2δ if sx >= 0 else x2 += 2δ; recompute
if ls > 0:
    yp = ls + lsep; rows = (y2 - y1 + lsep - 2*lb) / yp; if rows == 0: return
    sy = (y1 + y2 + lsep - rows*yp) / 2; cut y = [sy, sy+ls]; same grid fix
else:
    rows = 1; cut y = [y1+lb, y2-lb]; if empty: return (no slot)
```

Region processing: vertical iff `width(B) <= height(B)`. Cuts are stepped by `xpitch`/`ypitch` (short pitch across, long pitch along); row i's x origin is offset by `start + i*off` reduced modulo the x pitch (only when `>= xpitch`, subtract once per row), and all rows are offset in y by `start`. The cut-keeping test uses the cut grown by the x-border and y-border of the orientation. Retry shrinking uses `ydiff/xdiff = 2*(size+border)+sep` of the corresponding axis. Strips (pre-pass) paint only along the long axis with the long pitch.

(Magic ref: cif/CIFgen.c:3629-3765, cif/CIFgen.c:3069-3436, cif/CIFgen.c:5366-5381)

#### Scenario: Stripe slots

- **GIVEN** `slots 2 4 4` (no long-side values), gridLimit 0, and an isolated region `[0,100]×[0,10]`
- **WHEN** applied
- **THEN** the region is a horizontal strip (S = 8 ≤ 10 < 16); short axis y: cols = (10+4-4)/8 = 1, sy = (0+10+4-8)/2 = 3; long axis: one slot `[0,100]` (lb = 0)
- **AND** the result is `[0,100]×[3,7]`

### Requirement: close

`close [a]` SHALL OR into cur every bounded space region (a maximal 4-connected set of space tiles not touching the plane boundary) whose area is less than a mu², or every bounded space region if a is 0 or omitted.

Compatibility: exact

Area is the sum of tile areas (a split tile counts its full rectangle area divided by 2, truncated); accumulation stops once the running total reaches a. Space halves of split tiles are filled as triangles. The threshold a is normalised like a linear distance (see Q-MASK-2).

(Magic ref: cif/CIFgen.c:2191-2380, cif/CIFgen.c:5207-5221)

#### Scenario: Small hole filled

- **GIVEN** cur = ring `[0,30]×[0,30]` minus `[10,20]×[10,20]` (hole area 100)
- **WHEN** `close 101`
- **THEN** the result is `[0,30]×[0,30]`
- **AND** with `close 100` the hole remains

### Requirement: bridge

`bridge d w` SHALL add to cur bridging rectangles at catecorner configurations, where two solid shapes meet (or nearly meet) diagonally at a corner with spacing less than d, according to the per-tile algorithm below.

Compatibility: exact

```
out = copy of cur
for each solid tile T (unprocessed):
  # NE outside corner
  if left kind of TR(T) is space and bottom kind of RT(T) is space:
     search solid tiles V ≠ T overlapping [R-w, R+d]×[Tp-w, Tp+d] such that
        left(V) > R-w and bottom(V) > Tp-w, the tile below V's left end does not
        continue V (its top kind ≠ V's bottom kind), V is not space on right/top,
        and (tile below V's left end has space on top or tile left of V's bottom
        end has space on its right)
     first such V: paint [R, left(V)] × [Tp, bottom(V)]
  # SE outside corner (see Q-MASK-3 for the neighbour actually tested)
  RB = the right neighbour at T's bottom; LB0 = the bottom neighbour at T's LEFT end
  if left kind of RB is space and top kind of LB0 is space:
     search [R-w, R+d]×[B-d, B+w] for V (NW test) ; paint [R, left(V)]×[top(V), B]
for each space tile T: the dual test with kinds swapped and area
  [R-d, R+w]×[Tp-d, Tp+w] (resp. [R-d,R+w]×[B-w,B+d]); paints solid
```

R, Tp, B are T's right, top and bottom. The intended grid/width expansion of the bridge rectangle has no effect (Q-MASK-4); rectangles with non-positive extent paint nothing.

(Magic ref: cif/CIFgen.c:1838-2135, cif/CIFgen.c:1703-1836, cif/CIFgen.c:5223-5241)

#### Scenario: Catecorner squares (verify against Magic)

- **GIVEN** cur = `[0,10]×[0,10] ∪ [12,22]×[12,22]`
- **WHEN** `bridge 5 4`
- **THEN** the gap rectangle `[10,12]×[10,12]` is added

### Requirement: bridge-lim

`bridge-lim d w L` SHALL behave as `bridge` but first widen narrow tiles to w where not blocked by the limiting layers L and choose bridge shapes that avoid L, erasing L from a bridge when no limit-free shape exists, per the algorithm below.

Compatibility: exact

```
pass 0 (into a new plane), for each solid tile T:
  A = T; if width(T) < w: A.x1 = A.x2 - w; if L overlaps A: A.x1 = min(left(T), Xmax); A.x2 = A.x1 + w
         where Xmax = max right edge of L tiles overlapping the trial A (initially A.x1)
  if height(A) < w: for each bottom neighbour P and top neighbour Q of T that
     overlap in x within A, form a column; if its height (from P's bottom if P
     continues T, else A.y1, to Q's top if Q continues T, else A.y2) < w:
     paint the column grown to height w downward from its top, or, if L
     overlaps, upward from min(bottom, Ymax)
  paint A
pass 1/2 on the widened plane: as bridge, but the violator search uses a squared
  Euclidean test (corner pairs with dx>0, dy>0 and dx²+dy² ≥ d² (pass 1) or ≥ w²
  (pass 2) are ignored); candidate bridge rectangles are tried in a fixed order
  (vertical-first L, horizontal-first L, full square); the first candidate not
  overlapping L is painted; if none, the full candidate is painted and L's
  geometry inside it is erased from the result
```

Limiting database types are searched using mask-unit coordinates on internal-unit planes (Q-MASK-5). Hefesto MUST reproduce the per-tile procedure exactly as in the referenced code; the summary above is normative only together with the scenario cases in the golden set.

(Magic ref: cif/CIFgen.c:4146-4725, cif/CIFgen.c:5243-5267)

#### Scenario: Narrow tile widened (verify against Magic)

- **GIVEN** cur = `[0,2]×[0,20]`, empty L, `bridge-lim 4 6 L`
- **WHEN** pass 0 runs
- **THEN** the tile becomes `[-4,2]×[0,20]` (widened leftward from its right edge to width 6)

### Requirement: maxrect

`maxrect [internal|external]` SHALL replace each region of cur by a single rectangle: its bounding box for `external`; for `internal` the bounding box if the region is a single non-split tile with no solid neighbours, otherwise the largest-area axis-aligned rectangle contained in the region.

Compatibility: exact

Internal computation: start with the candidate list [bounding box]; for every obstacle tile (any tile overlapping the bounding box that is not part of the region, enumerated in the plane's area-search order) replace each candidate overlapping it by its non-degenerate parts above, below, left and right of the obstacle (each kept only if at least 1 mu thick), keeping non-overlapping candidates; finally choose the candidate with strictly largest area, the first in list order on ties. Split tiles belonging to the region are treated as entirely inside it. See Q-MASK-14.

(Magic ref: cif/CIFgen.c:2579-2735, utils/maxrect.c:30-160, utils/maxrect.c:306-344)

#### Scenario: L-shape

- **GIVEN** cur = `[0,10]×[0,30] ∪ [10,40]×[0,10]`
- **WHEN** `maxrect` (internal)
- **THEN** candidate areas are `[0,10]×[0,30]` (300) and `[0,40]×[0,10]` (400); the result is `[0,40]×[0,10]`
- **AND** with `maxrect external` the result is `[0,40]×[0,30]`

### Requirement: orthogonal

`orthogonal [fill|remove]` SHALL keep all rectangular solid tiles of cur and, for split (non-Manhattan) tiles, replace each by its full bounding rectangle (`fill`) or drop it (`remove`, default).

Compatibility: exact

(Magic ref: cif/CIFgen.c:2140-2189, cif/CIFgen.c:5383-5396)

#### Scenario: Remove triangle

- **GIVEN** cur = `[0,10]×[0,10]` plus triangle (10,0),(20,0),(10,10)
- **WHEN** `orthogonal`
- **THEN** the result is `[0,10]×[0,10]`; with `orthogonal fill` it is `[0,20]×[0,10]`

### Requirement: not-square

`not-square` SHALL keep every rectangular solid tile of cur except tiles that are square (width = height) and have no solid neighbour on any side; split tiles are dropped.

Compatibility: exact

See Q-MASK-7 (non-Manhattan geometry is removed).

(Magic ref: cif/CIFgen.c:4727-4818, cif/CIFgen.c:5420-5431)

#### Scenario: Isolated square removed

- **GIVEN** cur = `[0,10]×[0,10] ∪ [20,30]×[0,15]`
- **WHEN** `not-square`
- **THEN** the result is `[20,30]×[0,15]`

### Requirement: overlapping, nonoverlapping, interacting, noninteracting

These operators SHALL keep or discard whole regions of cur according to whether any tile of the region overlaps (`overlapping`) or overlaps-or-touches (`interacting`) the geometry of the listed layers; the `non` forms keep exactly the complementary set of regions.

Compatibility: exact

- Regions are seeded only from solid tiles overlapping the search area E (scaled), but extend without limit.
- Per tile rectangle r (mu): for the touching forms, r is grown by 1 mu on every side (diagonal corner contact therefore counts).
- Mask-layer test: any solid of the listed mask layer planes overlaps r.
- Database-type test: r is converted to internal units by dividing each coordinate by scaleFactor with C truncation toward zero, then any tile of a listed type overlaps it (Q-MASK-21).
- Kept regions are copied tile by tile into the result; others are discarded.

(Magic ref: cif/CIFgen.c:4822-4996, cif/CIFgen.c:4057-4140, cif/CIFtech.c:1131-1146)

#### Scenario: Touching counts only for interacting

- **GIVEN** scaleFactor 1, cur regions `[0,10]×[0,10]` and `[50,60]×[0,10]`, and layer M with `[10,20]×[0,10]`
- **WHEN** `overlapping M` is applied
- **THEN** the result is empty
- **AND** `interacting M` yields `[0,10]×[0,10]`, `noninteracting M` yields `[50,60]×[0,10]`

### Requirement: net

`net <name> [layers]` SHALL OR into cur the geometry of the listed types belonging to the electrical net that contains the label `<name>` in the cell being generated, found by hierarchical connectivity.

Compatibility: behavioral

The net is located by label name as by the net-finding command (see capability `cmd-selection` / selection), all electrically connected paint in the hierarchy is collected into a flat scratch cell, and tiles of the operation's types (overlapping the search area) are scaled and ORed into cur. With no layer list nothing is added (Q-MASK-22). If the name is not found nothing is added.

(Magic ref: cif/CIFgen.c:5433-5456, cif/CIFtech.c:1365-1372)

#### Scenario: Net extraction

- **GIVEN** a cell with label `VDD` on metal1 connected through via1 to metal2
- **WHEN** a layer `templayer PWR` with `net VDD metal1,metal2` is generated
- **THEN** PWR contains exactly the metal1 and metal2 tiles of that net

### Requirement: tagged

`tagged <text> [layers]` SHALL OR into cur every connected region of the listed layers (or, without a list, of cur itself — in which case cur is replaced by the selected regions) that is touched by a label of the cell being generated whose text equals `<text>` exactly.

Compatibility: exact

For each label of the cell (own labels only, in list order) with matching text, its rectangle (degenerate dimensions widened by 1 on each side) is used as a search area: database-type planes are searched with the area divided by scaleFactor; mask planes (or cur) are searched with the area as given — i.e. internal-unit label coordinates applied to mask-unit planes (Q-MASK-6). Each found tile seeds a `bloat-all`-style flood fill of the same layers (no clip).

(Magic ref: cif/CIFgen.c:5458-5599, cif/CIFgen.c:4057-4140)

#### Scenario: Tagged with scale 1

- **GIVEN** scaleFactor 1, cur regions `[0,10]×[0,10]` and `[20,30]×[0,10]`, a label `A` at point (25,5)
- **WHEN** `tagged A`
- **THEN** cur becomes `[20,30]×[0,10]`

### Requirement: bbox

`bbox [top]` SHALL OR into cur the cell's bounding box scaled to mu; with `top`, nothing is added if the cell has at least one parent instance in a non-internal cell.

Compatibility: exact

The bounding box is the cell definition's database bounding box (see capability `layout-geometry-model`; Q-MASK-23). Nothing is added when no cell is designated for generation errors (e.g. some preview paths).

(Magic ref: cif/CIFgen.c:5635-5680)

#### Scenario: bbox

- **GIVEN** scaleFactor 5 and a cell with bounding box `[0,0]-[100,40]`
- **WHEN** `bbox`
- **THEN** cur gains `[0,500]×[0,200]`

### Requirement: boundary

`boundary` SHALL OR into cur the rectangle given by the cell's `FIXED_BBOX` property (four integers in internal units), scaled to mu, when the cell has a fixed bounding box; otherwise nothing.

Compatibility: exact

If the property exists but is not a four-value dimension property the result is unspecified (Q-MASK-24).

(Magic ref: cif/CIFgen.c:5601-5633)

#### Scenario: Fixed bbox

- **GIVEN** scaleFactor 2 and `FIXED_BBOX 0 0 50 20`
- **WHEN** `boundary`
- **THEN** cur gains `[0,100]×[0,40]`

### Requirement: mask-hints

`mask-hints <name>` SHALL OR into cur every rectangle stored in the cell property `MASKHINTS_<name>` (internal units), scaled to mu; if the property is absent nothing is added.

Compatibility: exact

The property value is a set of rectangles (property type "plane"; textual form is groups of four integers `llx lly urx ury`, see capability `cell-file-format`). The property name is truncated to 511 characters.

(Magic ref: cif/CIFgen.c:5682-5705, cif/CIFgen.c:4998-5020)

#### Scenario: Hints

- **GIVEN** scaleFactor 1 and `MASKHINTS_HVI` = `0 0 10 10 20 0 30 10`
- **WHEN** `mask-hints HVI`
- **THEN** cur gains `[0,10]×[0,10] ∪ [20,30]×[0,10]`

### Requirement: Hierarchical-mode truncation

When a layer is evaluated in hierarchical mode (interaction areas and array processing), evaluation of that layer's operation list SHALL stop, returning cur as computed so far, at the first `net`, `tagged`, `boundary` or `bbox` operation, and at the first `squares`, `squares-grid` or `slots` operation that is not immediately followed by a `grow` operation.

Compatibility: exact

How the truncated result is used is specified in capability `mask-hierarchical-generation`. See Q-MASK-26.

(Magic ref: cif/CIFgen.c:5332-5380, cif/CIFgen.c:5433-5440, cif/CIFgen.c:5601-5641, cif/CIFgen.c:5714-5718)

#### Scenario: Squares stop

- **GIVEN** layer `CONT` = `or pc` `squares 5 17 17`
- **WHEN** evaluated in hierarchical mode
- **THEN** the returned plane is the unsquared `pc` geometry

### Requirement: Non-Manhattan merge option

When the non-Manhattan optimisation option is enabled (`cif unfracture yes`), each generated layer's plane SHALL be post-processed by merging split tiles into larger non-Manhattan tiles before clipping; the covered point set is unchanged but the tile decomposition (and hence polygon partitioning in output) changes.

Compatibility: exact

Default: disabled. The merge procedure is defined in capability `layout-geometry-model`.

(Magic ref: cif/CIFgen.c:49, cif/CIFgen.c:5797-5799, commands/CmdCD.c:2060-2080)

#### Scenario: Default off

- **GIVEN** a fresh session
- **WHEN** querying `cif unfracture`
- **THEN** it reports that non-Manhattan geometry is not optimised
