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

`grow-grid g` SHALL replace cur by the union over solid tiles of each tile's bounding rectangle with each finite side moved outward according to the `magic-compat` setting (Q-MASK-1, resolution: compat): with it ON (the default) by `|c| mod g`, where c is that side's coordinate, as in Magic; with it OFF to the g-grid, rounding outward.

Compatibility: exact

With `magic-compat` ON: `x1' = x1 - (|x1| mod g)`, `y1' = y1 - (|y1| mod g)`, `x2' = x2 + (|x2| mod g)`, `y2' = y2 + (|y2| mod g)`. This snaps lower/left edges with non-negative coordinates and upper/right edges with non-positive coordinates to the g-grid; other edges generally end off-grid.

With `magic-compat` OFF: `x1' = floor(x1/g)·g`, `y1' = floor(y1/g)·g`, `x2' = ceil(x2/g)·g`, `y2' = ceil(y2/g)·g`, so every finite edge is on the g-grid and the result contains the input.

In both settings split tiles are treated as their full rectangle and infinite edges are unchanged. The same function serves `grow-grid` in mask input styles (Q-MASK-334, capability `mask-input-styles`).

(Magic ref: cif/CIFgen.c:502-545, cif/CIFgen.c:519-522, cif/CIFgen.c:526-533, cif/CIFgen.c:5179-5191)

#### Scenario: Positive coordinates

- **GIVEN** cur = `[3,12]×[3,12]`
- **WHEN** `grow-grid 5`
- **THEN** the result is `[0,14]×[0,14]` (the upper edges become 12+2 = 14, not 15)

#### Scenario: Negative coordinates

- **GIVEN** cur = `[-7,-3]×[0,5]`
- **WHEN** `grow-grid 5`
- **THEN** x1 = -7 - 2 = -9 and x2 = -3 + 3 = 0, giving `[-9,0]×[0,5]`

#### Scenario: Outward rounding with magic-compat OFF

- **GIVEN** `magic-compat` OFF and cur = `[-7,12]×[3,12]`
- **WHEN** `grow-grid 5`
- **THEN** the result is `[-10,15]×[0,15]`

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

The per-tile result depends on the canonical decomposition. The algorithm is replicated exactly in both `magic-compat` settings (Q-MASK-15, resolution: replicate): h is evaluated exactly as Magic writes it, `(int) sqrt((double)(g*g) - 0.25 * (double)((g+W)*(g+W)) + 0.5)`: `g*g` and `(g+W)*(g+W)` are 32-bit `int` products, the subtraction and addition are IEEE double operations in that order, the square root is the double `sqrt`, and the `(int)` conversion truncates. The style loader warns when grow-min is used on a layer whose DRC rules are not `rect_only`.

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

(Neighbour coordinates are scaled before use.) For a split tile, sides on which the tile's own type is absent are skipped, and the triangle is bloated only along its hypotenuse direction by `dist` of the type on the other side of the diagonal: two rectangles of thickness `dist` along the legs and the triangle translated by `dist` outward (or, with `grow-euclidean`, the Euclidean grow of the tile), in both `magic-compat` settings (Q-MASK-20, resolution: replicate).

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

`bloat-all T T2 [d]` SHALL OR directly into cur, for every trigger tile of T, the tiles of T2 geometry reachable from that trigger by 4-connected flood fill, each scaled to mu and, when d > 0, clipped to the trigger area expanded by d, exactly as defined by the procedure below.

Compatibility: exact

Definitions:

- s = scaleFactor of the style; p2 = the single database plane of the T2 types (when T2 are database types).
- `connect` = the set of T2 database types (every type with a non-zero bloat value), or `{solid}` when T2 are mask layers.
- `a / s` divides each coordinate: with `magic-compat` ON with C truncation toward zero (so `-17/10 = -1`), as in Magic; with it OFF rounding outward, floor for lower edges and ceil for upper edges (so `[-17,23] / 10 = [-2,3]`) (Q-MASK-232, resolution: compat). `a · s` multiplies each coordinate.
- A tile's *mark* is one of `unmarked`, `pending`, `processed`. All marks start `unmarked`. `push(u, side)` pushes (u, side) onto a LIFO stack and sets mark(u) = pending, but only if mark(u) = unmarked.
- *Seed search* over area A in plane P: enumerate tiles of P whose type is in `connect` (database) or solid (mask layer) and overlap A, in tile enumeration order (capability `layout-geometry-model`). With `magic-compat` ON, `push` the first one whose mark is `unmarked` and stop, as in Magic; with it OFF, `push` every one whose mark is `unmarked` (Q-MASK-231, resolution: compat). Tiles that are already marked are skipped.

Trigger enumeration order: database planes in plane order, each searched over the evaluation area E for the database types of T; then the mask layers of T in layer order, each over its whole plane. Triggers never come from cur.

Per trigger X (rectangle `[x1,x2]×[y1,y2]`):

| Case | Trigger | T2 | Seeds | clip base | locScale |
|---|---|---|---|---|---|
| A | mask layer | database | seed search on p2 over `X / s` | `X / s` | s |
| B | mask layer | mask layers | one seed search per T2 layer, in layer order, over X | X | 1 |
| C | database type t, and every T2 type shares a plane with t | database | X itself (pushed with its own side, even if t ∉ connect) | X | s |
| D | database type t otherwise | database | seed search on p2 over X | X | s |
| E | database type t | mask layers | one seed search per T2 layer over `X · s` | `X · s` | 1 |

