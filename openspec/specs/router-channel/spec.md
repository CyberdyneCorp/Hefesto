# Router: Netlist Channel Router

## Purpose

Defines the netlist-driven channel router (`route`, `channels`): the `router` technology section, the routing grid, channel decomposition of empty space, obstacle and hazard maps, terminal stem assignment, the global router (crossing assignment with its cost model), the greedy channel router, metal maximization, paint-back of wires/contacts/stems, via minimization, and error feedback. Hefesto must reproduce Magic's routed geometry for the same layout, netlist and technology wherever the algorithm is deterministic. The gate-array front end (`garoute`) is in capability `router-gate-array`; the maze router used for maze stems is in `router-maze`; netlist files are in `netlist-window`. This capability is desktop-only in the first iPadOS release (decision P-13; capability `hefesto-ipad-feature-scope`).

Terminology used throughout:

- `G` = router grid spacing (technology `gridspacing`), `O = (Ox, Oy)` = routing grid origin (set by `route origin`, default `(0,0)`).
- `gridUp(v, o)` = smallest `o + k*G >= v`; `gridDown(v, o)` = largest `o + k*G <= v` (k integer; exact floor/ceiling for negative values as well).
- `half = floor(G/2)` (integer division of a positive number).
- A *channel* is a rectangle whose boundary lies on half-grid lines (`gridline - half`); it holds `length` columns (x grid lines) and `width` tracks/rows (y grid lines) strictly inside it, numbered from 1. Index 0 and `length+1` / `width+1` denote the boundary positions.
- A *crossing* (pin) is the point where a grid line crosses a channel boundary. Each channel has four pin arrays: top/bottom (indexed 1..length) and left/right (indexed 1..width).
- Layer 1 ("metal") is the preferred horizontal (track) layer; layer 2 ("poly") is the preferred vertical (column) layer.

## Requirements

### Requirement: Router technology section grammar
The technology file `router` section SHALL be parsed with the following grammar, each statement on one line, keywords compared case-sensitively by exact string match:

```ebnf
router_section = "router" , NL , { router_stmt , NL } , "end" ;
router_stmt    = layer_stmt | contacts_stmt | grid_stmt ;
layer_stmt     = ( "layer1" | "layer2" ) , type_name , width , { obstacle_types , separation } ;
contacts_stmt  = "contacts" , type_name , width , [ metal_surround , poly_surround ] ;
grid_stmt      = "gridspacing" , integer ;
obstacle_types = type_list ;      (* comma-separated type names / aliases, as in other tech sections *)
width          = integer ;        (* > 0 *)
separation     = integer ;        (* >= 0 *)
metal_surround = integer ;        (* >= 0, must be an integer literal *)
poly_surround  = integer ;        (* >= 0, must be an integer literal *)
```

Semantics:
- `layer1` sets the layer-1 type and width and REPLACES the layer-1 obstacle set with the listed types; for each listed type the stored layer-1 separation is the maximum of all separations given for that type. `layer2` does the same for layer 2.
- `contacts` sets the contact type and width and resets the contact offset to 0; when five fields are given it also sets the metal and poly surrounds. With three fields the surrounds keep their previous values.
- `gridspacing` sets `G`.
- Defaults before the section is read: layer types = space, widths = 2 (layer1, layer2, contact), contact offset 0, surrounds 0, `G = 4`, subcell separations 4/4, empty obstacle sets, all separations 0.
- Errors are reported and processing continues: wrong field count (`Wrong number of arguments in router %s statement.`), non-positive width (value ignored), negative separation (that pair ignored), non-integral or negative surround (negative surround becomes 0), non-positive gridspacing (ignored), unknown keyword (`Unknown router statement "%s".`). A `layer1`/`layer2` line with an odd trailing field still applies all complete pairs before reporting the field-count error. An unknown type name leaves the previous type unchanged.

Compatibility: exact
(Magic ref: router/rtrTech.c:70-330, scmos/scmos.tech.in router section)

#### Scenario: scmos router section
- **GIVEN** the section `layer1 metal1 3 allMetal1 3` / `layer2 metal2 3 allMetal2 4 allPoly,allDiff 1` / `contacts m2contact 4` / `gridspacing 8`
- **WHEN** the technology is loaded
- **THEN** layer 1 is metal1 width 3, layer 2 is metal2 width 3, contact is m2contact width 4 with surrounds 0 and 0, and `G = 8`

#### Scenario: Negative separation ignored
- **GIVEN** `layer1 metal1 3 poly -1`
- **WHEN** the line is parsed
- **THEN** the error `Layer1 obstacle separation must be positive; -1 is illegal.` is reported, poly gets no layer-1 separation, but poly is still added to the layer-1 obstacle set

### Requirement: Derived router technology values
After the `router` section is read the router SHALL compute derived values as follows (all integer arithmetic, C-style truncating division of the non-negative numerator):

1. `w = max(layer1Width, layer2Width)`; `contactOffset = -((contactWidth + 1 - w) / 2)` where the inner division truncates toward zero.
2. `above = contactWidth + contactOffset`; `below = -contactOffset`.
3. For each tile type t: `m = sep1[t] + metalSurround` if t is a layer-1 obstacle else 0; `p = sep2[t] + polySurround` if t is a layer-2 obstacle else 0; `s = max(m, p)`; `sepDown[t] = s + above`; `sepUp[t] = s + below`.
4. `subcellSepDown = max over t of sepDown[t]` and `subcellSepUp = max over t of sepUp[t]`, both starting from 0 (not from the pre-section default 4).

When the internal grid is rescaled by `n/d`, every width, offset, surround, `G`, subcell separation and per-type separation SHALL be multiplied by `n` then divided by `d` (truncating).

Compatibility: exact
(Magic ref: router/rtrTech.c:345-445)

#### Scenario: Derived values for scmos
- **GIVEN** the scmos router section above (widths 3/3, contact width 4, surrounds 0)
- **WHEN** derived values are computed
- **THEN** `contactOffset = -1`, `above = 3`, `below = 1`, metal1 has `sepDown = 6, sepUp = 4`, metal2 has `sepDown = 7, sepUp = 5`, poly has `sepDown = 4, sepUp = 2`, and the subcell separations are up 5, down 7 (assuming no larger separation elsewhere)

### Requirement: route command dispatch and parameters
The `route` command SHALL accept an optional first argument matched case-insensitively as a unique prefix of the option keywords (an exact keyword match wins over prefixes). Options and behavior:

| Option | Args | Effect / output |
|---|---|---|
| (none) | – | Route the edit cell inside the box (see "Routing flow"). Requires a box in an edit-cell window. |
| `end` | `[real]` | Set channel end constant (default 1.0); prints `Channel end constant is %f`. |
| `help` | `[*wizard]` | Lists the user options (or the wizard options with `*wizard`). |
| `jog` | `[int]` | Set minimum jog length in grid units (default 1); prints `Minimum jog length is %d`. |
| `metal` | – | Toggle metal maximization (default on); prints `Metal maximization: on|off`. |
| `netlist` | `[file]` | Select current netlist (see capability `netlist-window`); prints `Current list is "%s"`. |
| `obstacle` | `[real]` | Set obstacle constant (default 0.7); prints `Obstacle constant is %f`. |
| `origin` | `[x y]` | With no args prints `Routing grid origin = (%d,%d)`; with two coordinates sets the origin silently. |
| `stats` | – | Prints and clears statistics: `Total length %d;  Metal %d;  Poly %d;  Vias %d`. |
| `settings` | `[*wizard]` | Prints the parameter table (end constant, metal maximization, jog, net list, obstacle, steady, via limit, maze stems). |
| `steady` | `[int]` | Set steady-net constant (default 1); prints `Steady net constant is %d`. |
| `tech` | – | Prints layer types/widths, contact type/width/offset/surrounds, subcell separations, grid spacing. |
| `vias` | `[int]` | Set metal-maximization via limit (default 2); prints `Via limit is %d`. |
| `viamin` | – | Run via minimization over the current netlist and print `\n%d vias removed`. |
| `mazestems` | – | Toggle maze-routed stems (default off); prints `Maze route channel stems: yes|no`. |
| `*checks`, `*debug`, `*file <f>`, `*map`, `*showcolumns`, `*showfinal` | | Wizard/debug toggles; informative. |

An ambiguous option reports `Ambiguous routing option: "%s"`; an unknown option reports `"%s" isn't a valid router option.` followed by `  Type "route help" for help.`. A wrong argument count reports `Wrong number of arguments to %s option.`. An unparsable numeric value reports e.g. `Bad value for via limit` and leaves the value unchanged; the current value is still printed. Parameters persist for the session (they are not stored in any file).

Compatibility: behavioral
(Magic ref: router/rtrCmd.c:470-790, utils/lookup.c:80-150)

#### Scenario: Ambiguous prefix
- **GIVEN** any layout
- **WHEN** `route s` is executed
- **THEN** the option is ambiguous (matches `stats`, `settings`, `steady`) and `Ambiguous routing option: "s"` is reported

#### Scenario: Set and show via limit
- **WHEN** `route vias 5` is executed
- **THEN** the via limit becomes 5 and `Via limit is 5` is printed

#### Scenario: tech report for scmos
- **GIVEN** the scmos router section
- **WHEN** `route tech` is executed
- **THEN** the output includes `   Preferred layer..... metal1; width 3`, `   Contacts............ m2contact; width 4; offset -1; surrounds 0, 0`, `   Subcell separations: 5 up; 7 down` and `   Router grid spacing: 8`

### Requirement: Routing flow
Routing the edit cell with `route` SHALL perform, in order:
1. Netlist selection: if no netlist is selected, select the netlist named after the edit cell and print `No netlist selected yet;  using "%s".`.
2. Netlist build: read nets and resolve every terminal name to label locations in the edit cell hierarchy (see capability `netlist-window`). If there are zero nets report `No nets to route.` and stop.
3. Channel decomposition of the box area (see "Channel decomposition"). If the rounded area is empty report `Routing area (box) is too small to be of any use.` and stop.
4. One channel is created per channel tile; channel tiles are converted in the plane's enumeration order and each new channel is PREPENDED to the channel list, so later processing visits channels in reverse enumeration order.
5. Channel initialization (obstacles, densities, pin linking, stem assignment, blockage propagation, hazards) — see following requirements and capability `router-gate-array` "Channel initialization".
6. Global routing, then channel routing of every channel in list order, then paint-back of every channel in list order, then stem painting.
7. Report `No routing errors.`, `There was one routing error:  see feedback.` or `There were %d routing errors:  see feedback.`, where the count is the number of feedback areas added during routing. If channel routing produced errors, `%d bad connection%s.` is also reported.

