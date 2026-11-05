# Router: Maze Router Engine and mzrouter Technology Section

## Purpose

Defines Magic's maze router ("mzrouter"): the `mzrouter` technology section (styles, route layers, route contacts, spacings, search parameters, widths), the derivation of widths and spacings from DRC rules, the blockage-plane model, hint/fence/rotate layers, the cost model, the windowed best-first search with blooming, estimation, termination/result codes, and conversion of the found path into paint. The engine is used by the interactive router (`iroute`, capability `router-maze-iroute`), by gate-array and maze stems (`router-gate-array`), and by the `*mzroute` diagnostics. Because the search is a heuristic whose exact visit order determines the result, Hefesto must replicate the ordering rules described here to obtain identical routes.

## Requirements

### Requirement: mzrouter section grammar
The `mzrouter` technology section SHALL be parsed with this grammar (keywords exact, case-sensitive; one statement per line):

```ebnf
mzrouter_section = "mzrouter" , NL , { stmt , NL } , "end" ;
stmt        = style | layer | contact | notactive | spacing | search | width ;
style       = "style" , name ;
layer       = "layer" , type , hCost , vCost , [ jogCost , [ hintCost , [ overCost ] ] ] ;
contact     = "contact" , type , layerType1 , layerType2 , cost ;
notactive   = "notactive" , type , { type } ;
spacing     = "spacing" , routeType , ( type | "SUBCELL" ) , ( int | "NIL" ) , { ( type | "SUBCELL" ) , ( int | "NIL" ) } ;
search      = "search" , rate , width , penalty ;
width       = "width" , routeType , int , [ int ] ;
hCost, vCost, jogCost, hintCost, overCost, cost = positive_int ;
rate, width_int = positive_int ;  penalty = nonnegative_decimal ;
```

Rules:
- Every statement other than `style` requires a preceding `style` line; otherwise `Missing style line.` is reported. Each `style` line starts a new style with default parameters (next requirement); the previous style is closed. Unknown keywords report `Unrecognized keyword: "%s"`.
- `layer`: 4 to 7 fields; the type must be valid and on a plane not already used by another route layer of the style (`Attempt to define two route layers on same plane:  %s and %s`). Optional costs default to 1. Any non-numeric or non-positive cost aborts the line (the layer is not defined).
- `contact`: exactly 5 fields; both layer types must already be route layers of the style (`route layer must be declared before used in contact.`); cost must be a positive integer. See Q-LEGACY-31 for partially-applied contacts.
- `notactive`: marks listed route layers/contacts inactive by default (unknown ones report `Unrecognized route type: "%.20s"`).
- `spacing`: even field count >= 4; the second word of each pair may be any type, or a case-insensitive prefix of `subcell`; values are non-negative integers or the literal `NIL` (= no restriction, stored as -1). Explicit spacings are applied after the DRC-derived values (see derivation).
- `search`: exactly 4 fields; sets window rate and window width (positive integers) and the penalty factor (stored as the integer `floor(p * 2)` interpreted as `/2`, i.e. 0.5 resolution; see Q-LEGACY-32).
- `width`: sets route-type width and optional minimum length (default = width) — overridden later by DRC derivation (see Q-LEGACY-33).
- Route layers, contacts and types are kept in the order they appear in the file.

Example (scmos):

```
mzrouter
  style irouter
    layer       m2      32       64     256     1
    layer       m1      64       32     256     1
    layer       poly    128     128     512     1
    contact     m2contact       metal1  metal2  1024
    contact     pcontact        metal1  poly    2056
    notactive poly pcontact
  style garouter
    layer       m2      32       64     256     1
    layer       m1      64       32     256     1
    contact     m2contact       metal1  metal2  1024
end
```

Compatibility: exact
(Magic ref: mzrouter/mzTech.c:180-260, mzrouter/mzTech.c:300-800, scmos/scmos.tech.in mzrouter section)

#### Scenario: Layer defaults
- **GIVEN** `layer m2 32 64` inside a style
- **WHEN** the line is parsed
- **THEN** metal2 is a route layer with hCost 32, vCost 64, jogCost 1, hintCost 1, overCost 1, active

#### Scenario: Statement before style
- **GIVEN** an mzrouter section whose first line is `layer m1 64 32`
- **WHEN** the section is read
- **THEN** `Missing style line.` is reported and nothing is defined

### Requirement: Style defaults and lookup
Each style SHALL start with: penalty 1024.0, window rate 500, window width 10000, bloom delta cost 1, bounds increment AUTOMATIC, estimate on, expand endpoints on, top-level hints only off, max walk length (penetration) AUTOMATIC, no bounds hint, verbosity 1, bloom limit 0 (unlimited). Styles are looked up by exact name; the interactive router uses a private copy of style `irouter`, the gate-array router a copy of `garouter`. Changing the copy (via `iroute` subcommands) does not alter the technology style.

Compatibility: exact
(Magic ref: mzrouter/mzTech.c:262-300, mzrouter/mzInternal.h:60-120, mzrouter/mzMain.c:272-440, irouter/irMain.c:120-160)

#### Scenario: Default window width
- **GIVEN** a style with no `search` line
- **WHEN** its parameters are inspected
- **THEN** rate is 500, width is 10000 and penalty is 1024.0

### Requirement: Width and spacing derivation from DRC
After the technology (including DRC) is loaded or the grid rescaled, the engine SHALL recompute, for every route type t of every style:
1. `width(t)` = the distance of the last forward, non-area, non-trigger, non-maxwidth width rule on t's plane whose type set contains t and equals its corner set and whose distance equals its corner distance (0 if none); `length(t) = width(t)`.
2. For every tile type u: `spacing(t,u)` = the distance of the last forward non-area non-trigger spacing rule from t to space whose type set excludes u, lies on a plane of u, and whose distance equals its corner distance (0 if none).
3. Explicit `spacing` statements for real types override.
4. `spacing(t, SUBCELL)` = max over all types of `spacing(t,·)`; then explicit SUBCELL spacings override.

Spacing semantics: -1 = no restriction; 0 = may not overlap; s > 0 = must be at least s away.