If d > 0: `clip = (clipBase · locScale)` with every side moved outward by d.

```
flood(stack):
  while stack not empty:
    (t, side) = pop
    if mark(t) != pending: continue
    mark(t) = processed
    A = rect(t) · locScale
    if d > 0:
      remember t for reset
      C = A ∩ clip
      if C.x1 >= C.x2 or C.y1 >= C.y2: continue        # touching only: nothing painted, no propagation
    if t is split:
      if (side = right and leftType(t) ∈ connect) or (side = left and rightType(t) ∈ connect):
        paint A (full rectangle)
      else paint the `side` triangle of A
    else paint (d > 0 ? C : A)
    # propagation; each direction is skipped when d > 0 and A already reaches the clip edge
    if d = 0 or A.y2 < clip.y2: for u in top neighbours (right to left):   if bottomType(u) ∈ connect: push(u, dir(u) = dir(t) ? left : right)
    if d = 0 or A.x1 > clip.x1: for u in left neighbours (bottom to top):  if rightType(u) ∈ connect: push(u, right)
    if d = 0 or A.y1 > clip.y1: for u in bottom neighbours (left to right): if topType(u) ∈ connect: push(u, dir(u) = dir(t) ? right : left)
    if d = 0 or A.x2 < clip.x2: for u in right neighbours (top to bottom): if leftType(u) ∈ connect: push(u, left)
  if d > 0: set every remembered tile to unmarked
```

`dir(t)` is the split direction (0 for a non-split tile). Split tiles are never clipped: the clip only decides whether they are painted and expanded. With d = 0 the marks persist for the rest of the operation, so a region reached from one trigger is not re-flooded. With `magic-compat` ON a later trigger whose first overlapping region is already processed seeds the next unprocessed region overlapping it instead, so the result depends on trigger order; with it OFF every overlapping region is seeded, so the result is the union of all regions touching any trigger, independent of order (Q-MASK-231, resolution: compat). After all triggers, marks are reset: on the T2 mask layers (all solid tiles), or on plane p2 for the types `connect ∪ T` (database case). With `magic-compat` ON, trigger tiles of case C on another plane than p2 (contact images) are not reset, as in Magic: their marks stay set for the rest of the layer generation of that cell, so a later bloat-all with the same trigger produces nothing for it; marks never persist past the generation pass of the cell or into the editable database. With it OFF the marks on every plane searched for triggers are reset at the end of the operation (Q-MASK-230, resolution: compat). Mask-layer triggers in case A use the `a / s` division above, truncating with `magic-compat` ON and rounding outward with it OFF (Q-MASK-232, resolution: compat). See capability `mask-output-styles` for argument rules.

(Magic ref: cif/CIFgen.c:1233-1249, cif/CIFgen.c:1332-1344, cif/CIFgen.c:1374-1625, cif/CIFgen.c:3974-4030, cif/CIFgen.c:5282-5330, drc/DRCtech.c:113-126)

#### Scenario: Implant covering device and contacts

- **GIVEN** scaleFactor 1; `nfet` `[10,20]×[0,10]`, `ndiff` `[0,10]×[0,10]` and `[20,30]×[0,10]`, `ndc` `[30,35]×[0,10]`, all on the active plane
- **WHEN** `bloat-all nfet ndiff,ndc`
- **THEN** the result is `[0,35]×[0,10]` (case C: the nfet tile itself plus all connected ndiff/ndc)

#### Scenario: Clipped bloat-all

- **GIVEN** the same layout and `bloat-all nfet ndiff,ndc 5`
- **WHEN** applied
- **THEN** clip = `[5,25]×[-5,15]`; the nfet is painted, the left ndiff is painted as `[5,10]×[0,10]` and does not propagate left (its left edge 0 ≤ 5), the right ndiff is painted as `[20,25]×[0,10]` and does not propagate right (30 ≥ 25), so ndc is never reached
- **AND** the result is `[5,25]×[0,10]`

#### Scenario: Mask-layer trigger with truncating unit conversion (verify against Magic)

- **GIVEN** scaleFactor 10, a mask layer `M` with one tile `[-17,23]×[0,10]` mu, and an `ndiff` tile `[-2,3]×[0,1]` internal units
- **WHEN** `bloat-all M ndiff 5`
- **THEN** (case A) the seed area is `[-1,2]×[0,1]`, which finds the ndiff tile; clip = `[-10,20]×[0,10]` grown by 5 = `[-15,25]×[-5,15]`
- **AND** the painted result is `[-15,25]×[0,10]`
- **AND** with `bloat-all M ndiff` (no distance) the result is the whole ndiff tile `[-20,30]×[0,10]`
- **AND** with `magic-compat` OFF the seed area is `[-2,3]×[0,1]` and the clip is `[-20,30]×[0,10]` grown by 5 = `[-25,35]×[-5,15]`, so the painted result is the whole ndiff tile `[-20,30]×[0,10]`

#### Scenario: Order-independent seeding with magic-compat OFF

