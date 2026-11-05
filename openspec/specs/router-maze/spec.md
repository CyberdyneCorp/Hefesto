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
- For a route contact: `effWidth = max(width, width(layer1), width(layer2))` and bloats = component-wise max of its own and both layers' bloats.
- `contextRadius` = max `bloatBot` over all active route types.
- Max walk length AUTOMATIC → `2 * contextRadius`.
- Bounds increment AUTOMATIC → `30 * min over active layers (width + spacing(layer,itself))`, or 100 if no active layer.
- Bounding rectangle: the bounds hint expanded by `2*contextRadius`, or the whole plane shrunk by `3 * (maxSpacing + maxWidth + 2)`.

Compatibility: exact
(Magic ref: mzrouter/mzSubrs.c:60-270)

#### Scenario: Bounds increment
- **GIVEN** active layers with (width, self-spacing) of (3,3) and (4,4)
- **WHEN** a route starts with bounds increment AUTOMATIC
- **THEN** the bounds increment is 180

### Requirement: Blockage model
For each route type the engine SHALL maintain blockage planes in which a point is legal iff a wire of that type with its lower-left corner at that point creates no spacing violation. Every tile of type u visible to the router (searching expanded subcells according to the expansion mask) with `spacing(t,u) >= 0` blocks the area of the tile expanded by `bloatBot(u)` to the left and bottom and `bloatTop(u)` to the right and top. Unexpanded subcells block with the SUBCELL spacing. Material electrically connected to the start (when expanding endpoints) is marked "same node" (not blocking); destination areas are marked "destination" with surrounding "walk" regions where the route may penetrate blocked area up to the max walk length to reach the destination. Contact blockage = union of both layers' blockages plus its own. Fence paint (built-in layer `fence`) blocks the side opposite to the first start point (if the first start point lies inside fence paint, the route is confined inside; all start points must have the same fence parity, otherwise that start point is ignored with a message; see Q-LEGACY-30). Blockage is generated lazily in tiles of size `boundsIncrement` around path endpoints. Each blockage plane exists in a maximal-horizontal and a maximal-vertical strip form.

Compatibility: exact
(Magic ref: mzrouter/mzBlock.c:18-1487, mzrouter/mzMain.c:571-640)

#### Scenario: Bloat of a metal obstacle
- **GIVEN** route layer metal1 width 3 with spacing(metal1, metal1) = 3, and a metal1 obstacle (10,10)-(20,20)
- **WHEN** blockage is generated
- **THEN** the metal1 blockage covers (5,5)-(23,23)

### Requirement: Hint, fence and rotate layers
The built-in layers `magnet` (alias `mag`), `fence` (`f`) and `rotate` (`r`), available in every technology, SHALL be read from the route cell (top-level cell only when "top hints only" is set, otherwise the whole expanded hierarchy) and SHALL affect routing as follows: within `rotate` paint, horizontal segments are charged the layer's vCost (and vertical segments its hCost); a segment parallel to and not inside magnet paint is charged an extra `hintCost × length × distance` to the nearest magnet edge in its perpendicular direction (distance 0 when no magnet bounds the space in that direction); fences act as described in "Blockage model".

Compatibility: exact
(Magic ref: mzrouter/mzHint.c:40-197, mzrouter/mzXtndRght.c:470-520, database/DBtechtype.c:85-95)

#### Scenario: Rotate swaps costs
- **GIVEN** route layer m2 with hCost 32 and vCost 64 and a horizontal segment of length 10 inside rotate paint
- **WHEN** the segment cost is computed
- **THEN** the cost is 640

### Requirement: Segment and contact cost model
The cost of extending a path by a straight segment of length L on route layer R SHALL be `L × c` with `c = vCost(R)` for horizontal (`hCost(R)` for vertical) segments under rotate paint, `overCost(R)` if the segment crosses blocked area of a contact type connecting to R (over-routing), else `hCost(R)` (`vCost(R)` for vertical), plus the hint cost; a change of direction on the same layer adds `jogCost(R)`; a contact adds its `cost`. A point reached in "same node" area on a path whose cost so far is 0 keeps cost 0 (that portion is not painted). Paths are keyed by (point, layer, orientation) and a new path is discarded if a path with cost <= the new estimated total exists for that key.

Each candidate path stores `cost = previousCost - previousEstimate + segmentCost + newEstimate` (estimated total) and `togo = newEstimate` (0 for complete paths). All costs use 64-bit integers.

Compatibility: exact
(Magic ref: mzrouter/mzSearch.c:1211-1460, mzrouter/mzXtndRght.c:430-530, mzrouter/mzSearch.c:816-1010)

#### Scenario: Jog cost
- **GIVEN** a path on m1 (hCost 64, vCost 32, jogCost 256) arriving horizontally at P
- **WHEN** it is extended vertically by 5 units
- **THEN** the incremental cost is `5 × 32 + 256 = 416` (plus estimate change)