Compatibility: exact
(Magic ref: mzrouter/mzTech.c:150-228, drc/DRCtech.c:4620-4745)

#### Scenario: Width from DRC
- **GIVEN** a DRC rule `width allMetal2 3` and route layer metal2
- **WHEN** derivation runs
- **THEN** the metal2 route width and length are 3

### Requirement: Derived search quantities
Before each route the engine SHALL compute:
- For a route layer: `effWidth = width`; for each type u with `spacing >= 0`: `bloatBot(u) = width + spacing(u) - 1`, `bloatTop(u) = spacing(u)`; otherwise both -1.
- For a route contact: `effWidth = max(width, width(layer1), width(layer2))`; for each type u, its own pair `(width + spacing(u) - 1, spacing(u))` (or `(-1,-1)` when `spacing(u) < 0`) and both layers' pairs are computed the same way and `bloatBot(u)`, `bloatTop(u)` are the component-wise maxima of the three pairs.
- `contextRadius` = max `bloatBot(u)` over all active route types and all u (start value 0).
- Max walk length AUTOMATIC → `2 * contextRadius`.
- Bounds increment AUTOMATIC → `30 * min over active layers (width + spacing(layer, its own type))`, or 100 if no layer is active.
- Bounding rectangle `B`: the bounds hint expanded by `2*contextRadius` on every side, or else the whole representable plane shrunk on every side by `3 * (maxSpacing + maxWidth + 2)`, where the maxima are over all route types of the style (active or not) and all spacing entries.

Enumeration orders (these determine tie-breaking everywhere below):
- *Layer order*, *contact order* and *type order* = the order of the `layer` / `contact` statements in the style (style copies made for `iroute`/`garoute` preserve it).
- *Active layer order* and *active type order* = the REVERSE of the layer / type order, restricted to active entries.
- *Contacts of a layer* = the contacts naming that layer, in REVERSE contact order, including inactive ones (callers skip inactive contacts where stated).

Compatibility: exact
(Magic ref: mzrouter/mzSubrs.c:60-270, mzrouter/mzTech.c:455-495, mzrouter/mzTech.c:1170-1205, mzrouter/mzMain.c:280-370)

#### Scenario: Bounds increment
- **GIVEN** active layers with (width, self-spacing) of (3,3) and (4,4)
- **WHEN** a route starts with bounds increment AUTOMATIC
- **THEN** the bounds increment is 180

#### Scenario: Contact bloat
- **GIVEN** contact width 4 with spacing(contact, metal1) = 2, layer metal1 width 3 with spacing(metal1, metal1) = 3, layer metal2 width 3 with spacing(metal2, metal1) = -1
- **WHEN** derived quantities are computed
- **THEN** for u = metal1 the contact has `bloatBot = max(4+2-1, 3+3-1, -1) = 5` and `bloatTop = max(2, 3, -1) = 3`

#### Scenario: Active order is reversed
- **GIVEN** a style declaring `layer m2 ...` then `layer m1 ...`, both active
- **WHEN** the active layer order is formed
- **THEN** metal1 is visited before metal2

### Requirement: Blockage model
For each route type the engine SHALL maintain blockage planes in which a point is legal iff a wire of that type with its lower-left corner at that point creates no spacing violation. Each route type has two blockage planes holding the same regions, one decomposed into maximal horizontal strips (the *h-plane*) and one into maximal vertical strips (the *v-plane*); tile boundaries of both forms are significant for the search (see "Interesting points"). Every point of a blockage plane carries one of the following labels, listed in increasing priority:

`free` < `same-node` < `same-node-block` < `above-UD-walk` < `below-UD-walk` < `above-LR-walk` < `below-LR-walk` < `left-walk` < `right-walk` < `top-walk` < `bottom-walk` < `destination` < `blocked`.

Painting a label L over a point with label P SHALL give `max(L, P)` (painting `free` is never done). Hence the painting order of non-free labels never matters.

The engine paints into the planes as follows (r is the rectangle being painted; all rectangles are clipped to the current *build area* A described in "Lazy blockage generation"):

```
paintBlock(r, u, label):                       # u = tile type of the source material
  for t in active types (active type order):
    if u and t share no plane: continue
    if bloatBot_t(u) < 0: continue             # u unrestricted for t
    L = label
    if label != same-node:
      if h-plane_t at r.ll is same-node:
        if t is a layer: continue              # material of the start/dest node never blocks
        if t is a contact and u is layer1 or layer2 of the first contact
           (contact order) whose type is t: continue          # see Q-LEGACY-89
      paint [r.xbot - bloatBot, r.ybot - bloatBot, r.xtop + bloatTop, r.ytop + bloatTop] with L
    else:
      if u is a contact type and t == u: L = blocked          # see Q-LEGACY-88
      wl = bloatBot_t(u) - bloatTop_t(u) + 1
      paint [r.xbot - wl, r.ybot, r.xtop, r.ytop - wl] with same-node
      paint [r.xbot, r.ybot - wl, r.xtop - wl, r.ytop] with L
```

Sources of paint:
1. Start node: every start tile (see "Start and destination registration") with `paintBlock(tile, type, same-node)` clipped to B, once before the search.
2. Destination node: every tile of the destination-area set (see "Destination areas") with `paintBlock(tile, type, same-node)`, during lazy generation.
3. Mask data: every non-space tile of the route cell hierarchy visible through the expansion mask within `A` expanded by `contextRadius`, clipped to that search area, with `paintBlock(tile, type, blocked)`.
4. Unexpanded subcells (only when the expansion mask is non-zero): bounding box with `paintBlock(bbox, SUBCELL, blocked)`, or `same-node` for a subcell use marked as part of the destination.
5. Fences: when the first start point is inside fence paint, every fence-free tile, else every fence tile, of the fence plane within `A` expanded by `contextRadius`: for every active type t (regardless of plane) paint `[xbot - (effWidth_t - 1), ybot - (effWidth_t - 1), xtop, ytop]` with `blocked`.