- **GIVEN** scaleFactor 1, a mask-layer trigger `M` `[0,30]×[0,10]`, and two separate `ndiff` tiles `[0,10]×[0,10]` and `[20,30]×[0,10]`
- **WHEN** `bloat-all M ndiff` is applied with `magic-compat` OFF
- **THEN** both ndiff tiles are seeded and the result is `[0,10]×[0,10]` ∪ `[20,30]×[0,10]`
- **AND** with `magic-compat` ON only the first unmarked ndiff tile in enumeration order is seeded from that trigger

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

Region processing: for each region of the (post-pre-pass) input, compute its bounding box B. Up to three attempts: place cuts for B; keep each cut whose `border`-grown square overlaps only solid, non-split tiles of this region (always kept if the region is a single rectangle with no solid neighbours); if at least one cut was kept, stop; otherwise if `rows == 2` shrink B in y by `b = 1 + height(B) - (2*(size+border) + sep)`, rounded up to even, halved, from each side; else if `cols == 2` the same in x; else stop. If no cut was generated, report "no room for contacts in area!". The cuts are the new cur. See Q-MASK-28.

The grid-limit step depends on the `magic-compat` setting (Q-MASK-27, resolution: compat): with it ON (the default) it is sign-blind, `x2 -= 2δ` whatever the sign of cx, as in Magic, so a contact left of or below the origin can lose its cuts; with it OFF it is sign-aware, as in `slots`: `x2 -= 2δ` when cx ≥ 0 and `x2 += 2δ` when cx < 0 (likewise in y).

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

#### Scenario: Grid-limit shift at negative coordinates

- **GIVEN** gridLimit 5, `squares 5 20 20`, cur = `[-92,0]×[0,30]` (a strip)
- **WHEN** the x placement is computed
- **THEN** first cols = 2, cx = (-92+0+20-80)/2 = -76, δ = 1
- **AND** with `magic-compat` ON x2 becomes -2 and the recomputed cx = -77 (off grid, as in Magic)
- **AND** with `magic-compat` OFF x2 becomes 2 and the recomputed cx = -75 (on grid)

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

Area is the sum of tile areas (a split tile counts its full rectangle area divided by 2, truncated); accumulation stops once the running total reaches a. Space halves of split tiles are filled as triangles.

Unit normalisation of the threshold depends on the `magic-compat` setting (Q-MASK-2, resolution: compat). With it ON (the default) a is multiplied and divided linearly by each unit change, like a distance, as in Magic. With it OFF a is kept in the declared mask units² and the comparison is exact in 64-bit: a region is filled when `atotal · L² < a · D²`, where D is the cumulative multiply and L the cumulative reduce applied to the style since a was declared. In both settings close values take part in the unit-reduction GCD as lengths (capability `mask-output-styles`), so scaleFactor, expander, the GDS UNITS record and the CIF DS scale do not depend on the setting.

(Magic ref: cif/CIFgen.c:2191-2380, cif/CIFgen.c:2205-2223, cif/CIFgen.c:5207-5221, cif/CIFtech.c:2500-2502, cif/CIFtech.c:2648-2649, cif/CIFtech.c:1912-1915)

#### Scenario: Small hole filled

- **GIVEN** cur = ring `[0,30]×[0,30]` minus `[10,20]×[10,20]` (hole area 100)
- **WHEN** `close 101`
- **THEN** the result is `[0,30]×[0,30]`
- **AND** with `close 100` the hole remains

#### Scenario: Threshold after a unit reduction with magic-compat OFF

- **GIVEN** `magic-compat` OFF, `close 100` declared, and a later unit normalisation that reduces the style by L = 2 (D = 1)
- **WHEN** a bounded hole of 24 reduced mask units² (96 declared units²) is evaluated
- **THEN** 24 · 4 = 96 < 100 · 1, so the hole is filled
- **AND** with `magic-compat` ON the threshold becomes 100 / 2 = 50 and the hole (24 < 50) is also filled, while a hole of 26 reduced units² (104 declared units²) is filled with ON but not with OFF

### Requirement: bridge

`bridge d w` SHALL replace cur by cur plus one bridging rectangle for every catecorner solid–solid gap (pass A) and every catecorner space–space pinch (pass B) found by the corner tests below, each bridge being the corner-to-corner rectangle expanded by the minimum-width rule `expand(·, w, ·)`.

Compatibility: exact

d is the spacing distance, w the bridge width, both mu (> 0, see capability `mask-output-styles`). All tests read the unmodified input plane cur; all bridges are painted into a copy of cur, so the result is order-independent except for which violator is chosen. Neighbour notation for a tile T with left L, right R, bottom B, top Tp: `TR(T)` = right neighbour at T's top, `RT(T)` = top neighbour at T's right end, `LB(T)` = bottom neighbour at T's left end, `BL(T)` = left neighbour at T's bottom. `RBot(T)` = the right neighbour containing T's bottom edge (start at TR(T), step down while its bottom > B). `BRight(T)` = the bottom neighbour at T's right end (start at LB(T), step right while its right < R). `SEB(T)` is the bottom neighbour used for the pass-1 SE corner test, chosen by the `magic-compat` setting (Q-MASK-3, resolution: compat): with it ON (the default) `LB(T)`, the bottom-left neighbour, as in Magic, whose search loop tests the right neighbour's edge and never advances; with it OFF `BRight(T)`, the bottom neighbour under T's right end, as in pass B. The plane's boundary sentinel tiles are neither space nor solid. Rectangles are written `[x1,x2]×[y1,y2]` and may be inverted (x1 > x2) before `expand`.

