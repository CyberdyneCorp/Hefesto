# Router: Netlist Channel Router

## Purpose

Defines the netlist-driven channel router (`route`, `channels`): the `router` technology section, the routing grid, channel decomposition of empty space, obstacle and hazard maps, terminal stem assignment, the global router (crossing assignment with its cost model), the greedy channel router, metal maximization, paint-back of wires/contacts/stems, via minimization, and error feedback. Hefesto must reproduce Magic's routed geometry for the same layout, netlist and technology wherever the algorithm is deterministic. The gate-array front end (`garoute`) is in capability `router-gate-array`; the maze router used for maze stems is in `router-maze`; netlist files are in `netlist-window`.

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
Channel decomposition SHALL compute, inside the rounded routing area R, a set of rectangular channel tiles covering all space not occupied by subcells, using this algorithm:

1. Obstacle silhouettes: for every subcell use (and every element of an array use separately) intersecting R, take its bounding box in edit-cell coordinates and expand it: lower-left `x' = gridDown(x - subcellSepDown, Ox) - half`, `y' = gridDown(y - subcellSepDown, Oy) - half`; upper-right `x' = gridUp(x + subcellSepUp, Ox) + G - half`, `y' = gridUp(y + subcellSepUp, Oy) + G - half`. Clip to R and mark as "cell" area. Paint in the edit cell itself is NOT an obstacle for decomposition.
2. Represent the remaining space as maximal horizontal strips (the canonical corner-stitched decomposition: space tiles are as wide as possible, then merged vertically only when left and right edges coincide), with strips split at the four edges of R.
3. Every space tile edge lying on R's top boundary is marked "boundary"; every edge of a cell tile is marked "boundary".
4. Enumerate the cell tiles of the plane (top to bottom, left to right within a strip row). For each of a cell tile's four corners (in order SW, NW, NE, SE) that lies strictly inside R, is convex (the diagonal neighbor position is space and the vertical neighbor across the corner is a space tile whose left/right edge does not coincide with the corner x), and from which no boundary edge already extends horizontally, compute `xDist` = distance from the corner to the nearer of the horizontal ends of the two adjoining space tiles, and `yDist` = distance from the corner, moving vertically away from the cell, until a cell tile, R's edge, an existing vertical split at that x, or a marked horizontal boundary is reached. If `xDist < yDist`, mark the horizontal edge from the corner as a channel boundary; otherwise split the space tiles vertically at the corner's x over the `yDist` span and merge vertically-adjacent space tiles with identical left and right edges (and then horizontally-adjacent tiles with identical top and bottom inside R).
5. Each remaining space tile is one channel. The decomposition result is kept in an internal cell named `__CHANNEL__` (not saved).

Hefesto MAY use any data structure provided the resulting set of channel rectangles is identical. See Q-LEGACY-1 and Q-LEGACY-2 for flag-handling quirks that affect results.

Compatibility: exact
(Magic ref: router/rtrDcmpose.c:120-330, router/rtrDcmpose.c:430-1030)

#### Scenario: Empty area is one channel
- **GIVEN** `G = 8`, no subcells, box (0,0)-(40,24)
- **WHEN** decomposition runs
- **THEN** exactly one channel (-4,-4)-(44,28) results

#### Scenario: Single cell in the middle (verify against Magic)
- **GIVEN** `G = 8`, subcell bounding box (16,16)-(32,32), subcell separations 5 up / 7 down, routing area (-4,-4)-(76,76)
- **WHEN** decomposition runs
- **THEN** the cell silhouette is (4,4)-(52,52) and the space around it is divided into four channels whose boundaries extend from the silhouette corners in the direction of the shorter distance

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

### Requirement: Global routing
The global router SHALL assign, for every net, a sequence of crossings connecting its terminals' stem tips, as follows:

1. Nets with fewer than two terminals, or fewer than two terminal locations in total, are skipped. Nets are processed in increasing order of `width + height` of the bounding box of all terminal locations (ties in insertion order are not guaranteed; see Q-LEGACY-6).
2. Penalty constants (in grid units, multiplied by `G` once per session on first use, see Q-LEGACY-7): channel 1, jog 5, obstacle1 5, obstacle2 3, neighbor1 2, neighbor2 5, orphan 3.
3. For a net, the first terminal (in netlist order) having locations seeds the start list with all its stem tips, each with cost = Manhattan distance from the label rectangle to the tip (min over the two x-edges plus min over the two y-edges). Each subsequent terminal is routed: for each of its locations, find the best path from any start point; keep the cheapest over all locations; mark its crossings; add the path crossings and all of the terminal's locations to the start list. If no path exists: feedback `Can't find a path from "%s" to "%s"` (this terminal and the last connected terminal) over the label expanded by 1.
4. Path search is A* over crossings: from a crossing inside a channel tile, neighbor channel tiles across each side are visited; for each the closest free crossing pair (both pins unused and linked) along the shared edge is chosen (closest first, alternating outward). Cost to a crossing = previous cost + Manhattan distance + channel penalty; heap key = cost + Manhattan distance to the destination. In river (or density-saturated) tiles only the straight-across crossing is allowed. A first pass finds the shortest-length path (pruning a crossing if reached more cheaply before); a second pass enumerates paths in increasing length, adjusts each path's crossings to minimize total penalty, and stops when a path's raw length reaches the best adjusted cost.
5. Penalty for a channel segment entry→exit: Manhattan distance + channel penalty; + jog penalty if entry and exit differ in both x and y index; + obstacle penalties on the far-side pin (if in a normal channel) and on the exit pin: obstacle1, plus obstacle2×size if flagged obstacle, or `max(obstacle2×size - distance, 0)` if flagged hazard (applies also when only size ≠ 0); + neighbor2 if both neighbor pins on the exit side are used, neighbor1 if one is; + orphan if the path turns and the pin opposite the exit is unused. Through a river channel with a lookahead the opposite pin must be free or the cost is infinite.
6. Marking a path increments a per-net segment id per channel traversed; a channel's column/row density is incremented over the spanned range where the net was not already present; when a density reaches the channel capacity (rows for column density, columns for row density) the saturated span becomes impassable in that direction for subsequent nets.

Compatibility: exact
(Magic ref: grouter/grouteMain.c:60-320, grouter/grouteMult.c:112-290, grouter/grouteMaze.c:60-610, grouter/grouteCrss.c:40-795, grouter/grouteDens.c:64-185, grouter/grouteChan.c:667-800, utils/netlist.c:290-341)

#### Scenario: Jog penalty
- **GIVEN** `G = 8` and a path entering a channel at column 2 bottom and leaving at track 3 right
- **WHEN** the segment cost is computed
- **THEN** it includes the jog penalty 40 and the channel penalty 8 in addition to the Manhattan distance

### Requirement: Greedy channel router
Each channel SHALL be routed by a column-sweep greedy router with the following structure:

1. Orientation: if `width < length` the channel is routed as is; otherwise it is transposed (x↔y). If that attempt has errors, a left-right mirrored copy is routed and kept if it has strictly fewer errors (its feedback list then replaces the first attempt's). The kept result is mapped back.
2. River channels: a horizontal river channel with no used top/bottom pins and matching left/right pins (same net and segment) is routed by straight horizontal runs on every used left pin row; vertical river channels symmetric. Otherwise the message `Failing because ...` is printed and normal routing is used.
3. Nets are built from pins (left, then bottom/top interleaved by column, then right), keyed by net and segment. End distance `E = max(1, floor((multi/2 + right/4) * endConstant))` where `right` counts each net's trailing pins on the right edge and `multi` counts nets with more than one such pin. Density per column is computed; if max density exceeds `width`, feedback `Density (%d) > channel size (%d)`.
4. Flags from the obstacle map: track-contact where a clear point is followed by a layer-1-only obstacle to the right, track-end where followed by a doubly blocked point, column-contact / column-end symmetric for layer 2 upward; points blocked on both layers are track-end and column-end.
5. Column 0 takes the left pins as initial tracks. For each column i = 1..length: (a) make feasible top and bottom connections to the nearest track that is empty or holds the net (top searches downward from `width`, bottom upward from 1, stopping at blocking); conflicts resolved by giving priority to the shorter connection (if the same net, both connect); failures report `Can't make top connection` / `Can't make bottom connection`; (b) near the end (`length + 1 - i <= E`) mark right-edge wanted tracks; (c) collapse split nets choosing, among all legal patterns, the one freeing most tracks, then with outermost uncollapsed split nets farthest from the edge, then most vertical wiring; (d) reduce range of split nets by jogging outermost tracks inward (jogs >= minimum jog length); (e) vacate tracks wanted by other nets or in hazard shadows, shortest jog first; (f) jog rising/falling nets (target pin within the steady-net window) toward their target, longest distance first, only if the jog is at least the minimum jog length or near the end and reaching a right pin, and only if it reduces distance to target; (g) near the end, split nets with multiple right-edge pins (uncollapse) with the same pattern evaluation; (h) extend tracks to the next column, terminating nets with no further pins, reporting `Can't extend track through obstacle` or `Can't extend track to bad connection` when blocked.
6. After the last column, every track whose net differs from the right pin reports `Can't make end connection`.
7. Errors are feedback squares of size 4×4 centered on the grid point of (column, track), text `Net `%s', channel %p:  <message>` (the pointer text is informative).

The precise tie-breaking rules of each step SHALL follow the referenced algorithm exactly; see Q-LEGACY-8 for the column-contact condition quirk.

Compatibility: exact
(Magic ref: gcr/gcrRoute.c:40-320, gcr/gcrInit.c:40-398, gcr/gcrFeas.c:40-248, gcr/gcrColl.c:30-397, gcr/gcrEdge.c:30-275, gcr/gcrUnsplit.c:59-610, gcr/gcrLib.c:30-456, gcr/gcrFlags.c:40-144, gcr/gcrRiver.c:40-179, router/rtrChannel.c:70-200, router/rtrFdback.c:60-236)

#### Scenario: Straight-through net
- **GIVEN** a channel of length 6 and width 4 with left pin 2 and right pin 2 on the same net and no obstacles
- **WHEN** it is channel routed
- **THEN** track 2 carries the net from column 0 to column 6 with no jogs and no errors

#### Scenario: End distance
- **GIVEN** end constant 1.0, 4 nets each with exactly one trailing right pin, and 2 nets each with 2 trailing right pins
- **WHEN** the end distance is computed
- **THEN** `right = 8`, `multi = 2`, and `E = 1 + 2 = 3`

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