Routing output is ordinary paint in the edit cell, followed by a DRC re-check of the routing area. Progress milestone lines (`Building netlist: `, `#`/`!` progress characters, `... time: %.1fu %.1fs`) are informative.

Compatibility: behavioral
(Magic ref: router/rtrMain.c:85-150, garouter/gaMain.c:250-340)

#### Scenario: No nets
- **GIVEN** an edit cell whose selected netlist contains no nets
- **WHEN** `route` is executed
- **THEN** `No nets to route.` is reported and the layout is unchanged

### Requirement: Routing area rounding
The box area SHALL be rounded outward so every edge lies on a canonical half-grid position: for the upper edge `e`, let `t = gridUp(e, o) - half`; the new edge is `t + G` if `t < e`, else `t`. For the lower edge `e`, let `t = gridUp(e, o) - half`; the new edge is `t - G` if `t > e`, else `t`. (`o` is `Ox` for x edges and `Oy` for y edges.) If the rounded area is empty (zero or negative extent in either axis) routing SHALL not proceed.

Compatibility: exact
(Magic ref: router/rtrDcmpose.c:150-175)

#### Scenario: Box rounding with G=8
- **GIVEN** `G = 8`, origin (0,0), box (0,0)-(40,24)
- **WHEN** the routing area is rounded
- **THEN** the routing area is (-4,-4)-(44,28)

### Requirement: Channel decomposition
Channel decomposition SHALL compute, inside the rounded routing area R, a set of rectangular channel tiles covering all space not occupied by subcells, using exactly the following tile-level algorithm (the result depends on the intermediate tile structure, so Hefesto MUST reproduce these tile operations or an equivalent that yields identical rectangles).

Data: a plane of rectangular tiles, each "cell" or "space", forming a corner-stitched tiling (the plane is infinite; tiles outside R are space). Each space tile carries four boundary flags NW, NE, SW, SE: NW/NE mean "the left/right end of my top edge is a channel boundary", SW/SE likewise for the bottom edge. Neighbour lookups use the corner-stitch conventions: `above(t)` = the tile above t touching t's top-right corner, `below(t)` = the tile below touching t's bottom-left corner, `leftOf(t)` = the left neighbour touching t's bottom-left corner, `rightOf(t)` = the right neighbour touching t's top-right corner; `at(p)` = the tile containing point p (half-open: `left <= x < right`, `bottom <= y < top`).