```
bridge(cur, d, w):
  out = copy(cur)
  for each solid tile T of cur, in tile enumeration order:            # pass A
    if leftKind(TR(T)) = space and bottomKind(RT(T)) = space:          # NE outer corner
      V = violator(cur, [R-w, R+d]×[Tp-w, Tp+d], T, SW, k=space, search=solid)
      if V: paint(out, expand([R, left(V)]×[Tp, bottom(V)], w, false))
    if leftKind(RBot(T)) = space and topKind(SEB(T)) = space:          # SE outer corner (Q-MASK-3)
      V = violator(cur, [R-w, R+d]×[B-d, B+w], T, NW, k=space, search=solid)
      if V: paint(out, expand([R, left(V)]×[top(V), B], w, false))
  for each space tile T of cur, in tile enumeration order:            # pass B
    if leftKind(TR(T)) = solid and bottomKind(RT(T)) = solid:          # NE inner corner
      V = violator(cur, [R-d, R+w]×[Tp-d, Tp+w], T, SW, k=solid, search=space)
      if V: paint(out, expand([R, left(V)]×[Tp, bottom(V)], w, true))
    if leftKind(RBot(T)) = solid and topKind(BRight(T)) = solid:       # SE inner corner
      V = violator(cur, [R-d, R+w]×[B-w, B+d], T, NW, k=solid, search=space)
      if V: paint(out, expand([R, left(V)]×[top(V), B], w, true))
  cur = out

violator(plane, S, T, dir, k, search):
  for each tile V of kind `search` overlapping S, in tile enumeration order, V ≠ T:
    if dir = SW:
      P = LB(V); Q = BL(V)
      if left(V) <= S.x1 or bottom(V) <= S.y1: continue      # SW corner outside S
      if topKind(P) = bottomKind(V): continue                 # false corner
      if rightKind(V) = k or topKind(V) = k: continue         # split tile, NE corner clipped
      if topKind(P) = k or rightKind(Q) = k: return V         # note: OR
    if dir = NW:
      P = top neighbour at V's left end; Q = left neighbour at V's top end
      if left(V) <= S.x1 or top(V) >= S.y2: continue
      if bottomKind(P) = topKind(V): continue
      if rightKind(V) = k or bottomKind(V) = k: continue
      if bottomKind(P) = k and rightKind(Q) = k: return V     # note: AND
  return none
```

`expand(r, w, space)` computes the bridge rectangle (all square roots in IEEE single precision, `ceilf` rounding up):

```
overlap = false
if r.x1 > r.x2: overlap = true; horiz = not space
if r.y1 > r.y2: overlap = true; horiz = space            # wins if both are inverted
q = canonical(r)  (swap inverted coordinates);  W = q.x2 - q.x1;  H = q.y2 - q.y1
if not overlap: horiz = (W > H)
if horiz:
  dx = ceilf((w - W) / 2.0)
  b  = (overlap or space) ? w² - (W + dx)² : w² - dx²
  dy = (space and not overlap) ? ceilf(sqrtf(b) - H) : ceilf(sqrtf(b))
  dy = max(dy, ceilf((sqrtf(w² - W²) - H) / 2.0))
else:   (same with x/W and y/H exchanged)
  dy = ceilf((w - H) / 2.0)
  b  = (overlap or space) ? w² - (H + dy)² : w² - dy²
  dx = (space and not overlap) ? ceilf(sqrtf(b) - W) : ceilf(sqrtf(b))
  dx = max(dx, ceilf((sqrtf(w² - H²) - W) / 2.0))
q = [q.x1-dx, q.x2+dx] × [q.y1-dy, q.y2+dy]
if gridLimit g > 1: snap q.x1, q.y1 down and q.x2, q.y2 up to multiples of g (correct for negative values)
return q
```

dx or dy is negative when the gap already exceeds w, which narrows the bridge to w or w+1 along that axis. A negative argument to `sqrtf` yields NaN, and its conversion to int is platform-dependent (Q-MASK-227). The expansion is applied (an earlier reading that it was discarded, Q-MASK-4, is withdrawn).

(Magic ref: cif/CIFgen.c:1703-1803, cif/CIFgen.c:1838-2135, cif/CIFgen.c:5223-5241, cif/CIFtech.c:1199-1216, tiles/tile.c:139-160)

#### Scenario: Catecorner squares (verify against Magic)

- **GIVEN** gridLimit 0, cur = `[0,10]×[0,10] ∪ [12,22]×[12,22]`
- **WHEN** `bridge 5 4`
- **THEN** pass A on tile `[0,10]×[0,10]` finds NE corner (10,10), search box `[6,15]×[6,15]`, violator `[12,22]×[12,22]`; gap `[10,12]×[10,12]`, W = H = 2, vertical: dy = 1, b = 15, dx = ceil(3.873) = 4, dx2 = ceil(0.732) = 1
- **AND** the result is cur ∪ `[6,16]×[9,13]`; no other corner produces a bridge