Contact blockage is the union of both layers' blockage plus its own because the contact's bloats are the maxima. Fence semantics and start-point parity are as before (see Q-LEGACY-30).

Compatibility: exact
(Magic ref: mzrouter/mzBlock.c:140-650, mzrouter/mzInit.c:180-260, mzrouter/mzInternal.h:140-170, mzrouter/mzMain.c:571-640)

#### Scenario: Bloat of a metal obstacle
- **GIVEN** route layer metal1 width 3 with spacing(metal1, metal1) = 3, and a metal1 obstacle (10,10)-(20,20)
- **WHEN** blockage is generated
- **THEN** the metal1 blockage covers (5,5)-(23,23)

#### Scenario: Same-node shape
- **GIVEN** route layer metal1 width 3, spacing(metal1, metal1) = 3, and a start tile metal1 (10,10)-(20,20)
- **WHEN** it is painted as same-node
- **THEN** `wl = 5 - 3 + 1 = 3` and the metal1 planes get same-node over (7,10)-(20,17) and (10,7)-(17,20)

#### Scenario: Priority
- **GIVEN** a point labelled `left-walk`
- **WHEN** `same-node` and then `blocked` are painted over it
- **THEN** the label is `left-walk` after the first paint and `blocked` after the second

### Requirement: Lazy blockage generation
Blockage SHALL be generated lazily using two *bounds planes* (horizontal-strip and vertical-strip forms) whose points are either `generated` or not. Generating around a rectangle R:

```
extendBounds(R):
  G = R expanded by 2 * boundsIncrement on every side
  pending = maximal-horizontal-strip pieces of G not yet generated (in plane enumeration order)
  for each piece P in pending:
    if a bounds hint H exists: P = P ∩ H; skip P if empty (xbot > xtop or ybot > ytop)
    A = P expanded by 2 on every side
    paint mask data, start/destination same-node and subcell blockage over A   (Blockage model, items 2-4)
    paint fence blockage over A                                               (item 5)
  mark all of G generated in both bounds planes
```

Generation is triggered: (a) once at each start term's lower-left corner before the search; (b) for every destination-area rectangle before destination labels are painted; (c) when a path is popped for extension and its end point lies in a non-generated h-bounds tile or on that tile's left or right edge, or on the bottom or top edge of the v-bounds tile containing it; (d) during extension (see "Interesting points") until the blockage tile containing the start point is covered. Because bounds-tile edges are themselves interesting points, Hefesto MUST reproduce this generation schedule exactly.

Compatibility: exact
(Magic ref: mzrouter/mzBlock.c:651-840, mzrouter/mzSearch.c:236-280, mzrouter/mzMain.c:820-850)

#### Scenario: First generation window (verify against Magic)
- **GIVEN** bounds increment 180, no bounds hint, and a start term with lower-left (0,0)
- **WHEN** the route is initialized
- **THEN** blockage is built over (-362,-362)-(362,362) (the window (-360,-360)-(360,360) grown by 2) and that window is marked generated, so no rightward extension from (0,0) can end beyond x = 360 until more blockage is generated

### Requirement: Hint, fence and rotate layers
The built-in layers `magnet` (alias `mag`), `fence` (`f`) and `rotate` (`r`), available in every technology, SHALL be read from the route cell (top-level cell only when "top hints only" is set, otherwise the whole expanded hierarchy) and SHALL affect routing as follows: within `rotate` paint, horizontal segments are charged the layer's vCost (and vertical segments its hCost); a segment parallel to and not inside magnet paint is charged an extra `hintCost × length × distance` to the nearest magnet edge in its perpendicular direction (distance 0 when no magnet bounds the space in that direction); fences act as described in "Blockage model".

Compatibility: exact
(Magic ref: mzrouter/mzHint.c:40-197, mzrouter/mzXtndRght.c:470-520, database/DBtechtype.c:85-95)

#### Scenario: Rotate swaps costs
- **GIVEN** route layer m2 with hCost 32 and vCost 64 and a horizontal segment of length 10 inside rotate paint
- **WHEN** the segment cost is computed
- **THEN** the cost is 640

### Requirement: Start and destination registration
Start and destination areas SHALL be registered as follows:
- `addStart(point, type)`: on the first start, record whether the point is inside fence paint; if inside, shrink B to the fence plane's bounding box grown by `2*contextRadius` (intersected with B). A later start whose inside/outside status differs is ignored with a message. The connected set is then marked from the zero-size rectangle at the point.
- `addDest(rect, type)`: if endpoints are not expanded, paint into the destination-area set, at `rect`, every active route layer type that connects to `type` (layer order). Then mark the connected set.
- Marking a connected set (start or destination): breadth-first from the given rectangle; for each queued (rect, type) that overlaps B, find all tiles of types connecting to that type in the route-cell hierarchy (expansion mask applied) within the rectangle grown by 1; each tile not yet marked for this purpose is marked, queued, and — when expanding endpoints — added to the start term list (start; prepended, so later-found tiles come first) or, for destinations, every active route layer type connecting to its type is painted into the destination-area set over the tile. Subcell uses (expansion mask non-zero) overlapping the given rectangle within the cell bbox and B are marked as destination subcells.

Compatibility: exact
(Magic ref: mzrouter/mzMain.c:571-710, mzrouter/mzSubrs.c:290-530)

#### Scenario: Unexpanded destination
- **GIVEN** endpoint expansion off, active layers metal1 and metal2, and `addDest((0,0)-(4,4), m2contact)`
- **WHEN** the destination is registered
- **THEN** the destination-area set holds metal1 and metal2 over (0,0)-(4,4)

### Requirement: Destination areas
Before the search, for every tile D (type t, rectangle `[x1,y1,x2,y2]`) of the destination-area set within B, in enumeration order, the engine SHALL:
1. Generate blockage around D (Lazy blockage generation). If t is not an active route type the destination processing stops for all remaining tiles.
2. Paint `destination` on t's planes over `[x1, y1 - w, x2 - w, y2]` and `[x1 - w, y1, x2, y2 - w]` where `w = width(t)`.