1. **Silhouettes.** Start from an all-space plane. For every subcell use (and every element of an array use separately) intersecting R, take its bounding box in edit-cell coordinates and expand it: lower-left `x' = gridDown(x - subcellSepDown, Ox) - half`, `y' = gridDown(y - subcellSepDown, Oy) - half`; upper-right `x' = gridUp(x + subcellSepUp, Ox) + G - half`, `y' = gridUp(y + subcellSepUp, Oy) + G - half`. Clip to R and paint as "cell" (painting yields the canonical maximal-horizontal-strip tiling of capability `layout-geometry-model`). Paint in the edit cell itself is NOT an obstacle for decomposition. Two copies are kept: a read-only copy used to enumerate cell tiles, and the working plane that is split and merged below.
2. **Split at R.** In the working plane: the tile containing R's upper-right corner is split horizontally at `R.ytop` if it spans that line; the tile containing `(R.xbot, R.ybot - 1)` is split at `R.ybot` if it spans that line; walking up R's left edge from `R.ybot`, every tile containing `(R.xbot, y)` that strictly spans `x = R.xbot` is split vertically there (next y = that tile's top); the same along `x = R.xtop`.
3. **Initial flags.** For every working tile intersecting R: clear all flags; a space tile with `top == R.ytop` gets NW and NE; a space tile with `bottom == R.ytop` gets SW and SE (see Q-LEGACY-1, effectively never true); every cell tile gets all four.
4. **Corners.** Enumerate the cell tiles of the read-only copy intersecting R in tile enumeration order (capability `layout-geometry-model`, requirement "Tile enumeration order"). For each, try its corners in the order SW `(left,bottom)`, NW `(left,top)`, NE `(right,top)`, SE `(right,bottom)`. A corner `(x,y)` is *usable* iff it lies strictly inside R and, with `p0`/`p1` defined as

   | corner | p0 (vertical neighbour) | p1 (side neighbour) | flag that must be clear on t2 |
   |---|---|---|---|
   | NE | (x, y) | (x, y-1) | NW |
   | NW | (x, y) | (x-1, y-1) | NE |
   | SE | (x, y-1) | (x, y) | SW |
   | SW | (x, y-1) | (x-1, y) | SE |

   `t1 = at(p0)` (working plane) is space with `left != x` and `right != x`, `t2 = at(p1)` is space, and the listed flag of t2 is clear. For a usable corner compute (`east` = NE or SE, `up` = NE or NW):
   - `xDist = min(right(t1) - x, right(t2) - x)` if east, else `min(x - left(t1), x - left(t2))`.
   - `yDist` by walking from `cur = t1`:
     ```
     loop:
       if up:  py = top(cur); if py >= R.ytop: break
       else:   py = bottom(cur); if py <= R.ybot: break; py -= 1
       nxt = at(x, py)
       if nxt is cell: if not up: py += 1; break
       if left(nxt) == x or right(nxt) == x: break
       if left(cur) < left(nxt):
           flag = (right(cur) > right(nxt)) ? (up ? nxt.SW : nxt.NW) : (up ? cur.NE : cur.SE)
       else: flag = up ? cur.NW : cur.SW
       if flag: if not up: py = bottom(cur); break
       cur = nxt
     up:   t0 = t1;  yDist = py - y
     down: t0 = cur; yDist = y - py
     ```
     (When the downward walk stops because `nxt` has an edge at x, `py` is one unit below `bottom(cur)`; see Q-LEGACY-78.)
   - If `xDist < yDist`, mark a horizontal boundary (`d1`, `d2` = the far x of t1, t2 in the east/west direction, i.e. `right` if east else `left`):

     | corner | always | if d1 >= d2 | if d1 <= d2 |
     |---|---|---|---|
     | NE | t2.NW | t2.NE | t1.SE |
     | SE | t2.SW | t2.SE | t1.NE |
     | NW | t2.NE | t2.NW | t1.SW |
     | SW | t2.SE | t2.SW | t1.NW |

   - Otherwise split vertically at x from `t0` up to `lastY` (`lastY = y + yDist` if up, else `y`):
     ```
     t = t0
     loop:
       n = splitX(t, x)                   # t keeps the left part, n is the right part, both inherit t's flags
       n.NE = t.NE; if t.SE: n.SE = set else: n.NE = clear      # sic, Q-LEGACY-2
       n.NW = n.SW = clear; t.NE = t.SE = clear
       merge(n, below(n)); merge(t, below(t))
       if top(t) >= lastY: break
       t = at(x, top(t))
     merge(above(n), n); merge(above(t), t)
     merge(u, d):  # u directly above d
       if u or d is cell, or left(u) != left(d) or right(u) != right(d): return
       u.SW = d.SW; u.SE = d.SE; join d into u (u keeps NW/NE)
       s = leftOf(u):  if s is space and left(s) >= R.xbot and same top/bottom as u: join s into u
       s = rightOf(u): if s is space and right(s) <= R.xtop and same top/bottom as u: join s into u
     ```
     A join keeps the flags of the surviving (first-named) tile.
5. Each space tile of the working plane inside R is one channel. The decomposition result is kept in an internal cell named `__CHANNEL__` (not saved).

Compatibility: exact
(Magic ref: router/rtrDcmpose.c:120-330, router/rtrDcmpose.c:430-1030)

#### Scenario: Empty area is one channel
- **GIVEN** `G = 8`, no subcells, box (0,0)-(40,24)
- **WHEN** decomposition runs
- **THEN** exactly one channel (-4,-4)-(44,28) results

#### Scenario: Corner distances for a centred cell (verify against Magic)
- **GIVEN** `G = 8`, origin (0,0), subcell separations 5 up / 7 down, a subcell bounding box (16,16)-(32,32) and routing area (-4,-4)-(76,76)
- **WHEN** decomposition runs
- **THEN** the silhouette is `(gridDown(9) - 4, gridDown(9) - 4) = (4,4)` to `(gridUp(37) + 4, gridUp(37) + 4) = (44,44)`
- **AND** for its SW corner (4,4) the space strips are (-4,-4)-(76,4) below and (-4,4)-(4,44) to the left, so `xDist = min(8, 8) = 8` and `yDist = 4 - (-4) = 8`; `xDist < yDist` is false, so the strip below is split at x = 4 and its left piece merges with the left strip into the tile (-4,-4)-(4,44)

#### Scenario: Horizontal boundary chosen for a wide gap
- **GIVEN** a cell silhouette whose NE corner is 8 units from the right edge of R and 40 units below R's top, with a single space strip above spanning R's width
- **WHEN** the NE corner is processed
- **THEN** `xDist = 8 < yDist = 40`, so the horizontal boundary to the right of the corner is marked instead of splitting

### Requirement: Channel geometry
For a channel rectangle C the router SHALL compute: `hiX = gridDown(C.xtop, Ox)`, `loX = gridUp(C.xbot, Ox)`, `length = (hiX - loX)/G + 1`, `origin.x = loX - G`; similarly `width = (hiY - loY)/G + 1`, `origin.y = loY - G`. Column `c` lies at `x = origin.x + c*G`, track `r` at `y = origin.y + r*G`. If `hi < lo` in either axis, the feedback `Degenerate channel at (%d, %d) (%d, %d)` SHALL be added over C and printed. Crossing points: left pins at `(C.xbot, origin.y + r*G)`, right pins at `(C.xtop, ...)`, bottom pins at `(origin.x + c*G, C.ybot)`, top pins at `(..., C.ytop)`. A pin is *linked* to the pin of the adjacent channel sharing its crossing point, except that pins on the left/right sides of a vertical river channel, or top/bottom sides of a horizontal river channel, are never linked, nor are pins that would enter such a channel from an illegal side.

Compatibility: exact
(Magic ref: router/rtrChannel.c:220-275, router/rtrPin.c:82-212)

#### Scenario: Channel size
- **GIVEN** channel (-4,-4)-(44,28), `G = 8`, origin (0,0)
- **WHEN** its geometry is computed
- **THEN** `length = 6`, `width = 4`, `origin = (-8,-8)`, columns at x = 0,8,...,40 and tracks at y = 0,8,16,24

### Requirement: Channel obstacle map
For each channel the router SHALL build an obstacle map over grid indices `0..length+1` × `0..width+1`. Every tile of a layer-1 or layer-2 obstacle type found anywhere in the edit cell hierarchy within the channel's search area (origin expanded by `up` to the lower-left and `(length+1)*G + down` / `(width+1)*G + down` to the upper-right, where `up = subcellSepUp` unless `subcellSepUp + subcellSepDown < G`, in which case `up = G - subcellSepDown`) marks grid points:

- columns from `max(0, (gridUp(xbot - sepDown[t] + 1, Ox) - origin.x)/G)` through `min((gridUp(xtop + sepUp[t] - 1, Ox) - origin.x)/G, length+1)` inclusive;
- rows from `max(0, (gridUp(ybot - sepDown[t] + 1, Oy) - origin.y)/G)` through `min((gridDown(ytop + sepUp[t] - 1, Oy) - origin.y)/G, width+1)` inclusive (note: GRIDDOWN for the row upper bound but GRIDUP for the column upper bound; see Q-LEGACY-3);
- with "blocked on layer 1" if t is a layer-1 obstacle and "blocked on layer 2" if a layer-2 obstacle.

For the global-router density only, an obstacle blocking both layers blocks both a track and a column; otherwise if the marked column count is smaller than the marked row count it blocks columns, else tracks. Pins on the boundary whose map point is blocked on both layers are reserved (unusable); pins whose map point is blocked on one layer are flagged "obstacle". Initial densities: for each column, the number of rows marked as track-blocking; for each row, the number of columns marked as column-blocking; the channel maximum column and row densities are the maxima.

Compatibility: exact
(Magic ref: router/rtrChannel.c:300-560)

#### Scenario: Metal obstacle marks a span of columns
- **GIVEN** the scmos router tech, channel origin (-8,-8) length 6 width 4, and a metal1 box (10,10)-(13,13)
- **WHEN** the obstacle map is built
- **THEN** columns `(gridUp(10-6+1)+8)/8 = 2` through `(gridUp(13+4-1)+8)/8 = 3` and rows 2 through `(gridDown(16)+8)/8 = 3` are marked blocked on layer 1

### Requirement: Hazards
For each normal channel the router SHALL compute obstacle heights (vertical run length of non-clear map points within a column) and widths (horizontal run length within a row) and mark hazard flags:

- Left/right hazards: scanning columns 1..length, rows 1..width, at a point where (height < width and not already marked "vacate") or blocked on both layers, take the contiguous run of rows up to the first point with height >= width and not doubly blocked (rows `bot..top`); extend left and right through columns whose rows `bot..top` are all non-clear (`left..right`); `extra = floor(0.99999 + obstacle_constant * (top - bot + 1))`. Mark "vacate/hazard-from-left" on columns `max(0, left - extra) .. left-1`, rows `bot-1..top+1`; mark both directions on columns `left..right`, rows `bot-1..bot+1` (see Q-LEGACY-4); mark "hazard-from-right" on columns `right+1 .. min(length, right + extra)`, rows `bot-1..top+1`. When the shadow reaches the left edge (`left - extra <= 0`) unflagged left pins in rows `bot-1..top+1` get flag hazard with distance `left` and size `right-left`; when it reaches the right edge (`right + extra >= length`) right pins get hazard with distance `length - right` and size `right - left`.
- Top/bottom hazards: symmetric with rows/columns swapped, using height > width, `extra = floor(0.99999 + obstacle_constant*(right-left+1))`, columns `left-1..right+1` for all marked rows, and the bottom edge test `bot - extra < 0`.
- Pins over a single-layer obstacle get size = the obstacle height (bottom/top pins) or width (left/right pins); pins adjacent (index ±1) to such a pin get the "near contact" flag.

Compatibility: exact
(Magic ref: router/rtrHazards.c:70-560)

#### Scenario: Hazard shadow length
- **GIVEN** obstacle constant 0.7 and a short-wide obstacle spanning rows 2..3 (2 rows)
- **WHEN** hazards are marked
- **THEN** `extra = floor(0.99999 + 1.4) = 2` columns of shadow are flagged on each side

### Requirement: Terminal stems outside channels
For each terminal location (label rectangle) not inside any channel the router SHALL assign stem tips as follows:

1. A zero-size label is first grown to the maximal rectangle of connected material of its type touching the label (if none, feedback `Terminal is degenerate`).
2. The label type must connect to layer 1 or layer 2 (or any type when maze stems are on, treated as layer 2); otherwise feedback `Can't have terminal on %s layer:  must connect to %s or %s (try setting mazestems option?)`. The terminal's larger dimension must be at least the corresponding layer width (`Terminal must be %d wide to connect to %s`). North/south are allowed if its x extent >= layer width; east/west if its y extent >= layer width.
3. For each allowed direction in the order N, S, E, W: compute the contact line `cx = gridDown(floor((xbot + xtop + G - contactWidth)/2) + contactOffset, Ox)` (floor division) and similarly `cy`; search from `(cx, cy)` outward in that direction through the channel plane until a channel tile inside R is found; the start tip is that channel's boundary point on the grid line. The candidate grid lines span `gridUp(lo - half)` .. `gridDown(hi + half)` of the terminal's perpendicular extent. Try the start point, then alternately points `start - kG` and `start + kG` (see Q-LEGACY-5).
4. A tip is acceptable if the point is on the facing boundary of a normal channel, its pin is unused, and the three stem segment boxes (computed as in "Stem paint geometry", each expanded by the maximum layer separation on the three sides away from the terminal) contain no material in the edit cell hierarchy (with maze stems on: unless one of the two routing planes is free there).
5. Every acceptable direction reserves a pin for the net with a "stem" segment marker; additional directions create additional terminal locations. If no direction works, feedback `Can't find a channel in any direction from terminal`. Feedback per location is left only if the warn mode is on, otherwise only when all locations of a terminal fail (`No crossing reachable from terminal`). Unusable locations are deleted.

Compatibility: exact
(Magic ref: router/rtrStem.c:90-400, router/rtrStem.c:430-820)

#### Scenario: Wrong layer
- **GIVEN** a terminal label on ndiff and maze stems off with layers metal1/metal2
- **WHEN** stems are assigned
- **THEN** the location is unusable and (with warn on) feedback reports it can't have terminal on ndiff layer

### Requirement: Priority heap ordering
Every priority queue of the router (net ordering and the global-router search) SHALL behave exactly like this binary heap so that equal keys are dequeued in the same order as Magic. Keys are 32-bit signed integers here (the same structure with 64-bit, float or double keys is used by other capabilities). The heap is an array `h[1..used]`, either min-ordered ("smallest on top") or max-ordered; below, `better(a,b)` is `a < b` for a min-heap and `a > b` for a max-heap, and `notWorse(a,b)` is `a <= b` (min) / `a >= b` (max).

```
add(key, item):
    used += 1; h[used] = (key, item)
    if not built: return                       # lazy: just append until the first removal/peek
    i = used
    loop:
        c = i
        if i is odd and i != 1 and notWorse(h[i-1].key, h[i].key): c = i - 1     # pick better sibling (left on ties)
        i = floor(i / 2); if i == 0: return
        if notWorse(h[i].key, h[c].key): return  # parent already at least as good
        swap h[c], h[i]; siftDown(c)

siftDown(r):
    loop:
        x = 2r; if x > used: return
        if 2r+1 <= used and better(h[2r+1].key, h[2r].key): x = 2r+1   # right only if strictly better
        if notWorse(h[r].key, h[x].key): return
        swap h[r], h[x]; r = x

top():   if used == 0: none
         if not built: for i = used down to 1: siftDown(i); built = true
         return h[1]
removeTop(): t = top(); h[1] = h[used]; used -= 1; siftDown(1); return t
```

Once built the heap stays built (even when it becomes empty). Entries added before the first removal are therefore ordered by a bottom-up heap construction over the insertion sequence, and entries added afterwards by the sibling-aware sift-up above. See Q-LEGACY-6.

Compatibility: exact
(Magic ref: utils/heap.c:60-420)

#### Scenario: Equal keys before the first removal
- **GIVEN** an empty min-heap and the insertions (5,A), (5,B), (5,C) before any removal
- **WHEN** entries are removed one at a time
- **THEN** the order is A, C, B (the build makes no swap; removing A moves C to the root; with B and nothing else under C, C is returned next)

#### Scenario: Net ordering
- **GIVEN** three nets with bounding-box half-perimeters 40, 10, 40 inserted in the order n1, n2, n3
- **WHEN** they are ordered for global routing
- **THEN** the build swaps n2 to the root; the order is n2, then n3 (moved to the root by the removal and not displaced by the equal n1), then n1

### Requirement: Global routing
The global router SHALL assign, for every net, a sequence of crossings connecting its terminals' stem tips, as follows:

0. Internal list order: the net list, each net's terminal list and each terminal's location list are in reverse order of construction (every new element is prepended during the netlist build, see capability `netlist-window`); "first" and "in order" below refer to this internal order.
1. Net order: for each net with at least two terminals, the bounding box of all its terminal locations is computed; nets with fewer than two locations in total are skipped. The rest are inserted (in internal net-list order) into a min-heap ("Priority heap ordering") keyed by the box's `width + height`, and removed one at a time. 
2. Penalty constants (in grid units, multiplied by `G` once per session on first use, see Q-LEGACY-7): channel `Pc = 1`, jog `Pj = 5`, obstacle1 `Po1 = 5`, obstacle2 `Po2 = 3`, neighbour1 `Pn1 = 2`, neighbour2 `Pn2 = 5`, orphan `Por = 3`.
3. Per net: the first terminal (in netlist order) having locations seeds the start list with each of its locations' stem tips, cost = Manhattan distance from the label rectangle to the tip (min over the two x-edges plus min over the two y-edges). The start list is a stack: every addition is placed at its front. The segment counter of the net starts at 1. Each subsequent terminal with locations is routed: for each location in order, a path search ("Global router path search") is run with the current best cost as limit (initially "infinite"); a path is kept if its cost is strictly lower than the best so far. If a path was found, every crossing pair of the path is added to the start list (cost 0; see below), the path is marked ("Global router path marking and density"), and then all of the terminal's locations are added (with their stem costs) and it becomes the "last connected terminal". If no path exists: feedback `Can't find a path from "%s" to "%s"` (this terminal and the last connected terminal) over this terminal's first location rectangle expanded by 1.
4. Adding a path to the start list walks it from the destination end: for each consecutive pair (a = earlier point toward the source, b = later point) it takes a's crossing and the crossing of b that lies in a's channel; a's crossing is added only if it is unused or is a stem crossing, and b's crossing is always added.

Compatibility: exact
(Magic ref: grouter/grouteMain.c:60-320, grouter/grouteMult.c:112-290, utils/netlist.c:290-341)

#### Scenario: Jog penalty
- **GIVEN** `G = 8` and a path entering a channel at column 2 bottom and leaving at track 3 right
- **WHEN** the segment cost is computed
- **THEN** it includes the jog penalty 40 and the channel penalty 8 in addition to the Manhattan distance

#### Scenario: Stem cost
- **GIVEN** a label rectangle (0,0)-(10,4) and a stem tip at (16,12)
- **WHEN** the start cost is computed
- **THEN** it is `min(16, 6) + min(12, 8) = 14`

### Requirement: Global router channel map
Before routing, the global router SHALL build a map of rectangular tiles of four kinds — normal, horizontal-river, vertical-river and blocked — covering the plane (everything outside channels is blocked):

1. Each channel's area is painted with its channel kind (normal or river); every channel ends up covered by exactly one tile, which refers to that channel.
2. Density blockage, per normal channel, using its current column densities `dc[1..L]` (capacity W) and row densities `dr[1..W]` (capacity L), initialized from the channel's obstacle densities: if `max(dc) >= W`, each maximal run of columns `lo..hi-1` with `dc >= W` produces the area `x ∈ [origin.x - half + lo·G, origin.x - half + hi·G]` over the channel's full height, painted *vertical-river*; if `max(dr) >= L` likewise rows produce full-width areas painted *horizontal-river*. Painting combines kinds by this table (row = existing, column = painted):

   | existing \ painted | normal | h-river | v-river | blocked |
   |---|---|---|---|---|
   | normal | normal | h-river | v-river | blocked |
   | h-river | h-river | h-river | blocked | blocked |
   | v-river | v-river | blocked | v-river | blocked |
   | blocked | blocked | blocked | blocked | blocked |

   Tiles are split at the painted area's edges first, and after painting adjacent tiles of the same kind are merged only within one channel. Then the blockage is propagated: for every painted area that is not vertical-river, each vertical-river tile touching its top or bottom edge gets the overlap (x-range clipped to the area, the tile's full height) painted blocked; for every painted area that is not horizontal-river, each horizontal-river tile touching its left or right edge gets the overlap (y-range clipped, full width) painted blocked; repeated until no new area is produced.