#### Scenario: Bridge snapped to grid limit (verify against Magic)

- **GIVEN** the same cur, `bridge 5 4`, and gridLimit 5
- **WHEN** applied
- **THEN** the bridge `[6,16]×[9,13]` is snapped outward to `[5,20]×[5,15]` and ORed into cur

#### Scenario: Pinch between overlapping corners (verify against Magic)

- **GIVEN** gridLimit 0, cur = `[0,10]×[0,10] ∪ [9,20]×[9,20]` (1×1 overlap)
- **WHEN** `bridge 5 4`
- **THEN** pass B on space tile `[-∞,9]×[10,20]` finds the SE inner corner (9,10), search box `[4,13]×[6,15]`, violator space tile `[10,∞]×[0,9]`; gap `[9,10]×[9,10]`, W = H = 1, vertical with space: dy = 2, b = 7, dx = ceil(2.646 − 1) = 2, dx2 = 2
- **AND** the result is cur ∪ `[7,12]×[7,12]`

#### Scenario: Gap at or beyond the spacing distance

- **GIVEN** cur = `[0,10]×[0,10] ∪ [15,25]×[15,25]`
- **WHEN** `bridge 5 4`
- **THEN** the NE search box `[6,15]×[6,15]` does not overlap the second square's interior, no violator is found, and cur is unchanged

### Requirement: bridge-lim

`bridge-lim d w L` SHALL (pass 0) widen every solid tile of cur narrower or shorter than w into a new plane, avoiding the limiting layers L where possible, and then (passes 1 and 2) add bridges at catecorner gaps and pinches of that new plane, choosing among three fixed candidate shapes the first whose bounding box does not overlap L, exactly as defined below.

Compatibility: exact

L may contain database types and mask layers. `limitFree(A)` is true iff no tile of L overlaps A: database planes containing L types are searched first (plane order), then L mask layers (layer order). `limitOverlap(A)` searches the same tiles and returns `(found, Xmax, Ymax)` where Xmax/Ymax start at A.x1/A.y1 and become the largest right/top edge of any L tile overlapping A. `eraseLimits(A)` erases L geometry from the output plane. The units of these searches depend on the `magic-compat` setting (Q-MASK-5, resolution: compat):

- With `magic-compat` ON (the default), as in Magic: database planes are searched with the mask-unit rectangle A unchanged, Xmax/Ymax are raw tile edges of the plane searched, and `eraseLimits(A)` erases the **whole** rectangle of every L tile overlapping A, unscaled (Q-MASK-229, resolution: compat).
- With `magic-compat` OFF: the search area on a database plane is A converted to internal units with outward rounding (floor(x/scaleFactor) for lower edges, ceil for upper edges); database tile edges are multiplied by scaleFactor before they are used for Xmax/Ymax and for erasure; and `eraseLimits(A)` erases only the intersection of each overlapping L tile with A, in mask units (Q-MASK-229).

In both settings the style loader warns when bridge-lim names database types and scaleFactor ≠ 1, the case where the settings differ.

```
# pass 0: cur -> P (new empty plane), for each solid tile T of cur in enumeration order
A = rect(T)
if width(T) < w:
  A.x1 = A.x2 - w
  (found, Xmax, _) = limitOverlap(A)
  if found: A.x1 = min(left(T), Xmax); A.x2 = A.x1 + w
if height(A) < w:
  for p in bottom neighbours of T (left to right):
    lim = max(left(p), A.x1)
    for q in top neighbours of T (right to left) while right(q) > lim:
      if left(q) >= right(p): continue
      col.x1 = max(left(q), lim); col.x2 = min(right(q), right(p), A.x2)
      col.y2 = (bottomKind(q) = solid) ? top(q) : A.y2
      y0     = (topKind(p) = solid)    ? bottom(p) : A.y1
      if col.y2 - y0 < w:
        col.y1 = col.y2 - w
        (found, _, Ymax) = limitOverlap(col)
        if found: col.y1 = min(y0, Ymax); col.y2 = col.y1 + w
        paint(P, col)
paint(P, A)
```

With `magic-compat` ON (the default) passes 1 and 2 enumerate the tiles of P and paint into P itself while it is being enumerated, as in Magic, so bridges painted earlier in a pass are seen by later corner tests; Hefesto's tile plane reproduces Magic's tile splitting and merging and the behaviour of an area search over tiles that change during the search. With it OFF passes 1 and 2 enumerate a snapshot of the pass-0 plane P and paint into a copy, so the result depends only on geometry (Q-MASK-228, resolution: compat). They use the corner tests of `bridge` (including `SEB` of Q-MASK-3 for the pass-1 SE corner; the pass-2 SE corner uses BRight) with these differences: search boxes start at the corner, i.e. pass 1 NE `[R, R+d]×[Tp, Tp+d]`, SE `[R, R+d]×[B-d, B]`; pass 2 NE `[R, R+w]×[Tp, Tp+w]`, SE `[R, R+w]×[B-w, B]`; and the violator test is:

```
limViolator(P, S, T, dir, k, search, D):        # D = d (pass 1) or w (pass 2)
  for each tile V of kind `search` overlapping S, enumeration order, V ≠ T:
    if V is split and (dir = NW: rightKind(V) = k or bottomKind(V) = k;
                       dir = SW: rightKind(V) = k or topKind(V) = k): continue
    SW: P1 = LB(V), Q1 = BL(V); dx = left(V) - R; dy = bottom(V) - Tp; need topKind(P1) = k and rightKind(Q1) = k
    NW: P1 = top neighbour at V's left end, Q1 = left neighbour at V's top end;
        dx = left(V) - R; dy = B - top(V); need bottomKind(P1) = k and rightKind(Q1) = k
    if the need holds:
      if dx > 0 and dy > 0 and dx² + dy² >= D² (64-bit): continue
      return V
  return none
```

Bridge shapes (Vl, Vb, Vt = left, bottom, top of V; the first candidate whose bounding box is limit-free is used):

```
pass 1, NE (V up-right of T):
  c1 = [min(Vl,R), max(R,Vl+w)] × [min(Vb,Tp-w), max(Tp,Vb)]
  if limitFree(c1): paint [c1.x1,c1.x2]×[c1.y1,Tp]; paint [Vl,c1.x2]×[c1.y1,c1.y2]
  else c2 = [min(Vl,R-w), max(R,Vl)] × [min(Vb,Tp), max(Tp,Vb+w)]
    if limitFree(c2): paint [c2.x1,c2.x2]×[Vb,c2.y2]; paint [c2.x1,R]×[c2.y1,c2.y2]
    else c3 = [min(Vl,R-w), max(R,Vl+w)] × [min(Vb,Tp-w), max(Tp,Vb+w)]; paint c3; eraseLimits(c3)
pass 1, SE (V down-right of T):
  c1 = [min(Vl,R-w), max(R,Vl)] × [min(B,Vt-w), max(Vt,B)]
  if limitFree(c1): paint [c1.x1,R]×[c1.y1,c1.y2]; paint [c1.x1,c1.x2]×[c1.y1,Vt]
  else c2 = [min(Vl,R), max(R,Vl+w)] × [min(B,Vt), max(Vt,B+w)]
    if limitFree(c2): paint [Vl,c2.x2]×[c2.y1,c2.y2]; paint [c2.x1,c2.x2]×[B,c2.y2]
    else c3 = [min(Vl,R-w), max(R,Vl+w)] × [min(B,Vt-w), max(Vt,B+w)]; paint c3; eraseLimits(c3)
pass 2, NE:  a = [Vl-w, R]×[Vb-w, Tp]; if not limitFree(a): a = [Vl, R+w]×[Vb, Tp+w]
             if not limitFree(a): a = [Vl-w, R+w]×[Vb-w, Tp+w]; paint a; eraseLimits(a)  else paint a
pass 2, SE:  a = [Vl, R+w]×[B-w, Vt]; if not limitFree(a): a = [Vl-w, R]×[B, Vt+w]
             if not limitFree(a): a = [Vl-w, R+w]×[B-w, Vt+w]; paint a; eraseLimits(a)  else paint a
```

Paint/erase of a rectangle with non-positive extent changes nothing. Bridges do not use `expand` or the grid limit. Unlike `bridge`, the SW violator test requires both neighbour conditions (AND) and there is no false-corner or corner-in-box test.

(Magic ref: cif/CIFgen.c:4124-4725, cif/CIFgen.c:103-120, cif/CIFgen.c:5243-5267, cif/CIFtech.c:1218-1236)

#### Scenario: Narrow tile widened

- **GIVEN** cur = `[0,2]×[0,20]`, L a mask layer with no geometry, `bridge-lim 4 6 L`
- **WHEN** applied
- **THEN** pass 0 paints `[-4,2]×[0,20]` (widened leftward from its right edge to width 6); passes 1 and 2 find no corners
- **AND** the result is `[-4,2]×[0,20]`

#### Scenario: Catecorner gap without limits (verify against Magic)

- **GIVEN** cur = `[0,10]×[0,10] ∪ [12,22]×[12,22]`, L an empty mask layer, `bridge-lim 5 4 L`
- **WHEN** applied
- **THEN** pass 1 at the NE corner (10,10) finds V = `[12,22]×[12,22]` (dx = dy = 2, 8 < 25); c1 = `[10,16]×[6,12]` is limit-free
- **AND** the result is cur ∪ `[10,16]×[6,10]` ∪ `[12,16]×[6,12]`

#### Scenario: First candidate blocked by a limit (verify against Magic)

- **GIVEN** the same cur, L a mask layer with one tile `[13,30]×[0,8]`, `bridge-lim 5 4 L`
- **WHEN** applied
- **THEN** c1 = `[10,16]×[6,12]` overlaps L; c2 = `[6,12]×[10,16]` is limit-free
- **AND** the result is cur ∪ `[6,12]×[12,16]` ∪ `[6,10]×[10,16]`

#### Scenario: All candidates blocked (verify against Magic)

- **GIVEN** the same cur, L with tiles `[13,30]×[0,8]` and `[6,14]×[15,30]`, `bridge-lim 5 4 L`
- **WHEN** pass 1 processes the NE corner of `[0,10]×[0,10]`
- **THEN** c1 and c2 both overlap L, c3 = `[6,16]×[6,16]` is painted, and then both L tiles are erased in full
- **AND** with `magic-compat` ON the part `[12,14]×[15,22]` of the original square `[12,22]×[12,22]` is also removed (Q-MASK-229)
- **AND** with `magic-compat` OFF only the parts of the L tiles inside c3, `[13,16]×[6,8]` and `[6,14]×[15,16]`, are erased, so of the strip `[12,14]×[15,22]` only `[12,14]×[15,16]` is removed from the original square

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