### Requirement: Interesting points
From a path end on layer R the engine SHALL extend in each permitted direction only to the nearest "interesting" point, the minimum of: the end of the generated-bounds tile; the point just before a blockage of R (or the first unit into destination/walk area); points where free space on R or on any other active layer's blockage plane widens or narrows; the next x (or y) coordinate aligned with a destination area edge; the next hint-plane edge; rotate-plane edges (just before and last unit inside); first/last positions where each contact connecting to R is placeable. Extension codes of the new point allow: straight on always; contacts if reached for alignment, contact, goal, hint or rotate reasons; perpendicular turns if reached for jog, goal, hint or rotate reasons. Contacts at a point are placeable only if the contact blockage there is free or same-node, the free run is at least `length - width`, it is not within `spacing + width` (in both axes) of the previous contact of the same type, and not at a distance from the last bend in the open interval `(width, width + spacing)` or `(-spacing, 0)`.

Compatibility: exact
(Magic ref: mzrouter/mzXtndRght.c:40-400, mzrouter/mzXtndLeft.c, mzrouter/mzXtndUp.c, mzrouter/mzXtndDown.c, mzrouter/mzSearch.c:816-1010)

#### Scenario: Stop before obstacle
- **GIVEN** a path end at x = 0 on a blockage plane whose next blocked tile starts at x = 12
- **WHEN** it is extended to the right with no other interesting points
- **THEN** the new point is at x = 11 and perpendicular turns are allowed there

### Requirement: Cost estimation
When estimation is on, the engine SHALL build an estimate plane before searching: solid tiles for fences and for unexpanded subcells that no active layer may cross, space tiles split along extensions of solid-tile and destination edges; each space tile gets the minimum active hCost and vCost; Dijkstra over tile corners (edge weights = min cost of adjacent tiles in that direction) gives the cost from each corner to a destination; each tile gets estimators `(x0, y0, hcost, vcost, cost0)` for each corner plus straight-shot estimators, dominated estimators removed; the estimate at (x,y) is the minimum over the tile's estimators of `|x - x0|*hcost + |y - y0|*vcost + cost0`. With estimation off, the same plane is built without any solid tiles (subcells and fences are ignored), so the estimate reduces to the cheapest cost-weighted Manhattan distance to a destination. Unexpanded subcells are added as solid tiles only if no active layer has a SUBCELL spacing of -1 (i.e. no layer can cross them) and the expansion mask is non-zero.

Compatibility: exact
(Magic ref: mzrouter/mzEstimate.c:20-2038)

#### Scenario: Open-field estimate
- **GIVEN** no obstacles, one active layer with hCost 1 and vCost 2, destination point (100, 50)
- **WHEN** the estimate at (0,0) is computed
- **THEN** it equals `100*1 + 50*2 = 200`

### Requirement: Windowed bloom search
The search SHALL proceed as follows:
1. Initial paths from each start point (on each permitted start layer, including initial contacts) are placed on the max-togo heap (largest togo first). If a start point is already connected to a destination the result is "already routed".
2. `initialEstimate` = minimum initial estimated total cost; window `minToGo0 = initialEstimate`, `maxToGo0 = initialEstimate + windowWidth`.
3. Loop: pop from, in priority order, the walk stack, the downhill stack, the straight stack, the bloom stack (all LIFO). A popped path that is no longer the hashed best for its key is skipped. Otherwise ensure blockage is generated around its end and extend it.
4. When all stacks are empty: stop if `bloomLimit > 0` and blooms > limit. Set `offset = rate × blooms`; `minToGo = max(0, minToGo0 - offset)`, `maxToGo = max(0, maxToGo0 - offset)`. If `maxToGo == 0` and a complete path exists, stop. Move paths with `togo >= minToGo` from the max-togo heap into the min-cost heap (keyed by cost). Move paths with `togo > maxToGo` from the min-cost heap into the min-adjusted-cost heap with key `cost + ((togo × mantissa) >> exponent)` (penalty = mantissa/2^exponent). Compare the min-cost top with the adjusted-cost top re-normalized as `cost + (((togo - maxToGo) × mantissa) >> exponent)`; bloom from the min-cost top if its cost <= that value, else from the adjusted top; if one heap is empty use the other; if both are empty use the max-togo top; if all empty stop.
5. Blooming a path pushes it on the bloom stack and sets `bloomMax = cost + bloomDeltaCost`. New paths generated while expanding: from the bloom stack, contact moves ('O') return to the bloom stack, moves in the layer's cheaper (or equal) direction go to the straight stack, others to the max-togo heap; from the straight stack, continuing in the same orientation with cost < bloomMax stays on the straight stack, else goes to the downhill stack; from the downhill stack, paths with cost < parent cost + 1 stay downhill, else go to the max-togo heap; points in walk regions go to the walk stack; complete paths go to the complete heap.
6. Result: the cheapest complete path; result code success, "best before interrupt" if interrupted after finding one, failure if none, interrupted if interrupted before any, "unroutable" if no unblocked destination exists.

Heap tie-breaking SHALL follow Magic's heap algorithm (see Q-LEGACY-6).

Compatibility: exact
(Magic ref: mzrouter/mzSearch.c:20-730, mzrouter/mzSearch.c:1211-1490, mzrouter/mzMain.c:724-878, mzrouter/mzStart.c)

#### Scenario: Window sweep termination
- **GIVEN** initial estimate 1000, window width 10000, rate 500
- **WHEN** 22 blooms have occurred and a complete path exists
- **THEN** `maxToGo = max(0, 11000 - 11000) = 0` and the search stops returning the cheapest complete path

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