Then, in a second pass over the same tiles, for each destination tile D of type t and each `destination`-labelled tile T of t's h-plane overlapping D:
- insert `T.xbot` and `T.xtop` into the x-alignment set; for each tile N adjoining T's left edge (bottom to top) labelled `same-node`, add a `left-walk` region `[max(N.xbot, N.xtop - maxWalk), max(T.ybot, N.ybot), N.xtop, min(T.ytop, N.ytop)]`; for each tile N adjoining T's right edge labelled `same-node`, a `right-walk` region `[N.xbot, max(T.ybot,N.ybot), min(N.xtop, N.xbot + maxWalk), min(T.ytop,N.ytop)]`.
- for each `destination` tile T of t's v-plane overlapping D: insert `T.ybot`, `T.ytop` into the y-alignment set; same-node tiles adjoining the bottom edge give `bottom-walk` regions `[max(T.xbot,N.xbot), max(N.ybot, N.ytop - maxWalk), min(T.xtop,N.xtop), N.ytop]`; adjoining the top edge give `top-walk` regions `[.., N.ybot, .., min(N.ytop, N.ybot + maxWalk)]`.
- for each `destination` tile T of t's h-plane overlapping D and each active contact C having t as layer1 (label `above-LR-walk`) or layer2 (label `below-LR-walk`): every `free` or `same-node` tile of C's h-plane overlapping T, clipped to T, becomes a walk region with that label on the OTHER layer of C. The same with v-planes gives `above-UD-walk` / `below-UD-walk`.