`not-square` SHALL keep every rectangular solid tile of cur except tiles that are square (width = height) and have no solid neighbour on any side; split tiles are handled according to the `magic-compat` setting (Q-MASK-7, resolution: compat).

Compatibility: exact

- With `magic-compat` ON (the default), as in Magic: split tiles are dropped, and all four neighbour checks test the neighbour's bottom type.
- With `magic-compat` OFF: split tiles are copied into the result unchanged, with their diagonal (a triangle is never square), and each neighbour check tests the neighbour's type on the side facing the tile (right type of a left neighbour, left type of a right neighbour, bottom type of a top neighbour, top type of a bottom neighbour).

The same rule applies to `not-square` in mask input styles (Q-MASK-330, capability `mask-input-styles`).

(Magic ref: cif/CIFgen.c:4727-4818, cif/CIFgen.c:5420-5431)

#### Scenario: Isolated square removed

- **GIVEN** cur = `[0,10]×[0,10] ∪ [20,30]×[0,15]`
- **WHEN** `not-square`
- **THEN** the result is `[20,30]×[0,15]`

#### Scenario: Triangle kept with magic-compat OFF

- **GIVEN** cur = a solid triangle with bounding box `[0,10]×[0,10]` and right angle at (0,0), and the rectangle `[20,30]×[0,15]`
- **WHEN** `not-square` is applied with `magic-compat` OFF
- **THEN** the result is the triangle and `[20,30]×[0,15]`
- **AND** with `magic-compat` ON the result is `[20,30]×[0,15]`

### Requirement: overlapping, nonoverlapping, interacting, noninteracting

These operators SHALL keep or discard whole regions of cur according to whether any tile of the region overlaps (`overlapping`) or overlaps-or-touches (`interacting`) the geometry of the listed layers; the `non` forms keep exactly the complementary set of regions.

Compatibility: exact

- Regions are seeded only from solid tiles of cur overlapping the search area E as given in internal units (not scaled to mu, as CIFgen.c cifInteractingRegions searches it), in both `magic-compat` settings, but extend without limit.
- Per tile rectangle r (mu): for the touching forms, r is grown by 1 mu on every side (diagonal corner contact therefore counts).
- Mask-layer test: any solid of the listed mask layer planes overlaps r.
- Database-type test: r is converted to internal units by dividing each coordinate by scaleFactor, then any tile of a listed type overlaps it. The division depends on the `magic-compat` setting (Q-MASK-21, resolution: compat): with it ON (the default) it is C truncation toward zero, as in Magic; with it OFF it rounds outward, floor for lower/left edges and ceil for upper/right edges, so a database tile reached by r is never missed.
- Kept regions are copied tile by tile into the result; others are discarded.

(Magic ref: cif/CIFgen.c:4822-4996, cif/CIFgen.c:4057-4140, cif/CIFtech.c:1131-1146)

#### Scenario: Touching counts only for interacting

- **GIVEN** scaleFactor 1, cur regions `[0,10]×[0,10]` and `[50,60]×[0,10]`, and layer M with `[10,20]×[0,10]`
- **WHEN** `overlapping M` is applied
- **THEN** the result is empty
- **AND** `interacting M` yields `[0,10]×[0,10]`, `noninteracting M` yields `[50,60]×[0,10]`

#### Scenario: Database test at negative coordinates

- **GIVEN** scaleFactor 10, a cur region `[-25,-15]×[0,10]` mu, and a `poly` tile `[-3,-2]×[0,1]` internal units
- **WHEN** `overlapping poly` is applied
- **THEN** with `magic-compat` ON r converts to `[-2,-1]×[0,1]`, which does not overlap the poly tile, and the result is empty
- **AND** with `magic-compat` OFF r converts to `[-3,-1]×[0,1]`, which overlaps it, and the region is kept

### Requirement: net

`net <name> [layers]` SHALL OR into cur the geometry of the listed types belonging to the electrical net that contains the label `<name>` in the cell being generated, found by hierarchical connectivity.

Compatibility: behavioral

The net is located by label name as by the net-finding command (see capability `cmd-selection` / selection), all electrically connected paint in the hierarchy is collected into a flat scratch cell, and tiles of the operation's types (overlapping the search area) are scaled and ORed into cur. With no layer list nothing is added, in both `magic-compat` settings (Q-MASK-22, resolution: replicate). CIF and temp layers in the list contribute their whole plane, not only the net. The style loader warns about a `net` operator without layers and about one whose layer list names CIF or temp layers. If the name is not found nothing is added.

(Magic ref: cif/CIFgen.c:5433-5456, cif/CIFtech.c:1365-1372)

#### Scenario: Net extraction

- **GIVEN** a cell with label `VDD` on metal1 connected through via1 to metal2
- **WHEN** a layer `templayer PWR` with `net VDD metal1,metal2` is generated
- **THEN** PWR contains exactly the metal1 and metal2 tiles of that net