3. Every horizontal-river tile is split horizontally at the y of every left- or right-neighbour tile boundary where either neighbour on that boundary is not blocked; every vertical-river tile is split vertically at the x of every top/bottom neighbour boundary likewise; repeated until no split occurs.
4. A horizontal-river tile whose left-side pins in rows `max(1, (bottom - origin.y)/G) .. min(W, (top - origin.y)/G)` are all unusable AND whose right-side pins in that range are all unusable becomes blocked (a pin is usable when unused and linked); vertical-river tiles likewise with bottom/top pins over the column range. See Q-LEGACY-79.

During path marking (below), density blockage (step 2 only) is re-applied to a channel whenever an increment makes some density reach or exceed its previous maximum.

The tile containing a crossing is found by probing the point of the crossing, moved 1 unit down for a top-side crossing and 1 unit left for a right-side crossing; a crossing in a blocked tile is unusable as a start point or destination.

Compatibility: exact
(Magic ref: grouter/grouteChan.c:60-1117, grouter/grouteDens.c:64-185)

#### Scenario: Saturated columns become a vertical river (verify against Magic)
- **GIVEN** a normal channel (-4,-4)-(28,20) with origin (-8,-8), `G = 8`, L = 3, W = 2 and column densities 2, 2, 1 for columns 1..3
- **WHEN** density blockage is applied
- **THEN** the area x ∈ [-4, 12] (around columns 1..2) over the channel's full height is marked vertical-river

### Requirement: Global router path search
For one destination location (its crossing `D` at point `Dp`, in map tile `DT`) the path search SHALL run two passes over a min-heap of search points (each point = crossing, map tile, raw cost `g`, predecessor) keyed by an integer:

```
pass 1 (shortest): fresh heap; add start points; best1 = search(limit = bestCost, prune = true)
                   reset every crossing cost touched in pass 1 to infinity
                   if best1 == none: return none
pass 2 (penalty):  fresh heap; add start points; best = none
                   while (p = search(limit = bestCost, prune = false)) != none:
                       a = adjust(p)
                       if a.cost < bestCost: bestCost = a.cost; best = a
                   return best
add start points: for each start entry s in start-list order whose crossing lies in a non-blocked tile T:
                   key = g(s) + manhattan(s, Dp); push (s, T, g(s))
search(limit, prune): loop: pop top (none -> return none)
    if point(top) == Dp: return top
    if g(top) >= limit: return none
    if prune and g(top) > cost(crossing(top)): continue
    if tile(top) == DT: propagateFinal(top)
    elif tile(top) is normal: propagateNormal(top)
    else: propagateRiver(top)
```

Crossing costs `cost(x)` start at infinity for every crossing. Successor generation (`m(a,b)` = Manhattan distance between crossing points):