All walk regions are painted after the pass (both plane forms of the walk's route type). If the x-alignment set is still empty the result is "unroutable". The alignment sets always also contain −∞ and +∞. See Q-LEGACY-93.

Compatibility: exact
(Magic ref: mzrouter/mzBlock.c:842-1487, mzrouter/mzNumLine.c:60-240, mzrouter/mzMain.c:742-760)

#### Scenario: Destination labels
- **GIVEN** route layer metal1 width 3 and destination tile metal1 (10,10)-(20,20)
- **WHEN** destination areas are built
- **THEN** metal1 gets `destination` over (10,7)-(17,20) and (7,10)-(20,17), and x-alignment includes the h-plane destination tile edges (e.g. 7, 10, 17, 20)

#### Scenario: Walk length cap
- **GIVEN** max walk length 12 and a same-node tile (−40,10)-(7,17) adjoining a destination tile on its left
- **WHEN** walks are generated
- **THEN** a `left-walk` region (−5,10)-(7,17) is painted

### Requirement: Cost estimation
When estimation is on the engine SHALL build an estimate plane exactly as follows; when off, steps 1 and 2 are skipped (no solid tiles):

```
1. if expansion mask != 0 and no active layer has spacing(layer, SUBCELL) < 0:
     paint EST_SUBCELL over the bbox of every unexpanded subcell within B ∩ routeCell.bbox
2. paint EST_FENCE over every fence tile (or every fence-free tile when starting inside) within B
3. for each destination-area tile D within B: search t's h-plane within D grown by maxWalk;
     destination tiles → paint EST_DEST; walk tiles (left/right/top/bottom) → paint SPACE (erase)
   (paint priority: EST_SUBCELL < EST_FENCE < EST_DEST; painting SPACE erases)
4. S = all non-space tiles (enumeration order), processed in REVERSE of that order;
     for each solid s: split(ll), split(xbot, ytop), split(xtop, ytop), split(xtop, ybot)
5. costs: EST_DEST tiles h = v = 0; space tiles h = min hCost, v = min vCost over active layers;
     EST_SUBCELL/EST_FENCE tiles h = v = ∞ (INT_MAX)
6. corner Dijkstra (below); 7. estimators per tile (below); 8. prune dominated estimators (below)
```

`split(p)` (ignored if p has an infinite coordinate): extend a horizontal cut at `y = p.y` rightward from the tile containing p through consecutive space tiles not already having that bottom, stopping at the first non-space tile or the plane edge; the same leftward at `y`; extend vertical cuts at `x = p.x` upward and downward through space tiles; finally, if the tile containing p is space, cut it at x and y.

Corner Dijkstra: vertices are all tile corners with finite coordinates; from a vertex, an edge runs along each tile boundary line to the next corner in each of the four directions; its weight is `distance × min(cost of the two tiles flanking that segment)` (h-cost for horizontal edges, v-cost for vertical ones) and the edge is absent if that minimum is ∞ or the far end is infinite. Sources: the lower-left corner of every EST_DEST tile within B, cost 0. The result `cost0(v)` is the least total weight (COST_MAX = (2^63−1)>>2 when unreachable). Tie order does not affect the result. See Q-LEGACY-90 for 32-bit products.

Estimators of a tile T (tile costs h, v):
- Corner estimators for each finite corner c of T: `(x0,y0) = c, hcost = h, vcost = v, cost0 = cost0(c)`.
- Straight-shot estimators: follow from T through the chain of right neighbours at T's top-right (each step: the tile adjoining the right edge at its top) while tiles are space; if the chain ends at an EST_DEST tile D, add `(x0 = T.xtop, y0 = 0, hcost = h, vcost = 0, cost0 = (D.xbot − T.xtop) × h)` (COST_MAX if h = ∞). Likewise leftward via the left-edge-bottom neighbour chain (`x0 = T.xbot, cost0 = (T.xbot − D.xtop) × h`), upward via the top-edge-right neighbour chain (`y0 = T.ytop, hcost = 0, cost0 = (D.ybot − T.ytop) × v`), downward via the bottom-edge-left chain (`y0 = T.ybot, cost0 = (T.ybot − D.ytop) × v`). See Q-LEGACY-96.

Pruning: process T's estimators in list order (most recently created first: straight shots down, up, left, right, then corners UL, UR, LR, LL); an estimator e is dropped if some already-kept estimator or some later estimator e2 is *always as good*: `cost0(e2) <= cost0(e)` and e2's costs are finite and `h2 × |x0(e) − x0(e2)| + v2 × |y0(e) − y0(e2)| + cost0(e2) <= cost0(e)`, where, before this test, if `hcost(e) = 0` x0(e) is replaced by whichever of T.xbot/T.xtop is farther from x0(e2), and if `vcost(e) = 0` y0(e) likewise with T.ybot/T.ytop (the replacement persists).

Estimate at point p: `min over estimators of T(p) with finite costs of hcost×|p.x−x0| + vcost×|p.y−y0| + cost0` in 64-bit arithmetic, or COST_MAX if none.

Compatibility: exact
(Magic ref: mzrouter/mzEstimate.c:20-1530, mzrouter/mzEstimate.c:1530-1900, mzrouter/mzEstimate.c:1900-1960)

#### Scenario: Open-field estimate
- **GIVEN** no obstacles, one active layer with hCost 1 and vCost 2, destination point (100, 50)
- **WHEN** the estimate at (0,0) is computed
- **THEN** it equals `100*1 + 50*2 = 200`

#### Scenario: Straight-shot estimate
- **GIVEN** one active layer hCost 1, vCost 2 and a single EST_DEST tile (100,50)-(101,51)
- **WHEN** the estimate at (0,50) is computed
- **THEN** the containing tile is (−∞,50)-(100,51), its right straight shot gives `|0 − 100|×1 + 0 = 100`, the corner (100,51) gives 102, and the estimate is 100

#### Scenario: Subcell detour (verify against Magic)
- **GIVEN** estimation on, hCost = vCost = 1, an opaque unexpanded subcell (40,−50)-(60,50) and a destination tile (100,0)-(101,1)
- **WHEN** the estimate at (0,0) is computed
- **THEN** the containing tile is (−∞,−50)-(40,50); its corner (40,−50) gives `40 + 50 + 110 = 200`, its corner (40,50) gives `40 + 50 + (20 + 40 + 49) = 199`, so the estimate is 199 (the unobstructed value 101 is not available)

### Requirement: Initial paths
For each start term (start term list order) the engine SHALL generate blockage at its lower-left and create initial paths:

```
srect = term.rect with xbot-1, ybot-1
L = first active layer (active layer order) whose type == term.type
if L is none and term.type is a contact:
  for C in contacts (contact order), active, whose layer1 and layer2 types both connect to term.type:
    p = lower-left of the first tile (h-plane of C.layer1, enumeration order) within srect labelled
        same-node, any walk, or destination;  if found: initPath(none, C.layer1, p, 0); L = C.layer2; break
if L is none: L = first active layer connecting to term.type
if L is none: no paths for this term
p = as above on L's h-plane; if found: initPath(none, L, p, 0)

initPath(prev, L, p, cost):
  lab = label of L's h-plane at p;  if blocked: return
  if prev is none: initialContacts(L, p)
  extend = {R,L,U,D,UDcontacts,LRcontacts} if free/same-node; walk labels → the matching walk code;
           destination → complete, and the route reports "already routed"
  orient = 'O' if prev none; else contact orient ('X' if prev.x == p.x else 'O') when layers differ
  addPoint(prev, p, L, orient, extend, cost)              # source = INIT

initialContacts(L, p): for C in contacts of L (active, other layer active):
  if C's h-plane at p is same-node and (tile.xtop − p.x <= length(C) − width(C)):   # Q-LEGACY-85
      initPath(pseudo(L, p, 'O', cost 0), other, p, cost(C))
  if C's v-plane at p is same-node and (tile.ytop − p.y > length(C) − width(C)):
      initPath(pseudo(L, p, 'X', cost 0), other, p, cost(C))
```

See Q-LEGACY-92 for the choice of p. After all terms, `initialEstimate` = the minimum estimated total of all initial paths actually added (COST_MAX if none), `minToGo0 = initialEstimate`, `maxToGo0 = initialEstimate + windowWidth`.

Compatibility: exact
(Magic ref: mzrouter/mzStart.c:60-416, mzrouter/mzMain.c:800-850)

#### Scenario: Start on route layer
- **GIVEN** start term metal1 (10,10)-(20,20), metal1 width 3 spacing 3, no obstacles
- **WHEN** initial paths are created
- **THEN** the first same-node tile in srect (9,9)-(20,20) is found and the initial path starts at its lower-left corner with orientation 'O' and all four directions plus both contact kinds enabled

### Requirement: Path records and the hashed-best rule
A path SHALL be a record `(point, layer, orient ∈ {H, V, O, X}, E, T, extend, predecessor)` where T = estimate to go (0 for complete paths) and E = estimated total. Adding a point (`addPoint(prev, p, L, orient, extend, c)`, c = incremental segment cost >= 0):

```
if p not inside B (closed rectangle): return
T = 0 if extend == complete else estimate(p)
E = c
if prev and prev.layer == L and prev.orient not in {O, X} and prev.orient != orient: E += jogCost(L)
if prev: E += prev.E
if source != INIT: E -= prev.T
E += T
key = (p, L, orient)
if hash[key] exists and hash[key].E <= E: return              # hashed-best rule
if source == INIT: minInitial = min(minInitial, E)
create path; hash[key] = path; dispatch it (Windowed bloom search, step 5)
```

All arithmetic is signed 64-bit. Older paths for the same key are not removed from stacks or heaps; they are skipped when popped from a stack because they are no longer the hashed path for their key. Orientation 'O' marks a layer change through an LR-checked contact, 'X' through a UD-checked contact.

Compatibility: exact
(Magic ref: mzrouter/mzSearch.c:1211-1350, mzrouter/mzSearch.c:166-200)

#### Scenario: Hash rejection on equal cost
- **GIVEN** a path stored for key ((50,10), metal1, H) with E = 900
- **WHEN** another path reaches the same key with E = 900
- **THEN** the new path is discarded

#### Scenario: Jog cost
- **GIVEN** a path on m1 (hCost 64, vCost 32, jogCost 256) arriving horizontally at P with E = 1000 and T = 400, estimate 395 at the new point
- **WHEN** it is extended vertically by 5 units
- **THEN** the new E = `5×32 + 256 + 1000 − 400 + 395 = 1411`

### Requirement: Segment cost model
The incremental cost c of a straight extension from pOrg to pNew (distance d) on layer R SHALL be:

```
rot = rotate paint present at pOrg (vertical-strip rotate plane for horizontal moves,
      horizontal-strip rotate plane for vertical moves)
c = d × (rot ? other-direction cost : overroute ? overCost(R) : this-direction cost) + hint
```

`overroute` is set during interesting-point computation (a contact type of R is `blocked`, not merely walk/destination, at the first step). Walk moves use the same formula without overroute. Contact moves cost the contact's `cost`. Hint cost for a horizontal move: iterate over the vertical-strip magnet tiles q from the left end of the segment (lowPt) to its right end; for each non-magnet q containing lowPt: `up = q.ytop − y` if the tile adjoining q's top edge at its right end is magnet else −1; `down = y − q.ybot` if the tile adjoining q's bottom edge at its left end is magnet else −1; `delta` = the smaller non-negative one (0 if both −1); add `(min(q.xtop, rightEnd) − lowPt.x) × hintCost(R) × delta` when delta > 0; continue at `q.xtop`. Vertical moves use the horizontal-strip magnet plane with right (top end) and left (bottom end) neighbours symmetrically. After pNew is chosen, if R's blockage at pNew is `same-node` and (the origin path has no predecessor or its predecessor's E is 0), the origin path's stored E is overwritten with 0 before the new point is added (see Q-LEGACY-86).

Compatibility: exact
(Magic ref: mzrouter/mzXtndRght.c:550-634, mzrouter/mzXtndLeft.c, mzrouter/mzXtndUp.c, mzrouter/mzXtndDown.c, mzrouter/mzWalk.c:60-430)

#### Scenario: Rotate swaps costs
- **GIVEN** route layer m2 with hCost 32 and vCost 64 and a horizontal segment of length 10 starting inside rotate paint
- **WHEN** the segment cost is computed
- **THEN** the cost is 640

#### Scenario: Hint cost
- **GIVEN** hintCost 1, a horizontal move on y = 10 from x = 0 to x = 20 through a non-magnet strip (−5,0)-(30,25) whose top neighbour is magnet and bottom neighbour is not
- **WHEN** the hint cost is computed
- **THEN** `delta = 25 − 10 = 15` and the hint cost is `20 × 1 × 15 = 300`

### Requirement: Interesting points
Extension from path P (point pOrg, layer R) in a direction SHALL stop at the nearest interesting point computed as below (shown for RIGHT; `prune(x, reason)`: if x < pNew then pNew = x and reasons = {reason}; if x == pNew reasons ∪= {reason}). R's planes used: v-plane for horizontal moves, h-plane for vertical moves; contact planes: h-plane in all four directions (see Q-LEGACY-94).

```
pStep = pOrg + (1,0)
pNew = right edge of the h-bounds tile containing pOrg; reasons = {BOUNDS}
# current layer
ensureCovered(R, pOrg)             # may skip the block below if blockage not generated
tThis = R tile at pOrg; tNext = tile adjoining tThis's right edge at y
if tNext is not free/same-node:
  if tNext.xbot == pStep.x:        # touching
     left-walk → pNew = pStep, reasons = {WALK}, goto done
     LR-walk (above/below) → pNew = pStep, {WALKLRC}; UD-walk → {WALKUDC}; destination → {DONE}; goto done
     otherwise: no extension in this direction
  else prune(tNext.xbot − 1, JOG)
elif (tNext.ytop < tThis.ytop or tNext.ybot > tThis.ybot) and tThis.xtop − 1 > pOrg.x:
  prune(tNext.xbot − 1, JOG)
else prune(tNext.xbot, JOG)
# other layers
for L in active layers (active order), L != R:
  if L at pOrg is not free/same-node: continue
  ensureCovered(L, pOrg)
  same three cases with reason ALIGNOTHER, except a touching blocked tile just skips L
prune(smallest x-alignment value > pOrg.x, ALIGNGOAL)
prune(right edge of the vertical-strip magnet tile at pOrg, HINT)
q = vertical-strip rotate tile at pStep: if q.xbot > pOrg.x prune(q.xbot, ROTBEFORE) else prune(q.xtop − 1, ROTINSIDE)
for C in contacts of R (all, active or not):
  q = C's h-plane tile at pStep
  if free/same-node: if q.xbot > pOrg.x prune(q.xbot, CONTACT) else prune(q.xtop − 1, CONTACT)
  else: prune(q.xtop, CONTACT); if q is blocked: overroute = true
done:
  WALK → extend = walkRight; WALKLRC → walkLRContact; WALKUDC → walkUDContact; DONE → complete
  else extend = {RIGHT} ∪ ({UDcontacts, LRcontacts} if reasons ∩ {ALIGNOTHER, CONTACT, ALIGNGOAL, HINT, ROTINSIDE})
                        ∪ ({UP, DOWN} if reasons ∩ {JOG, ALIGNGOAL, HINT, ROTINSIDE})
  addPoint(P, pNew, R, 'H', extend, segment cost)
```

`ensureCovered(L, pOrg)`: walk the v-bounds tiles along y from the one containing pOrg while `bt.xbot <= tThis.xtop` and `bt.xbot <= routeCell.bbox.xtop`: if bt is not generated → abandon this layer's checks (current layer: pNew keeps the bounds limit); else if `bt.ytop <= tThis.ytop` and `bt.ytop <= bbox.ytop` → generate around `(max(bt.xbot, pOrg.x), bt.ytop)` and restart; else if `bt.ybot >= tThis.ybot` and `bt.ybot >= bbox.ybot` → generate around `(max(bt.xbot, pOrg.x), bt.ybot)` and restart; else advance to the next bounds tile to the right.

LEFT mirrors RIGHT with `pStep = pOrg − (1,0)`, bounds = left edge of the h-bounds tile, touching test `tThis.xbot == pOrg.x`, prune targets `tThis.xbot` (blocked or constricting with `tThis.xbot < pOrg.x`) else `tThis.xbot − 1`, largest alignment value < pOrg.x, hint tile taken at pStep with target its left edge, rotate tile q at pStep: if `q.xtop − 1 < pOrg.x` prune `q.xtop − 1` (ROTBEFORE) else `q.xbot` (ROTINSIDE); contact free tile: same two targets with the same test; blocked target `q.xbot − 1`, and maxima instead of minima. UP and DOWN are the same with x↔y, using h-planes and the y-alignment set; UP has the additional rule that in the "else" (non-constricting free) case pNew is set to `tNext.ybot` unconditionally after pruning (see Q-LEGACY-87). Each extension then also emits points in the order RIGHT, LEFT, UP, DOWN, UD contacts, LR contacts, walk (by the path's extend set).

Compatibility: exact
(Magic ref: mzrouter/mzXtndRght.c:40-634, mzrouter/mzXtndLeft.c:40-639, mzrouter/mzXtndUp.c:40-632, mzrouter/mzXtndDown.c:40-634, mzrouter/mzInternal.h:460-495, mzrouter/mzSearch.c:730-800)

#### Scenario: Stop before obstacle
- **GIVEN** a path end at x = 0 on a v-plane whose space tile containing it ends at x = 12 where a blocked tile starts, bounds edge at 360, nothing else interesting
- **WHEN** it is extended to the right
- **THEN** the new point is at x = 11 with reason JOG, so straight, up and down are allowed but no contacts

#### Scenario: Ties merge reasons
- **GIVEN** a rightward extension where the JOG target and an x-alignment value are both 20
- **WHEN** pruning finishes
- **THEN** pNew = 20 with reasons {JOG, ALIGNGOAL} and the extend set includes RIGHT, UP, DOWN and both contact kinds

### Requirement: Contact extension
From path P (point p, layer R) with contact extension enabled the engine SHALL, for UD (orientation 'X', v-planes) and LR (orientation 'O', h-planes) separately:

```
last = nearest ancestor of P (including P) with orient == 'O' (LR) / 'X' (UD) that has a predecessor
lastType = the contact type joining last and its predecessor (none if no such ancestor)
bend = 0
if P.orient == 'V': a = first ancestor before P whose orient != 'V'; if a.orient == 'H': bend = a.y − p.y
if P.orient == 'H': a = first ancestor before P whose orient != 'H'; if a.orient == 'V': bend = a.x − p.x
if bend < 0: bend += width(R)
for C in contacts of R (reverse contact order); skip inactive C or inactive other layer N:
  q = C's plane tile at p; skip unless q is free or same-node
  skip if (LR: q.xtop − p.x; UD: q.ytop − p.y) <= length(C) − width(C)
  if type(C) == lastType: d = spacing(C, lastType) + width(C);
       skip if |p.x − last.x| < d and |p.y − last.y| < d
  if bend != 0: s = spacing(C, type(R)); skip if width(C) < bend < width(C) + s; skip if −s < bend < 0
  lab = N's plane (same form) at p: free/same-node → {R,L,U,D}; walk labels → walk codes; destination → complete
  addPoint(P, p, N, 'O' or 'X', extend, cost(C))
```

See Q-LEGACY-91.

Compatibility: exact
(Magic ref: mzrouter/mzSearch.c:800-1210)

#### Scenario: Contact too close to previous contact
- **GIVEN** a path whose last LR contact of type m2contact (width 4, spacing(m2contact, m2contact) = 2) is at (10,10) and the current point (14,10)
- **WHEN** LR contact extension is attempted with m2contact
- **THEN** `d = 6`, `|14−10| = 4 < 6` and `0 < 6`, so no contact is placed

### Requirement: Walk extension
A path whose extend set is a walk code SHALL emit exactly one complete path:
- walk right: `pNew = (xtop of the left-walk tile containing pOrg on the layer's h-plane, pOrg.y)`, orient 'H'; walk left: `pNew.x = xbot(right-walk tile) − 1`; walk up: `pNew.y = ytop(bottom-walk tile)` (v-plane), 'V'; walk down: `pNew.y = ybot(top-walk tile) − 1`. Cost per "Segment cost model" without overroute.
- walk LR (UD) contact: look up the walk label at pOrg on the layer's h-plane (v-plane); for C in contact order (all contacts of the style): skip inactive; for `below-*` require layer1(C) == current layer, for `above-*` require layer2(C) == current layer; skip if C's same-form plane at pOrg is `blocked`; skip if (walk tile xtop − pOrg.x, resp. ytop − pOrg.y) <= length(C) − width(C); otherwise choose C. Emit (pOrg, other layer of C, 'O' / 'X', complete, cost(C)). If none qualifies, nothing is emitted.

Compatibility: exact
(Magic ref: mzrouter/mzWalk.c:60-690)

#### Scenario: Walk into destination
- **GIVEN** a path on metal1 (hCost 64) at (−3,12) inside a left-walk tile (−5,10)-(7,17), no rotate or magnet
- **WHEN** it is walked right
- **THEN** a complete path at (7,12) with incremental cost `10 × 64 = 640` is added

### Requirement: Windowed bloom search
The search SHALL run this loop (stacks are LIFO lists; heaps per "Heap ordering"; maxToGo heap keyed by T largest first; minCost, minAdjCost and complete heaps keyed smallest first):

```
loop while not stopped:
  P = pop(walk) or pop(downhill) or pop(straight) or pop(bloom)  (first non-empty, in that order)
  if P:
     if hash[(P.point, P.layer, P.orient)] != P: continue
     trigger lazy generation (rule c); extend P; continue
  if bloomLimit > 0 and blooms > bloomLimit: stop
  off = rate × blooms
  minToGo = minToGo0 − off if off <= minToGo0 else 0
  maxToGo = maxToGo0 − off if off <= maxToGo0 else 0
  if maxToGo == 0 and complete heap non-empty: stop
  while (x = removeTop(maxToGo heap)) and x.T >= minToGo: add(minCost heap, key x.E)
  if x: add(maxToGo heap, key x.T)                         # put back the first non-qualifying one
  while (x = removeTop(minCost heap)) and x.T > maxToGo:
       add(minAdj heap, key x.E + ((x.T × m) >> e))       # penalty = m / 2^e
  if x: add(minCost heap, key x.E)
  a = top(minAdj); c = top(minCost); g = top(maxToGo)
  if c and a: if c.key <= a.E + (((a.T − maxToGo) × m) >> e): bloom(removeTop(minCost)) else bloom(removeTop(minAdj))
  elif c: bloom(removeTop(minCost)); elif a: bloom(removeTop(minAdj)); elif g: bloom(removeTop(maxToGo))
  else stop
bloom(P): push P on bloom stack; bloomMax = P.E + bloomDeltaCost; blooms += 1
```

Dispatch of a new non-complete path N created while extending a path popped from source S (step 5 of addPoint):
- complete → complete heap (key E); walk extend set → walk stack (any source);
- S = INIT → maxToGo heap (key T);
- S = bloom: orient 'O' → bloom stack; orient 'H' with hCost(L) <= vCost(L) or 'V' with vCost(L) <= hCost(L) → straight stack; else (including 'X') → maxToGo heap;
- S = straight: if N.orient == parent.orient and N.E < bloomMax → straight stack, else downhill stack;
- S = downhill: if N.E < parent.E + 1 → downhill stack, else maxToGo heap;

The penalty is `m = floor(p × 2)`, `e = 1` (see Q-LEGACY-32). Result: the top of the complete heap (cheapest E) with code success (or "best before interrupt"), else failure / interrupted. See Q-LEGACY-95 for blooms of stale paths.

Compatibility: exact
(Magic ref: mzrouter/mzSearch.c:60-730, mzrouter/mzSearch.c:1350-1490, mzrouter/mzTech.c:985-1010, mzrouter/mzInternal.h:60-70)

#### Scenario: Window sweep termination
- **GIVEN** initial estimate 1000, window width 10000, rate 500
- **WHEN** 22 blooms have occurred and a complete path exists
- **THEN** `maxToGo = max(0, 11000 - 11000) = 0` and the search stops returning the cheapest complete path

#### Scenario: Adjusted-cost comparison
- **GIVEN** penalty 1024.0 (m = 2048, e = 1), maxToGo = 900, minCost top E = 5000, minAdj top E = 3000 with T = 901
- **WHEN** the next bloom source is chosen
- **THEN** the window-corrected cost is `3000 + ((1 × 2048) >> 1) = 4024 < 5000`, so the minAdj top blooms

#### Scenario: Straight stack continuation
- **GIVEN** a bloom from a path with E = 1000 and bloomDeltaCost 1
- **WHEN** a straight-stack path produces a same-orientation child with E = 1000
- **THEN** the child goes on the straight stack (1000 < 1001), while a child with E = 1001 goes to the downhill stack

### Requirement: Heap ordering
All search heaps (and the estimate Dijkstra heap) SHALL behave exactly like the following array heap with 64-bit integer keys, so that equal keys pop in the same order as in Magic. `A[1..n]`; `better(a,b)` is the strict comparison `a < b` for min-heaps and `a > b` for max-heaps.

```
sift(i):  loop: x = 2i; if x > n: return
            if 2i+1 <= n and better(A[2i+1], A[x]) : x = 2i+1      # tie → left child
            if not better(A[x], A[i]): return                      # tie → stop
            swap(A[i], A[x]); i = x
build():  if not built: for i = n downto 1: sift(i); built = true    # only when n > 0
top():    if n == 0: none; build(); return A[1]
removeTop(): if n == 0: none; build(); r = A[1]; A[1] = A[n]; n -= 1; sift(1); return r
add(k):   n += 1; A[n] = k
          if built: i = n
            loop: c = i
                  if i is odd and i != 1 and not better(A[i], A[i-1]): c = i-1   # tie → left sibling
                  i = i div 2; if i == 0: return
                  if not better(A[c], A[i]): return
                  swap(A[c], A[i]); sift(c)
```

Entries added before the first `top`/`removeTop` are not ordered until then. See Q-LEGACY-6.

Compatibility: exact
(Magic ref: utils/heap.c:60-420)

#### Scenario: Equal keys before first removal
- **GIVEN** an empty min-heap and adds a(5), b(3), c(5) with no removal in between
- **WHEN** three removals follow
- **THEN** they return b, c, a

#### Scenario: Equal keys after build
- **GIVEN** a built min-heap holding only x(4)
- **WHEN** y(4) is added and two removals follow
- **THEN** y is not moved above x (tie), and the removals return x then y

### Requirement: Path to paint conversion
The found path SHALL be converted to paint in a scratch result cell and then copied: consecutive segments of the same orientation are merged; each straight segment from point A to point B on layer R paints the rectangle `[min(Ax,Bx), max(Ax,Bx) + width(R)] × [min(Ay,By), max(Ay,By) + width(R)]`; each layer change paints the contact type at the change point P as `[Px, Px + width] × [Py, Py + length]` for up/down contacts and `[Px, Px + length] × [Py, Py + width]` for left/right contacts on all its planes. Before painting, close contact pairs that would violate rules are fixed: two contacts closer than their width in both axes cause one to be painted as its two residue layers only (width × width); routes between contacts closer than the contact spacing are painted at contact width and, if both contacts are the same type, with both residue layers.

Compatibility: exact
(Magic ref: mzrouter/mzMain.c:880-1250, mzrouter/mzSubrs.c:589-651)

#### Scenario: Horizontal segment paint
- **GIVEN** a path segment on metal1 (width 3) from (0,0) to (20,0)
- **WHEN** it is painted
- **THEN** metal1 covers (0,0)-(23,3)

### Requirement: Maze router diagnostics
The wizard command `*mzroute` with subcommands `debug`, `dumpEstimates`, `dumpTags`, `help`, `numberLine`, `parms`, `plane`, `version` SHALL be treated as developer diagnostics.

Compatibility: informative
(Magic ref: mzrouter/mzTestCmd.c:530-622, mzrouter/mzDebug.c)

#### Scenario: Diagnostics have no layout effect
- **WHEN** `*mzroute parms` is executed
- **THEN** maze parameters are printed and the layout is unchanged