### Requirement: tagged

`tagged <text> [layers]` SHALL OR into cur every connected region of the listed layers (or, without a list, of cur itself — in which case cur is replaced by the selected regions) that is touched by a label of the cell being generated whose text equals `<text>` exactly.

Compatibility: exact

For each label of the cell (own labels only, in list order) with matching text, its rectangle (degenerate dimensions widened by 1 on each side) is used as a search area. The units depend on the `magic-compat` setting (Q-MASK-6, resolution: compat):

- With `magic-compat` ON (the default), as in Magic: database-type planes are searched with the area divided by scaleFactor; mask planes (or cur) are searched with the area as given — i.e. internal-unit label coordinates applied to mask-unit planes.
- With `magic-compat` OFF: the label rectangle is first multiplied by scaleFactor, so database-type planes are searched in internal units and mask planes in mask units.

Each found tile seeds a `bloat-all`-style flood fill of the same layers (no clip). The same unit rule applies to `tagged` in mask input styles (capability `mask-input-styles`).

(Magic ref: cif/CIFgen.c:5458-5599, cif/CIFgen.c:4057-4140)

#### Scenario: Tagged with scale 1

- **GIVEN** scaleFactor 1, cur regions `[0,10]×[0,10]` and `[20,30]×[0,10]`, a label `A` at point (25,5)
- **WHEN** `tagged A`
- **THEN** cur becomes `[20,30]×[0,10]`

#### Scenario: Tagged with scale 10

- **GIVEN** scaleFactor 10, cur regions `[0,100]×[0,100]` and `[200,300]×[0,100]` mu, and a label `A` at point (25,5) internal units
- **WHEN** `tagged A`
- **THEN** with `magic-compat` ON the search area `[24,26]×[4,6]` is used unscaled on cur and touches `[0,100]×[0,100]`, so cur becomes `[0,100]×[0,100]`
- **AND** with `magic-compat` OFF the search area is `[240,260]×[40,60]` and cur becomes `[200,300]×[0,100]`

### Requirement: bbox

`bbox [top]` SHALL OR into cur the cell's bounding box scaled to mu; with `top`, nothing is added if the cell has at least one parent instance in a non-internal cell.

Compatibility: exact

The bounding box is that of the cell being generated (the cell designated for generation errors), in both `magic-compat` settings (Q-MASK-23, resolution: replicate): the union of its paint, the bounding boxes of its subcell uses and the anchor rectangles of its labels, without the extents of rendered label text. With `top`, nothing is added when the cell has a parent use in a cell that is not an internal scratch cell. Nothing is added when no cell is designated for generation errors (e.g. some preview paths).

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

The property value is a set of rectangles (property type "plane"; textual form is groups of four integers `llx lly urx ury`, see capability `cell-file-format`). The property name is truncated to 511 characters. The full operator semantics (argument count, interaction-distance cut-off, hint flattening for flattened regions) are owned by capability `mask-hints` requirement "Mask-hint output operator"; this entry is a summary for the operator catalog.

(Magic ref: cif/CIFgen.c:5682-5705, cif/CIFgen.c:4998-5020)

#### Scenario: Hints

- **GIVEN** scaleFactor 1 and `MASKHINTS_HVI` = `0 0 10 10 20 0 30 10`
- **WHEN** `mask-hints HVI`
- **THEN** cur gains `[0,10]×[0,10] ∪ [20,30]×[0,10]`

### Requirement: Hierarchical-mode truncation

When a layer is evaluated in hierarchical mode (interaction areas and array processing), evaluation of that layer's operation list SHALL stop, returning cur as computed so far, at the first `net`, `tagged`, `boundary` or `bbox` operation, and at the first `squares`, `squares-grid` or `slots` operation that is not immediately followed by a `grow` operation.

Compatibility: exact

The truncation is the same in both `magic-compat` settings (Q-MASK-26, resolution: replicate). How the truncated result is used is specified in capability `mask-hierarchical-generation` (Q-MASK-402).

(Magic ref: cif/CIFgen.c:5332-5380, cif/CIFgen.c:5433-5440, cif/CIFgen.c:5601-5641, cif/CIFgen.c:5714-5718)

#### Scenario: Squares stop

- **GIVEN** layer `CONT` = `or pc` `squares 5 17 17`
- **WHEN** evaluated in hierarchical mode
- **THEN** the returned plane is the unsquared `pc` geometry

### Requirement: Non-Manhattan merge option

When the `unfracture` switch is on, each generated layer's plane SHALL be post-processed by merging split tiles into larger non-Manhattan tiles before clipping, as specified in capability `mask-hierarchical-generation` requirement "Non-manhattan merge option", which owns the switch, its default (off) and its command syntax (see Q-MASK-413). The covered point set is unchanged; only the tile decomposition (and hence polygon partitioning in output) changes.

Compatibility: exact

The merge procedure is defined in capability `layout-geometry-model`.

(Magic ref: cif/CIFgen.c:49, cif/CIFgen.c:5797-5799, commands/CmdCD.c:2060-2080)

#### Scenario: Default off

- **GIVEN** a fresh session in which the `unfracture` switch has never been set
- **WHEN** a layer is generated from split tiles
- **THEN** no merge is applied and the layer keeps its generated tile decomposition