- **propagateFinal(p)**: `g' = g + m(p, D) + Pc`; with prune, skip if `g' >= cost(D)`, else `cost(D) = g'`; push D with key `g'` and tile DT.
- **propagateRiver(p)**: o = the pin directly opposite p's crossing in p's channel (same column for top/bottom, same row for left/right); skip unless o and its linked pin o' are usable; T' = tile of o'; `g' = g + m(p, o') + Pc`; with prune skip if `g' >= cost(o')`, else set `cost(o') = cost(o) = g'`; without prune skip if T' already appears on p's predecessor chain (loop check). Push o' with key `g' + m(o', Dp)`.
- **propagateNormal(p)**: visit the neighbours of tile(p) in this order — along its top edge from right to left, along its left edge from bottom to top, along its bottom edge from left to right, along its right edge from top to bottom — skipping blocked neighbours, vertical-river neighbours on the left/right sides and horizontal-river neighbours on the top/bottom sides. For a neighbour of a different channel: enumerate crossings ("Crossing enumeration") on the shared edge and take the FIRST usable one c (usable: c unused and linked, and its linked pin also unused); `g' = g + m(p, c') + Pc` where c' = c's linked crossing in the neighbour; prune/loop checks as above (with prune, both c' and c get the cost); push c' with key `g' + m(c', Dp)` and tile = neighbour. For a neighbour of the same channel (a split piece of p's channel), recurse into that neighbour's own neighbours: for a horizontal-river piece only further along the direction of travel (east or west), for a vertical-river piece only north or south, for a normal piece all sides except the one leading back.

Crossing enumeration on the edge shared by tiles A (current) and B: the shared range `[lo, hi)` in x (top/bottom edge) or y (left/right edge) is converted to pin indices `lo' = (lo + G - 1 - origin)/G`, `hi' = (hi - origin - 1)/G` (integer division, channel origin coordinate of A's channel), and pins are offered starting at `s` = the current crossing's column (top/bottom) or row (left/right): if `s <= lo'` in increasing order, if `s >= hi'` in decreasing order, otherwise in the pairs `(s-k, s+k+1)` for k = 0, 1, 2, … — i.e. `s, s+1, s-1, s+2, s-2, …` — skipping members outside `[lo', hi']`.

The result is the destination point; its predecessor chain is the path back to a start point.

Compatibility: exact
(Magic ref: grouter/grouteMain.c:200-320, grouter/grouteMaze.c:60-610, grouter/grouteCrss.c:330-470, grouter/groutePath.c:60-140, grouter/grouteChan.c:1060-1117)

#### Scenario: Alternating crossing order
- **GIVEN** a shared edge with pin indices 2..7 and the current crossing at index 4
- **WHEN** crossings are enumerated
- **THEN** they are offered in the order 4, 5, 3, 6, 2, 7

#### Scenario: Search stops at the cost limit
- **GIVEN** a best cost of 100 from a previous location and a heap top with raw cost 100 that is not the destination
- **WHEN** the search continues
- **THEN** it ends without a new path

### Requirement: Global router crossing adjustment and cost
Each path found in pass 2 SHALL be re-costed and locally improved by `adjust`, which processes the path from its start end toward the destination:

```
adjust(look, p):                      # initially look = none, p = destination point
    if p has no predecessor: return p  # start point: fixed, its raw cost kept
    r = adjust(p, pred(p))
    q = new point(crossing(p), tile(p), pred = r); q.cost = r.cost + crossCost(look, p, r)
    if look == none: return q          # destination: fixed
    if tile(r) is not normal:          # previous segment was a river: exit pin is fixed
        q.crossing = linked pin of the pin opposite crossing(r) in r's channel
        q.cost = r.cost + crossCost(look, q, r)
    else:
        enumerate crossings on the edge between tile(r) and tile(p) in the order of
        "Crossing enumeration" (starting from r's crossing); for each usable crossing x:
            if m(x, r) + r.cost >= q.cost: stop
            c = r.cost + crossCost(look, q with crossing x, r)
            if c < q.cost: q.crossing = x; q.cost = c        # ties keep the earlier crossing
    return q
```

`crossCost(look, exit, entry)`: let `en` = entry's crossing, `ex` = exit's crossing taken in en's channel (its linked pin if needed), `ot` = ex's linked crossing (in the next channel).

```
cost = m(en, ex)
if look != none and tile(exit) is not normal:
    if the pin opposite ot in ot's channel is not usable: return INFINITY
cost += Pc
if en.col != ex.col and en.row != ex.row: cost += Pj      # pin indices; left/right pins have col 0 / L+1, bottom/top pins row 0 / W+1
if ot exists and ot's channel is normal: cost += obstaclePenalty(ot)
if en's channel is not normal: return cost
cost += obstaclePenalty(ex)
k = number of non-empty (used or blocked) pins among ex's two neighbours on the same side (index ±1)
cost += (k == 2 ? Pn2 : k == 1 ? Pn1 : 0)
if ex's side is not opposite en's side and the pin facing ex on the opposite side
   (same column/row) is unused: cost += Por
obstaclePenalty(x): if x is flagged obstacle/hazard/near-contact or size(x) != 0:
    Po1 + (obstacle ? Po2·size(x) : hazard ? max(Po2·size(x) - dist(x), 0) : 0)  else 0
```

All arithmetic is 32-bit integer; INFINITY is the router's "infinite" cost.

Compatibility: exact
(Magic ref: grouter/grouteCrss.c:470-795)

#### Scenario: Obstacle penalty with hazard (G = 8)
- **GIVEN** `G = 8`, an exit crossing flagged hazard with size 2 and distance 3 and no used neighbours, path straight across
- **WHEN** its cost is computed
- **THEN** the penalties are `Pc = 8`, `Po1 = 40` and `max(24·2 - 3, 0) = 45`, total 93 plus the distance

### Requirement: Global router path marking and density
Marking a found path SHALL walk it from the destination toward the start; for each consecutive pair (source crossing s = predecessor's crossing, destination crossing d = the point's crossing taken in s's channel):

```
seg += 1                                          # net's segment counter
id = (net, s.segment) if s is used and is not a stem crossing else (net, seg)
densityAdjust(s.channel, s, d, id)  -> if it reports a change, re-apply density blockage to that channel
if s was not used (or was a stem): take(s, id)
take(d, id)
take(x, id): if x unused, or x is a stem crossing of the same net: assign id
             elif x already has exactly id: feedback "Warning: crossing reassigned to same net/seg" on the 1×1 square at x
             else: feedback "Crossing multiply used, nets %s/%d, %s/%d"
```

`densityAdjust`: determine where id is already present in the channel: rows `[minR, maxR]` and columns `[minC, maxC]` start empty; a left pin of id at row r sets `minC = 1` and extends the row range by r; a right pin sets `maxC = L` and extends by r; a bottom pin at column c sets `minR = 1` and extends the column range by c; a top pin sets `maxR = W` and extends by c. Then for rows `clamp(min(s.row,d.row),1,W) .. clamp(max(...),1,W)` not inside `[minR, maxR]` increment the row density; for columns likewise; a change is reported when an incremented value becomes `>=` the running maximum of that vector (the maximum is updated).

Compatibility: exact
(Magic ref: grouter/grouteCrss.c:40-200, grouter/grouteDens.c:64-185)

#### Scenario: Segment ids per channel
- **GIVEN** a path through three channels for a net whose segment counter is 1 and whose start crossing is unused
- **WHEN** it is marked
- **THEN** the three channel segments, taken from the destination end, get segment ids 2, 3 and 4

### Requirement: Greedy channel router
Each channel SHALL be routed by a column-sweep greedy router whose steps, state and tie-breaking are defined by the requirements "Greedy router: orientation and retry", "Greedy router: nets, end distance and density", "Greedy router: column flags", "Greedy router: column state and primitives", "Greedy router: column sequence", "Greedy router: feasible top and bottom connections", "Greedy router: collapsing split nets", "Greedy router: reducing the range of split nets", "Greedy router: vacating tracks", "Greedy router: rising and falling runs", "Greedy router: uncollapse near the end", "Greedy router: extension and end of channel", "Greedy router: deterministic sort" and "River channels". Errors are feedback squares of size 4×4 centered on the grid point of (column, track) in the original (unflipped) channel frame, text `Net `%s', channel %p:  <message>` (the pointer text is informative). Error counting: every message below except `Density (%d) > channel size (%d)` increments the channel's error count; the density message adds feedback only.

Compatibility: exact
(Magic ref: gcr/gcrRoute.c:40-320, router/rtrFdback.c:60-236)

#### Scenario: Straight-through net
- **GIVEN** a channel of length 6 and width 4 with left pin 2 and right pin 2 on the same net and no obstacles
- **WHEN** it is channel routed
- **THEN** track 2 carries the net from column 0 to column 6 with no jogs and no errors

### Requirement: Greedy router: orientation and retry
Before routing, used-but-unrouted stem crossings that were reserved during global routing SHALL be released (set to unused). Then, with `W` = channel width (tracks) and `L` = channel length (columns):

```
if W < L:                         # wide channel, route as is
    A = copy(ch); e1 = greedy(A)
    if e1 == 0: result = A
    else:
        B = mirrorLR(ch); e2 = greedy(B)
        result = (e2 < e1) ? mirrorLR(B) : A
else:                             # tall or square channel, transpose
    A = transpose(ch); e1 = greedy(A)
    if e1 == 0: result = transpose(A)
    else:
        B = mirrorLR(transpose(ch)); e2 = greedy(B)
        result = transpose( (e2 < e1) ? mirrorLR(B) : A )
errors(ch) = min-chosen error count; feedback = the list of the kept attempt only
```

`transpose` swaps x and y: bottom pins become left pins, top pins become right pins (and vice versa), grid point (c, t) becomes (t, c), "blocked on layer 1" and "blocked on layer 2" are exchanged, the right-connection and up-connection result bits are exchanged, hazard-from-right ↔ hazard-from-top and vacate/hazard-from-left ↔ hazard-from-bottom are exchanged, and a horizontal river channel becomes a vertical one and vice versa. `mirrorLR` maps column c to `L+1-c`, exchanges left and right pins, top/bottom pin c to `L+1-c`, exchanges the hazard-from-left and hazard-from-right flags, and moves each right-connection bit from column c-1 to the mirrored column of c (so a wire c-1→c stays a wire between the same two grid points). Ties (`e2 == e1`) keep the first attempt.

Compatibility: exact
(Magic ref: router/rtrChannel.c:70-200, gcr/gcrChannel.c:227-445)

#### Scenario: Square channel is transposed
- **GIVEN** a channel with `W = 4`, `L = 4`
- **WHEN** it is routed
- **THEN** it is routed transposed (columns sweep along the original y axis), because `W < L` is false

#### Scenario: Mirror retry only on errors
- **GIVEN** a channel with `W = 3`, `L = 10` whose first attempt has 2 errors and whose mirrored attempt has 2 errors
- **WHEN** it is routed
- **THEN** the first attempt and its 2 feedback entries are kept

### Requirement: Greedy router: nets, end distance and density
The router SHALL build its nets from the channel's pins keyed by (net, segment). Pins marked "blocked" are first reset to unused. Pins are visited in the order: left pins tracks 1..W; then for each column c = 1..L the bottom pin of c followed by the top pin of c; then right pins tracks 1..W. The first pin of a key creates a net (the channel's net list is built by prepending); each later pin is appended to that net's pin list, so each net's pin list is in visiting order. A net's *next pin* is the first pin remaining in its list; left pins are removed when the first column is initialized and top/bottom pins are removed when their column is processed; right pins are never removed.

End distance: for each net count `k` = the number of trailing pins of its list that lie on the right edge (stop at the first pin, scanning from the end, whose column ≤ L). `right = Σk`, `multi` = number of nets with `k > 1`. Then `E = trunc((floor(multi/2) + floor(right/4)) × endConstant)` (integer divisions first, then the product truncated toward zero), and `E = 1` if the result is < 1. A column c is *near the end* iff `L + 1 - c <= E`.

Density: `d = ` number of left pins that are the first pin of their net; `last` = number of left pins that are the last pin of their net. For c = 1..L: `d -= last; last = 0`; if the top pin of c is used: `d += 1` if it is its net's first pin, else `last += 1` if it is its net's last pin; if the bottom pin is used: `d += 1` if it is its net's first pin, else if it is its net's last pin then (`d -= 1` if the top pin of c belongs to the same net, else `last += 1`). The channel density is the maximum of all `d` values (including the initial one). If it exceeds W the feedback `Density (%d) > channel size (%d)` is placed at (L, W). A channel with no nets is left unrouted with no errors.

Compatibility: exact
(Magic ref: gcr/gcrInit.c:60-260, gcr/gcrRoute.c:60-90)

#### Scenario: End distance
- **GIVEN** end constant 1.0, 4 nets each with exactly one trailing right pin, and 2 nets each with 2 trailing right pins
- **WHEN** the end distance is computed
- **THEN** `right = 8`, `multi = 2`, and `E = 1 + 2 = 3`

#### Scenario: End distance rounding with constant 0.7
- **GIVEN** end constant 0.7, `right = 9`, `multi = 3`
- **WHEN** the end distance is computed
- **THEN** `(1 + 2) × 0.7 = 2.1` truncates to `E = 2`

#### Scenario: Density counts a two-pin net once
- **GIVEN** a channel with L = 3, W = 2, net a on left pin 1 and on top pin of column 2, nothing else
- **WHEN** density is computed
- **THEN** the densities are 1, 1, 1, 0 for columns 0..3 (the top pin is the last pin, so it is subtracted at column 3) and the channel density is 1

### Requirement: Greedy router: column flags
Before the sweep, the router SHALL derive per-grid-point routing flags from the obstacle map (B1 = blocked on layer 1 "track layer", B2 = blocked on layer 2 "column layer"; `clear` = neither; `only1` = B1 and not B2; `only2` = B2 and not B1; `both` = B1 and B2). For c = 1..L and t = 1..W, with `here = (c,t)`, `right = (c+1,t)` and `up = (c,t+1)` (index W+1 and L+1 are the boundary and initially carry no obstacle flags unless the map marks them):

| here | right is | set | up is | set |
|---|---|---|---|---|
| clear | only1 | TC on here | only2 | CC on here |
| clear | both (and not only1) | TE on here | both | CE on here |
| only2 | only1 or both | TE on here | clear | CC on **up** |
| only2 | | | only1 or both | CE on here |
| only1 | clear | TC on **right** | only2 or both | CE on here and on up |
| only1 | only2 or both | TE on here | | |
| both | – | TE and CE on here | – | CE on up |

The two columns of the table are evaluated independently (first the "right" test, then the "up" test) except that for `both` both flags are set unconditionally. Flags are cumulative (bit-or). Meaning: TC = a contact is needed when the track continues to the right (track meets a layer-1-only obstacle); TE = the track cannot be extended to the right; CC = a contact is needed for vertical wiring through this point; CE = vertical wiring cannot continue upward from this point. See Q-LEGACY-8.

Compatibility: exact
(Magic ref: gcr/gcrFlags.c:40-144, gcr/gcr.h:255-310)

#### Scenario: Track contact before a metal-only obstacle
- **GIVEN** grid point (3,2) clear and (4,2) blocked on layer 1 only
- **WHEN** flags are computed
- **THEN** (3,2) gets TC; and since (4,2) is layer-1-only and (5,2) is clear, (5,2) also gets TC

### Requirement: Greedy router: column state and primitives
The router SHALL keep, for the column being processed, per track t = 0..W+1: `H[t]` (net occupying the track), `V[t]` (net of vertical wiring already placed in this column at t), `hi[t]`/`lo[t]` (next higher/lower track of the same net — a *split net* — or NONE), `hOk[t]`/`lOk[t]` (the link to hi/lo is already joined by vertical wiring in this column), `want[t]` (net that needs t for a right-edge connection, or none) and `F[t]` (flags of the current column: B1, B2, TC, CC, TE, CE, VL = vacate/hazard-from-left, plus X = contact, which can be added during the column). A net with `hi = lo = NONE` on its only track is *unsplit*; a net is *done* when it is unsplit and has no next pin.

Primitives (pseudo-code; `n` is a net):

```
blocked(t, n, last):            # may net n use point t vertically?
    if V[t] != none and V[t] != n:                       return true
    if CE in F[t] and t != last and H[t] != n:           return true
    if F[t] ∩ {B1,B2,CC} and H[t] != none and H[t] != n: return true
    if X in F[t]:                                        return true
    return false

vertClear(a, b):                # a = source track, n = H[a]
    for t in min(a,b)..max(a,b):
        if V[t] not in {none, n}:                              return false
        if CE in F[t] and t != max(a,b):                       return false
        if F[t] ∩ {B1,B2,X,CC} and H[t] not in {none, n}:      return false
    return true

unlinkTrack(t):  join hi[t] and lo[t] to each other; hi[t]=lo[t]=NONE; H[t]=none
linkTrack(n, t): H[t]=n; hi[t]= nearest u>t (u<=W) with H[u]==n (and lo[u]=t), else NONE;
                 lo[t]= nearest u<t (u>=1) with H[u]==n (and hi[u]=t), else NONE

moveTrack(n, from, to):          # paint a jog in this column
    if from == to: return
    if n == none and want[from] != none: n = want[from]
    last = from; s = sign(to - from)
    for i strictly between from and to (stepping by s):
        V[i] = n
        if H[i] == n:
            if want[i] != n: unlinkTrack(i)
            else: mark i joined to last on the side facing from
                  (s>0: lOk[i]=true, hOk[last]=true; s<0: hOk[i]=true, lOk[last]=true); last = i
        if CC in F[i]: F[i] += {X, B1, B2}
    if want[to] == n or (not hOk[to] and not lOk[to]): H[to] = n
    else: H[to]=none; hOk[to]=lOk[to]=false; hi[to]=lo[to]=NONE
    if want[from] != n:              # "to" replaces "from" in the split chain
        s>0: lo[to]=lo[from] (fix back-link); if to < hi[from]: hi[to]=hi[from] (fix back-link)
        s<0: hi[to]=hi[from] (fix back-link); if lo[from] < to: lo[to]=lo[from] (fix back-link)
        hi[from]=lo[from]=NONE
    else:                            # both tracks stay, now joined
        s>0: hi[from]=to, lo[to]=from, hOk[from]=lOk[to]=true
        s<0: lo[from]=to, hi[to]=from, lOk[from]=hOk[to]=true
    V[from] = V[to] = n
    if want[from] != n: H[from]=none; hOk[from]=lOk[from]=false; hi[from]=lo[from]=NONE
```

`NONE` behaves as the integer -1 in the comparisons `to < hi[from]` and `lo[from] < to` (so for a downward jog with `lo[from] = NONE`, `lo[to]` becomes NONE). "Fix back-link" means updating the neighbour's opposite link to point at the new track when that neighbour is not NONE. Tracks 0 and W+1 stand for the bottom and top pins of the column.

Compatibility: exact
(Magic ref: gcr/gcrLib.c:38-456)

#### Scenario: Jog frees the source track
- **GIVEN** W = 4, net a only on track 1 (unsplit), no wanted tracks, no obstacles
- **WHEN** `moveTrack(a, 1, 3)` is applied
- **THEN** `V[1] = V[2] = V[3] = a`, `H[3] = a`, `H[1] = none`, and track 1 has no links

### Requirement: Greedy router: column sequence
The router SHALL process the channel as follows (all loops in increasing index order unless stated):

```
initialize collapse memory (best pattern = none, split cache empty)   # once per channel
compute column flags
initColumn(left pins); extend(0)
for c in 1..L:
    feasible(c)
    if nearEnd(c) and (E < L or not nearEnd(c-1)): markWanted()
    collapse(); pickBest()
    reduceRange()
    vacate(c)
    runs(c, classify(), riseFall = true)
    if nearEnd(c): uncollapse(); pickBest()
    extend(c)
endOfChannel()
```

`initColumn`: `H[0] = H[W+1] = none`, `H[t]` = net of left pin t (then the left pin is removed from its net's list); all `V`, `want`, `hOk`, `lOk`, `F` cleared, `hi`/`lo` = NONE; then for t = 0..W+1 in increasing order each occupied track is linked to the previous track of the same net (`lo[t]` = previous, `hi[previous] = t`). Then for t = 1..W `wanted(t, 0)` is applied.

`wanted(t, c)`: let `n = H[t]`; do nothing unless n is unsplit, has a next pin `p`, and p is on the right edge. If p is the net's only remaining pin, `want[row(p)] = n`; else if c is near the end, `want[row(q)] = n` for p and every following pin q.

`markWanted()`: for t = 1..W, if right pin t is used, `want[t]` = its net.

Note: when `E >= L+1` every column, including column 0, is near the end, so `markWanted` is never called (see Q-LEGACY-77).

Compatibility: exact
(Magic ref: gcr/gcrRoute.c:150-200, gcr/gcrInit.c:330-398, gcr/gcrEdge.c:30-110)

#### Scenario: Wanted marking at the first near-end column only
- **GIVEN** L = 5 and E = 5
- **WHEN** the columns are swept
- **THEN** `markWanted` is applied at column 1 only (column 1 is near the end, column 0 is not, and `E < L` is false)

### Requirement: Greedy router: feasible top and bottom connections
For column c with top-pin net `T` and bottom-pin net `B` (either may be none), the router SHALL remove both pins from their nets' lists, then:

```
topTarget: for t = W down to 1: if blocked(t, T, 0): fail; if H[t] in {none, T}: target = t, stop
botTarget: for t = 1 up to W:   if blocked(t, B, W): fail; if H[t] in {none, B}: target = t, stop
(no top pin -> top not failed; no bottom pin -> bottom not failed; lo[0]=hi[0]=NONE first)

if topFailed and botFailed and T == B != none and T has no next pin:
    if any t in 1..W is blocked(t, B, W): return (no error reported)
    moveTrack(T, W+1, 0); both succeed            # straight vertical wire through the column
elif both found and T,B != none and botTarget >= topTarget:      # conflict
    if T == B: makeFeasible(T, W+1, topTarget); makeFeasible(B, 0, botTarget); wanted(topTarget, c)
    elif botTarget > W - topTarget - 1:  makeFeasible(T, W+1, topTarget); botFailed; wanted(topTarget, c)
    else:                                makeFeasible(B, 0, botTarget);  topFailed; wanted(botTarget, c)
else:
    if T found: makeFeasible(T, W+1, topTarget); wanted(topTarget, c)
    if B found: makeFeasible(B, 0, botTarget);  wanted(botTarget, c)
topFailed -> error "Can't make top connection" at (c, W)
botFailed -> error "Can't make bottom connection" at (c, 1)
```

`makeFeasible(n, from, target)`: `hi[from] = lo[from] = NONE`; if `H[target] == n`, copy target's links to `from`; else if `from != 0` (top), link `from` to the nearest track `i < target` holding n (`lo[from] = i`, `hi[i] = target`); else (bottom) link to the nearest `i > target` holding n (`hi[0] = i`, `lo[i] = target`); then `moveTrack(n, from, target)`; then `hi[from] = lo[from] = NONE`.

The conflict rule gives the top pin priority unless the bottom jog length `botTarget` is at most `(W + 1 - topTarget) - 2` (see Q-LEGACY-82).

Compatibility: exact
(Magic ref: gcr/gcrFeas.c:40-248)

#### Scenario: Conflicting top and bottom pins
- **GIVEN** W = 6, no obstacles, top pin net a, bottom pin net b, track 3 empty and tracks 1, 2, 4, 5, 6 occupied by other nets
- **WHEN** feasible connections are made
- **THEN** `topTarget = botTarget = 3` is a conflict; `3 > 6 - 3 - 1 = 2` holds, so the top pin connects to track 3 and `Can't make bottom connection` is reported at (c, 1)

#### Scenario: Bottom pin wins when much closer
- **GIVEN** the same column but with only track 2 empty
- **WHEN** feasible connections are made
- **THEN** `2 > 6 - 2 - 1 = 3` is false, so the bottom pin connects to track 2 and `Can't make top connection` is reported at (c, 6)

### Requirement: Greedy router: collapsing split nets
At each column the router SHALL enumerate collapse patterns depth-first and keep the best:

```
collapse(colState, bot=1, top=W, freed=0):
    for i in bot..top:
        to = hi[i]
        if to == NONE or hOk[i]: continue
        if vertClear(i, to):
            n = H[i]; new = copy(colState)
            if want[to] != n and want[i] == n: new.moveTrack(n, to, i)
            else:                              new.moveTrack(n, i, to)
            if new.H[to] != none:
                inc = 2 if (new.hi[to]==NONE and new.lo[to]==NONE and H[to] has no next pin) else 1
                collapse(new, to, top, freed + inc)
            if top > to: top = to - 1         # shrinks the remaining loop range
    evaluate(colState, freed)
```

Note that `freed` is only increased when the surviving track `to` is still occupied; when `moveTrack` empties `to`, that branch is not recursed into and the jog is not scored. Patterns are evaluated in post-order (deeper patterns first, then the pattern of the caller); the unmodified column is evaluated last.

`evaluate(P, freed)` compares P with the current best B (kept across calls; reset only when a best is taken by `pickBest`):

```
if B is none: B = P; Bfreed = freed; return
if freed > Bfreed: B = P; return          # P better
if freed < Bfreed: return                 # B better
n = 0
for i in 0 .. floor(W/2) - 1:
    if n > cacheTop: cache[++cacheTop] = nextSplit(B, i)
    if cache[n] > W: return                       # B better (no more split nets)
    d = nextSplit(P, i)
    if d < cache[n]: return                        # B better
    if d == cache[n]: n += 1
    else: cache[n] = d; cacheTop = n; B = P; return   # P better
wB = #{t in 1..W-1 : B.V[t] != none}; wP = #{t in 1..W-1 : P.V[t] != none}
if wB > wP: return (B better) else B = P            # ties favour P
nextSplit(S, i): for j = i+1 .. floor(W/2) - 1:
    if (S.hi[j] != NONE and S.lo[j] == NONE) or (S.lo[W-j+1] != NONE and S.hi[W-j+1] == NONE): return j
    return W + 1
```

`cache`/`cacheTop` are initialized to empty (`cacheTop = -1`) once per channel and are NOT reset between columns (see Q-LEGACY-74). `pickBest` installs B as the column state and sets B = none. Criteria in words: most freed tracks; then uncollapsed split nets farthest from the channel edges; then most vertical wiring; then the later-evaluated pattern.

Compatibility: exact
(Magic ref: gcr/gcrColl.c:30-250)

#### Scenario: Two collapsible nets
- **GIVEN** W = 6, net a on tracks 1 and 3, net b on tracks 4 and 6, no pins left for either, no obstacles or wanted tracks
- **WHEN** collapse runs
- **THEN** the pattern that collapses both (a onto 3, then b onto 6) frees 4 tracks and is kept

### Requirement: Greedy router: reducing the range of split nets
After collapsing, the router SHALL move the outermost tracks of still-split nets inward:

```
for i in 1..W-1:
    # lowest track of a split net
    if hi[i] != NONE and lo[i] == NONE and H[i] != want[i] and not hOk[i]:
        clear = F[i] has neither B1 nor B2; far = i; n = H[i]
        for j in i+1..W:
            if H[j] == n and hOk[j]: break
            if blocked(j, n, W): break
            if clear and F[j] ∩ {B1,B2}: break
            if H[j] == none and CC not in F[j]: far = j
        if far - i >= minJog: moveTrack(n, i, far)
    # highest track of a split net
    k = W + 1 - i
    if hi[k] == NONE and lo[k] != NONE and H[k] != want[k] and not lOk[k]:
        clear = F[i] has neither B1 nor B2          # sic: tests track i, see Q-LEGACY-75
        far = k; n = H[k]
        for j in k-1 down to 1:
            if H[j] == n and lOk[j]: break
            if blocked(j, n, 0): break
            if clear and F[j] ∩ {B1,B2}: break
            if H[j] == none and CC not in F[j]:
                far = j
                if lo[j] == NONE: break
        if k - far >= minJog: moveTrack(n, k, far)
```

`minJog` is the `route jog` parameter (default 1). The upward scan keeps the farthest free track; the downward scan stops at the first free track whose `lo` is NONE.

Compatibility: exact
(Magic ref: gcr/gcrColl.c:320-397)

#### Scenario: Minimum jog suppresses a short inward jog
- **GIVEN** minJog = 3, net a split on tracks 2 and 6 (W = 7), tracks 3 and 4 free and clear, track 5 holding net b, track 7 holding net c
- **WHEN** the range is reduced
- **THEN** the upward scan from 2 passes tracks 5..7 (occupied but not blocked for vertical wiring, and track 6 is not joined), far = 4, and since `4 - 2 = 2 < 3` no jog is made

### Requirement: Greedy router: vacating tracks
The router SHALL then vacate tracks that are wanted by another net or lie in a hazard shadow:

```
nearEndV = (L - c <= E)                     # note: differs from nearEnd(c), see Q-LEGACY-76
list = []
for i in 1..W:
    n = H[i]; skip if none or done
    if (want[i] not in {none, n}) or (VL in F[i] and not nearEndV):
        skip if split (hi[i] or lo[i] != NONE)
        to = look(i, canCover = (TE in F[i]) or i == 1 or i == W)
        if to != NONE and right pin `to` is unused:
            append n with track = i, dist = to - i, key = |to - i|
sort list ascending by key (see "deterministic sort")
runs(c, list, riseFall = false)
```

`look(t, canCover)` (n = H[t]): returns NONE if `V[t]` is another net. `up` limit = hi[t] or W; `dn` limit = lo[t] or 1. `dir = class(n, t)`, `target = t + dir`. Scan `u = t+1, d = t-1` simultaneously (u++ and d-- each round) while `u <= upLim or d >= dnLim`:
- Upward candidate (if `u <= upLim`, not yet blocked upward, none found upward): stop upward if both layers blocked at u, or `V[u]` is another net, or `H[u]` is another net and u has B1 or B2. Otherwise u qualifies if `want[u]` ∈ {none, n}, VL ∉ F[u], and (u has neither B1 nor B2, or canCover). If it qualifies: return u when `dir >= 0`; else remember `bestUp = u`, `upLen = (u - t) + (u - target)` and raise `dnLim` to `t - upLen + 1` if larger.
- Downward candidate symmetric, but additionally requires `H[d] == none`; returns d when `dir <= 0`, else remembers `bestDn = d`, `dnLen = (t - d) + (target - d)`, and lowers `upLim` to `t + dnLen - 1` if smaller.
After the loop return `bestDn` if `dir > 0`, else `bestUp`. (The upward candidate does not require the track to be empty; see Q-LEGACY-83.)

`class(n, t)`: with p = n's next pin (none → 0), `dist = row(p) - t` (0 → 0); for each following pin q in order: if `col(q) > col(p) + steady` return dist; if `(row(q) - t > 0) != (dist > 0)` return 0; after all pins return dist. `steady` is the `route steady` parameter (default 1).

Compatibility: exact
(Magic ref: gcr/gcrUnsplit.c:59-270, gcr/gcrUnsplit.c:400-420)

#### Scenario: Vacate toward the target side
- **GIVEN** W = 5, unsplit net a on track 3 with VL set, next pin on the top at the following column (class `6 - 3 = +3`), tracks 2 and 4 free and clear
- **WHEN** vacate runs
- **THEN** `look` returns track 4 immediately (dir ≥ 0) and a is queued with key 1

### Requirement: Greedy router: rising and falling runs
`classify()` SHALL select, for t = 1..W, each track holding an unsplit net n (`lo[t] = hi[t] = NONE`) that has a next pin, with `dist = class'(n, t) != 0`, where `class'` is `class` except that the steady-window break yields the current dist (same as `class`). Each selected net gets `dist`, `track = t` and key `realDist(t, dist)`; the list is sorted descending by key. `realDist(t, dist)`: `last = 0`; for j from t toward `t + dist` (exclusive): stop if `V[j] != n`; if `H[j]` ∈ {n, none} then `last = j - t`; return `|dist - last|`.

`runs(c, list, riseFall)`:

```
for n in list (in order):
    from = n.track; to = from + n.dist; distToTarget = |from - to|
    to = clamp(to, 1, W)
    r = tryRun(n, from, to, c); if r == NONE: continue
    if not riseFall and r != to: continue
    if not riseFall:
        save = want[from]; want[from] = none; moveTrack(n, from, r); want[from] = save
    elif |from - r| >= minJog or (nearEnd(c) and right pin r belongs to n):
        if distToTarget > |r - to|: moveTrack(n, from, r)
```

`tryRun(n, from, to, c)` scans i from `from` toward `to` inclusive and returns the last acceptable i ≠ from (the farthest), NONE if none:

```
covered = F[from] ∩ {B1,B2} != ∅; srcEnds = TE in F[from]; run = NONE
for i = from .. to:
    if CE in F[i]: break
    if V[i] not in {none, n}: break
    if both B1,B2 in F[i]: break
    if F[i] ∩ {B1,B2,CC} and H[i] not in {none, n}: break
    if TE in F[i]: continue
    if CC in F[i] and (not (nearEnd(c) and want[i]==n) or (srcEnds and run==NONE)): continue   # Q-LEGACY-8
    if VL in F[i] and VL not in F[from] and not (srcEnds and run==NONE)
                  and not (want[i]==n and nearEnd(c)): continue
    if H[i] not in {none, n}: continue
    if want[i] not in {none, n} and (run != NONE or want[from] in {n, none}): continue
    if not covered and F[i] ∩ {B1,B2} and not (want[i]==n and nearEnd(c)): continue
    if i != from: run = i
return run
```

Compatibility: exact
(Magic ref: gcr/gcrUnsplit.c:280-610)

#### Scenario: Rising net jogs toward its pin
- **GIVEN** W = 6, minJog 1, steady 1, unsplit net a on track 2 whose next pin is the top pin of column c+1, tracks 3..6 free and clear
- **WHEN** runs are made at column c
- **THEN** `dist = 7 - 2 = 5`, `to` is clamped to 6, `tryRun` returns 6 and a jogs from track 2 to track 6

### Requirement: Greedy router: uncollapse near the end
At near-end columns the router SHALL enumerate split patterns that bring nets onto tracks wanted for right-edge pins:

```
uncollapse(S, bot=1, top=W, split=0):
    for i in bot..top:
        n = S.H[i]
        if n == none and S.want[i] != none and S.V[i] == none:
            w = S.want[i]
            for to in i+1..W:
                if blocked(to, none, last = (S.H[to] != w)): to = NONE; stop
                if S.H[to] == w: type = 1; extra = 2; stop
                if S.want[to] == w and S.H[to] == none: type = 2; extra = 1; stop
                if CE in S.F[to]: to = NONE; stop
        elif n != none:
            for to in i+1..W:
                if blocked(to, n, last = (S.want[to] == n)): to = NONE; stop
                if S.want[to] == n and S.H[to] == none: type = 3; extra = 2; stop
                if CE in S.F[to]: to = NONE; stop
        else: continue
        if a track `to` was found:
            new = copy(S)
            type 1: new.moveTrack(none, to, i)        # net taken from want[to], see Q-LEGACY-81
            type 2: w2 = new.want[to]; new.linkTrack(w2, to); new.moveTrack(w2, to, i)  # net arg is none → want[to]
            type 3: new.moveTrack(n, i, to)
            uncollapse(new, to, top, split + extra)
            if top > to: top = to - 1
    evaluate(S, split)                                 # same evaluation as collapse
```

The boolean `last` argument of `blocked` is 1 or 0 and is compared with the track index as in `blocked` (so it only exempts tracks 0 or 1 from the column-end test).

Compatibility: exact
(Magic ref: gcr/gcrEdge.c:110-275)

#### Scenario: Split onto a second wanted track (verify against Magic)
- **GIVEN** a near-end column, W = 4, net a on track 1 only, right pins 1 and 3 both on net a (so tracks 1 and 3 are wanted by a), track 2 and 3 free and clear
- **WHEN** uncollapse runs
- **THEN** a type-3 jog from track 1 to track 3 is evaluated with split 2 and, being better than the unmodified column, a ends the column occupying tracks 1 and 3

### Requirement: Greedy router: extension and end of channel
`extend(c)` SHALL write column c's result and prepare column c+1, for t = 0..W in increasing order (`res = result(c, t)`, `prev = result(c-1, t)` when c > 0, `next = result(c+1, t)`):

```
if V[t+1] == V[t] != none:
    res += U; if t == W: result(c, W+1) += U
    if CC in F[t]: res += X; if CC in F[t+1]: result(c, t+1) += X
hasPrev = (c > 0 and R in prev)
if H[t] == none:
    if c == 0: res -= R
    if hasPrev: res += X
    V[t] = none
else:
    hasNext = hi[t] != NONE or lo[t] != NONE or H[t] has a next pin
    if V[t] == H[t] and (hasPrev or hasNext): res += X
    V[t] = none
    if not hasNext: H[t] = none                                  # net terminated
    elif TE in F[t]: error "Can't extend track through obstacle" at (c,t); H[t] = none
    elif c == L and t != 0 and right pin t unused:
        error "Can't extend track to bad connection" at (c,t); H[t] = none
    else: res += R; if c == L: next += R
    if TC in next: V[t] = H[t]          # pre-place own vertical wiring: forces a contact next column
F[t] = flags of next (or cleared if there is no next column)
after the loop: V[W+1] = none; F[W+1] = cleared
```

`endOfChannel()`: for t = 1..W, if `H[t]` differs from the net of right pin t, report `Can't make end connection` at (L, t) using the track's net (or the pin's net when the track is empty).

Compatibility: exact
(Magic ref: gcr/gcrRoute.c:90-130, gcr/gcrRoute.c:200-320)

#### Scenario: Terminated net
- **GIVEN** a track holding net a whose last pin was the bottom pin of column c and which is unsplit
- **WHEN** `extend(c)` runs
- **THEN** the track gets no right connection and becomes empty for column c+1

#### Scenario: Track blocked by both-layer obstacle
- **GIVEN** track 2 holds net a that still has pins, and grid point (c+1, 2) is blocked on both layers so `TE` is set at (c, 2)
- **WHEN** `extend(c)` runs
- **THEN** `Can't extend track through obstacle` is reported at (c, 2) and the track is emptied

### Requirement: Greedy router: deterministic sort
Every list sort in the greedy router SHALL use this exact (non-stable) shell sort so that ties are ordered identically:

```
sort(v[0..n-1], ascending):
    gap = floor(n/2)
    while gap > 0:
        for i in gap..n-1:
            for j = i-gap; j >= 0; j -= gap:
                swap v[j], v[j+gap] if (ascending ? key(v[j]) > key(v[j+gap]) : key(v[j]) < key(v[j+gap]))
        gap = floor(gap/2)
```

(The inner loop does not stop early when no swap occurs.)

Compatibility: exact
(Magic ref: gcr/gcrLib.c:330-370)

#### Scenario: Equal keys keep input order for two elements
- **GIVEN** list [a(key 2), b(key 2)] sorted ascending
- **WHEN** the sort runs
- **THEN** the result is [a, b]

### Requirement: River channels
A horizontal river channel SHALL be routed without the greedy algorithm when no top or bottom pin is used and, for every track whose left and right pins are both used, both pins carry the same net and segment: every track whose left pin is used gets right connections in all columns 0..L. A vertical river channel symmetrically requires no used left/right pins and matching top/bottom pins and gets up connections on rows 0..W for each column 1..L whose top pin is used. ("Used" excludes blocked pins.) Otherwise the router prints one of `Failing because top or bottom pins are used`, `Failing because left and right pins don't match`, `Failing because left or right pins are used`, `Failing because top and bottom pins don't match` and routes the channel with the greedy algorithm. The river test is applied to the channel after orientation, so a horizontal river channel that was transposed is tested as a vertical one.

Compatibility: exact
(Magic ref: gcr/gcrRiver.c:40-179)

#### Scenario: Left pin without right partner
- **GIVEN** a horizontal river channel with left pin 2 used, right pin 2 unused, no top/bottom pins
- **WHEN** it is routed
- **THEN** track 2 is wired straight across all columns and no error is reported

### Requirement: Metal maximization
When metal maximization is on, before painting each channel the router SHALL scan each column 1..length bottom to top, find maximal vertical runs (layer-2 wiring) that are not crossed by a layer-1 track lacking a contact and not on layer-1-blocked points, and convert a run `bottom..top-1` to layer 1 if `bottom < top` and (`needLow + needHigh - hasHigh - hasLow <= 1` or `top - bottom >= viaLimit`), adding contacts where needed at the ends. A run may continue into the adjacent channel only if the adjacent pin is unlinked (adjacent to a cell) or its point has a contact or is neither layer-1 blocked nor carrying a horizontal wire.

Compatibility: exact
(Magic ref: router/rtrPaint.c:420-600)

#### Scenario: Short run not converted
- **GIVEN** via limit 2 and a vertical run of length 1 that would need two new contacts
- **WHEN** metal maximization runs
- **THEN** the run stays on layer 2

### Requirement: Channel paint-back geometry
Channel results SHALL be painted into the edit cell as:

- Tracks (rows 0..width): scanning columns 0..length, the segment from column c to c+1 is layer 2 if either end is layer-1 blocked, else layer 1. A run starting at column c begins at `x = origin.x + c*G` (or `C.xbot` when c = 0) and `y = origin.y + r*G`, height = that layer's width; it ends at `origin.x + c'*G + width(layer)` when the type changes at column c', or at `C.xtop` if it reaches the end.
- Columns (0..length): analogous vertically; a segment is layer 1 if the point was converted by metal maximization or either end is layer-2 blocked, else layer 2; runs start at `C.ybot` when row = 0 and end at `C.ytop` at the top.
- Contacts: at a grid point flagged as contact, a contact `[origin + idx*G + contactOffset, +contactWidth]` in both axes is painted only if the wiring meeting there includes both layers, plus a metal surround square (expanded by metalSurround) of layer 1 and poly surround of layer 2 when nonzero. At column 0/1 a contact is also placed where a track enters a layer-1-blocked first column.

Compatibility: exact
(Magic ref: router/rtrPaint.c:55-420, router/rtrPaint.c:600-680)

#### Scenario: Track paint extent
- **GIVEN** channel (-4,-4)-(44,28), origin (-8,-8), layer1 width 3, track 2 routed on layer 1 from column 0 to the right edge
- **WHEN** paint-back runs
- **THEN** layer 1 is painted over (-4,8)-(44,11)

### Requirement: Stem paint geometry
After channel paint-back, each terminal location whose stem pin was used SHALL get a stem painted as three segments with width `w` (the start layer's width):
- For a stem going north: contact point `(stem.x, gridDown(stem.y, Oy))`; jog point = contact point with x clamped to `[label.xbot, label.xtop - w]`; start point `(jog.x, label.ytop)`. South: contact y = `gridUp(stem.y)`, start y = `label.ybot - w`. East/west symmetric (start x = `label.xtop` or `label.xbot - w`).
- Segment boxes are the bounding boxes of `w×w` squares anchored (lower-left) at consecutive points: start→jog on the start layer, jog→contact on the start layer (if the start and final layers differ, a contact `[contact + contactOffset, + contactWidth]` is painted and the segment box includes it), contact→stem tip on the final layer with the final layer's width.
- Final layer selection: inspect the paint in a 2×2 box around the tip; if both routing layers are present choose layer 2 for north/south stems and layer 1 for east/west; exclude layers blocked at the pin; a contact counts as both. If neither is present no stem is painted. The start layer is the final layer if the terminal connects to it, otherwise the other routing layer. Errors leave feedback over the label expanded by 1 (`Terminal is not on a legal routing layer`, `Couldn't find crossing point for stem`).

Compatibility: exact
(Magic ref: router/rtrStem.c:830-1130, garouter/gaStem.c:951-1180)

#### Scenario: North stem jog clamp
- **GIVEN** a metal1 label (2,0)-(10,4), stem tip (16,12), `G = 8`, width 3
- **WHEN** the stem geometry is computed
- **THEN** contact = (16,8), jog = (7,8), start = (7,4)

### Requirement: Via minimization
`route viamin` SHALL, for every net of the current netlist (following electrically connected paint from each terminal), first replace layer-2 segments that are not overlapped by layer 1 and are terminated at both ends by contacts with layer 1 (adjusting width by the width difference) and erase the then-unneeded contacts, then repeat replacing layer-1 segments with layer 2; it SHALL report the number of contacts removed.

Compatibility: behavioral
(Magic ref: router/rtrVia.c:485-604, router/rtrTravers.c)

#### Scenario: Report count
- **GIVEN** a routed cell with one removable via
- **WHEN** `route viamin` is executed
- **THEN** `1 vias removed` is printed after a blank line

### Requirement: channels command
`channels [netlist | -]` SHALL run only the channel decomposition over the box (with a netlist name: select it and build it, `-` meaning the edit cell's name; `No nets in netlist.` if empty) and show each resulting channel as a feedback outline with text `Channel area`. If the area is too small it SHALL report `Routing area (box) is too small to hold useful channels.`. More than one argument reports a usage error.

Compatibility: behavioral
(Magic ref: router/rtrCmd.c:200-260, router/rtrDcmpose.c:70-110)

#### Scenario: Show channels
- **GIVEN** an empty cell and box (0,0)-(40,24) with `G = 8`
- **WHEN** `channels` is executed
- **THEN** one feedback outline (-4,-4)-(44,28) labelled `Channel area` is added

### Requirement: Router debugging commands
The wizard commands `*groute`, `*seeflags [flag]`, and `route *file <file>` (route a channel description file and paint it) SHALL be treated as developer diagnostics with no compatibility obligation beyond not corrupting the layout.

Compatibility: informative
(Magic ref: router/rtrCmd.c:60-160, router/rtrCmd.c:790-813, gcr/gcrDebug.c:70-240, grouter/grouteTest.c)

#### Scenario: seeflags without argument
- **WHEN** `*seeflags` is executed with no flag name
- **THEN** flag highlights are turned off and `*seeflags:  flag highlights turned off.` is printed
