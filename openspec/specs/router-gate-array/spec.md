# Router: Gate-Array Router (garoute)

## Purpose

Defines the gate-array router driven by the `garoute` command: user-defined routing channels (normal and river-routing), automatic channel-definition generation, channel initialization shared with the netlist router, stem assignment for terminals inside river-routing channels ("internal" stems with simple and maze-routed variants), and the overall routing flow. Global routing, greedy channel routing, paint-back and external stems are specified in capability `router-channel` and are only referenced here. Maze routing used for stems is specified in `router-maze`. This capability is desktop-only in the first iPadOS release (decision P-13; capability `hefesto-ipad-feature-scope`).

## Requirements

### Requirement: garoute command dispatch
The `garoute` command SHALL accept an optional subcommand matched case-insensitively as a unique prefix (exact match wins). With no subcommand it SHALL behave as `garoute route`. Subcommands:

| Subcommand | Arguments | Behavior |
|---|---|---|
| `channel` | `[type]` | Define a channel over the current box. |
| `channel` | `xl yl xh yh [type]` | Define a channel over the given coordinates (parsed as layout coordinates with the usual unit suffix rules of the command parser). |
| `generate` | `type [file]` | Write channel-definition commands for the box area to `file` (created/truncated) or to standard output. |
| `help` | – | Print `Gate-array router commands have the form:` / `"garoute option", where option is one of:` followed by the option list. |
| `nowarn` | – | Leave feedback only when all locations of a terminal are unusable (default). Prints `Will only leave feedback if all locs for a terminal are unusable.` |
| `route` | `[netlist]` | Route the edit cell. |
| `reset` | – | Print `Clearing all channel information.` and delete all channel definitions. |
| `warn` | – | Leave feedback for every unusable terminal location. Prints `Will leave feedback for each unusable terminal loc.` |

`type` is recognised by its first character only (see Q-LEGACY-22): `h` = horizontal river channel, `v` = vertical river channel; any other value reports `Unrecognized channel type: %s` (for `generate` additionally `Legal types are "h" or "v"`). Omitted type for `channel` = normal channel. Errors: `Ambiguous option: "%s"` / `Unrecognized routing command: %s` followed by `    Type "garoute help" for help.`; wrong argument counts print the usage line of the subcommand (`Usage: garoute channel xlo ylo xhi yhi [type]`, `Usage: garoute generate type [file]`, `Usage: garoute route [netlist]`, `Usage: "garoute warn" or "garoute nowarn"`). An unopenable output file reports the OS error for that path. The warn setting persists for the session.

Compatibility: behavioral
(Magic ref: router/rtrCmd.c:270-420)

#### Scenario: Channel with explicit type
- **GIVEN** `G = 8`, origin (0,0)
- **WHEN** `garoute channel 4 4 60 28 h` is executed
- **THEN** `Channel [HRIVER] 4 4 60 28` is printed and a horizontal river channel is defined

#### Scenario: Unknown type
- **WHEN** `garoute channel x` is executed with a box
- **THEN** `Unrecognized channel type: x` and the channel usage line are reported and no channel is defined

### Requirement: Channel definition and alignment
Defining a channel over rectangle r SHALL first print `Channel [%s] %d %d %d %d` with the type name `NORMAL`, `HRIVER` or `VRIVER` and the coordinates as given, then align the rectangle to center-grid positions: `xbot = gridUp(xbot, Ox) - half`, `ybot = gridUp(ybot, Oy) - half`, `xtop = gridDown(xtop, Ox) + G - half`, `ytop = gridDown(ytop, Oy) + G - half` (notation as in `router-channel`). If any coordinate changed it SHALL print `Rounding channel to center-grid alignment: ll=(%d,%d) ur=(%d,%d)` with the new values. If the aligned rectangle overlaps any previously defined channel area it SHALL report `Channel ll=(%d,%d) ur=(%d,%d) overlaps existing channels` and `Channel definition failed.` and define nothing. Otherwise the channel's columns/tracks/origin are computed as in `router-channel` "Channel geometry" and the channel is prepended to the channel list. Channel definitions are not saved in any file and SHALL be cleared after every `garoute route` (successful or not, once routing started) and by `garoute reset`.

Compatibility: exact
(Magic ref: garouter/gaChannel.c:60-185, router/rtrChannel.c:220-275)

#### Scenario: Alignment message
- **GIVEN** `G = 8`, origin (0,0)
- **WHEN** `garoute channel 0 0 40 24` is executed
- **THEN** the channel becomes (-4,-4)-(44,28) and `Rounding channel to center-grid alignment: ll=(-4,-4) ur=(44,28)` is printed

#### Scenario: Overlap rejected
- **GIVEN** a channel (-4,-4)-(44,28) is already defined
- **WHEN** `garoute channel 16 16 80 80` is executed
- **THEN** the overlap error and `Channel definition failed.` are reported and the channel count is unchanged

### Requirement: Channel definition generation
`garoute generate h|v [file]` SHALL produce `garoute channel` command lines covering the box area as follows:
1. For `h`: `ytop = gridDown(ytop - halfUp, Oy) + halfUp`, `ybot = gridUp(ybot + halfDown, Oy) - halfDown`, where `halfDown = floor(G/2)` and `halfUp = G - halfDown`. For `v` the same applies to x.
2. For each subcell use overlapping the area, take the bounding box of its paint on the planes containing any routing obstacle type or the layer-1, layer-2 or contact type, transform to edit coordinates and clip to the area; skip if empty.
3. For `h`: extend that box to the full area height, expand left by `subcellSepDown` and right by `subcellSepUp`, then `xtop = gridUp(xtop + halfDown, Ox) - halfUp`, `xbot = gridDown(xbot - halfUp, Ox) + halfDown`. For `v` symmetric in y with full area width.
4. The union of these boxes is decomposed into maximal horizontal strips (for `h`) or maximal vertical strips (for `v`), with space decomposed the same way; every resulting tile clipped to the area and non-empty is written as `garoute channel %d %d %d %d` followed by ` h` or ` v` for covered (cell) tiles and nothing for space tiles, one per line, in tile enumeration order (top to bottom, left to right).

```ebnf
gen_file  = { gen_line } ;
gen_line  = "garoute channel" , " " , int , " " , int , " " , int , " " , int , [ " " , ( "h" | "v" ) ] , "\n" ;
```

Compatibility: exact
(Magic ref: garouter/gaTest.c:144-360)

#### Scenario: Empty area generates one normal channel
- **GIVEN** `G = 8`, no subcells, box (0,0)-(40,24)
- **WHEN** `garoute generate h` is executed
- **THEN** a single line `garoute channel 0 4 40 20` is written (the y extent rounded inward to center-grid lines, x unchanged)

### Requirement: Gate-array routing flow
`garoute route [netlist]` SHALL:
1. Require an mzrouter style named `garouter` (see `router-maze`); if absent report `Could not initialize maze router.` then `Couldn't route at all.` and stop.
2. Require at least one channel; otherwise `Must define channels before routing.` / `Couldn't route at all.`.
3. Select the netlist: the argument if given, otherwise the current netlist, otherwise the edit cell's name (printing `No netlist selected yet; using "%s".`). Build it; zero nets reports `No nets to route.` but routing continues (see Q-LEGACY-20).
4. Routing area = bounding box of all channel areas and all net areas.
5. Run channel initialization, global routing, channel routing, paint-back and stem painting exactly as specified in `router-channel` (same steps as `route` but using the user-defined channels and their types).
6. Clear all channel definitions.
7. Report `%d routing error%s.` (plural `s` unless 1) or `No routing errors.`; the error count is the number of feedback areas added.

Compatibility: behavioral
(Magic ref: garouter/gaMain.c:180-420, router/rtrCmd.c:395-412)

#### Scenario: No channels
- **GIVEN** an mzrouter `garouter` style exists and no channels are defined
- **WHEN** `garoute route` is executed
- **THEN** `Must define channels before routing.` and `Couldn't route at all.` are reported

### Requirement: Channel initialization
Before global routing the router SHALL, in order:
1. For each channel (list order): split channel-plane tiles at the channel boundary, build the obstacle map (`router-channel` "Channel obstacle map"), initialise densities for normal channels only, then clear the density-only flags.
2. Associate channel-plane tiles with channels; initialize pins (coordinates and links).
3. Assign stems for all terminal locations (next requirements), in netlist order of nets and terminals.
4. For each river channel: for every track (h) or column (v) index n, search the strip `[coord - subcellSepUp, coord + subcellSepDown]` across the full channel for layer-1 obstacles AND layer-2 obstacles in the edit cell hierarchy; if both are found, mark both end pins of that track/column as blocked unless already taken.
5. Repeatedly propagate blockage until no change: a blocked pin blocks its linked pin (if free); in river channels it also blocks the pin on the opposite side; obstacle flags propagate to linked pins.
6. Compute hazards for normal channels; link the free pins of each side for the global router.

Compatibility: exact
(Magic ref: garouter/gaChannel.c:190-640, router/rtrPin.c:261-345)

#### Scenario: River track blocked on both layers
- **GIVEN** a horizontal river channel whose track 2 strip contains metal1 and metal2 obstacles
- **WHEN** channels are initialized
- **THEN** left pin 2 and right pin 2 are marked blocked unless they were already assigned to a stem

### Requirement: Terminal classification for stems
For each terminal location the router SHALL determine the containing channel: a zero-area location is first grown by 1 in each degenerate axis and must then overlap at most one channel; a location overlapping more than one channel is rejected (`Terminal overlaps a channel boundary`). A location inside a normal channel is rejected (`Terminal is inside a normal routing channel`). A location inside a river channel gets internal stems; a location outside every channel gets external stems as in `router-channel` "Terminal stems outside channels". Feedback for individual locations is emitted only in warn mode; when no location of a terminal is usable, `No crossing reachable from terminal` is emitted (in nowarn mode) over the location expanded by 1.

Compatibility: exact
(Magic ref: garouter/gaStem.c:150-330, router/rtrStem.c:90-170)

#### Scenario: Terminal inside a normal channel
- **GIVEN** warn mode and a terminal label inside a normal channel
- **WHEN** stems are assigned
- **THEN** feedback `Terminal is inside a normal routing channel` is placed over the label and the location is discarded

### Requirement: Internal stem grid line selection
For a terminal rectangle r inside a horizontal river channel the candidate grid lines SHALL be: `min = gridDown(r.ybot, Oy)`, `max = gridDown(r.ytop - maxAbove, Oy)`, `start = gridDown(floor((min+max)/2), Oy)` (C truncating division then grid-down), and if `start < r.ybot` and `start + G < r.ytop` then `start += G`; finally `min = min(min, start)`, `max = max(max, start)`. `maxAbove = max(layer1Width, layer2Width, contactWidth - contactOffset)`. Vertical river channels use x symmetrically. Lines are tried in the order start, start−G, start+G, start−2G, start+2G, ... within [min, max]; the first line for which at least one side succeeds is used (no attempt to find a line usable on both sides). Failure on all lines yields `Terminal can't be brought out to either channel boundary` (warn mode).

Compatibility: exact
(Magic ref: garouter/gaStem.c:330-420, garouter/gaStem.c:708-770)

#### Scenario: Grid line choice
- **GIVEN** `G = 8`, origin 0, maxAbove 3, terminal y-range [10, 30] in a horizontal river channel
- **WHEN** the grid line is chosen
- **THEN** `min = 8`, `max = gridDown(27) = 24`, `start = gridDown(16) = 16`, and lines are tried in the order 16, 8, 24

### Requirement: Internal stem pin checks
For a chosen grid line, each of the two crossings (west/east for horizontal river channels, south/north for vertical) SHALL be accepted only if: the river channel's pin is free, it is linked to a pin of an adjacent channel which is also free; no other netlist terminal location overlapping the corridor between the terminal and the crossing (corridor = from the terminal edge to the crossing, `±subcellSepUp/Down` around the grid line) has the same preferred start grid line; and the connection is possible on every routing layer not blocked at the outside pin — first by a "simple" stem, otherwise by maze routing (garouter style) to both layer 2 and layer 1. Accepted crossings mark the OUTSIDE pin with the net and a stem marker, set the location's crossing, direction and channel to the outside channel; two accepted crossings create a second location for the terminal.

Compatibility: exact
(Magic ref: garouter/gaStem.c:420-706)

#### Scenario: Second terminal on same line blocks the corridor
- **GIVEN** terminals A and B side by side on the same preferred grid line in a horizontal river channel, A to the west of B
- **WHEN** stems are assigned for A
- **THEN** A's east crossing is rejected because B lies in the corridor with the same preferred grid line, and A uses only the west crossing

### Requirement: Simple stems
A simple stem from terminal rectangle T to crossing point P on side s SHALL be considered only if the grid line coordinate `g` (P.x for north/south, P.y for east/west) satisfies `g >= T.lo` and `g + max(layer1Width, layer2Width) <= T.hi` along the terminal's perpendicular extent. Geometry for side north (others symmetric):
- terminal contact `cTerm = [P.x + contactOffset, +contactWidth] × [T.ytop - contactWidth, T.ytop]`;
- pin contact `cPin = [P.x + contactOffset, +contactWidth] × [gridDown(P.y) + contactOffset, +contactWidth]`;
- for each layer L with width w: pin stub `[P.x, P.x+w] × [cPin.ytop, P.y]`, short wire `[P.x, P.x+w] × [T.ytop, cPin.ybot]`, long wire `[P.x, P.x+w] × [T.ytop, P.y]`.
Check areas are expanded by the per-layer clearance (max separation of that layer; contact clearance = max(clear1 + metalSurround, clear2 + polySurround)) and clipped to the strip between the terminal and P (except the terminal contact). A wire is OK if its check area contains no obstacle of its layer; a short wire additionally requires the other layer's stub to be clear. Route preference for target layer L: if the terminal connects to L: long wire on L; else short wire on the other layer + pin contact + stub on L (if the terminal connects to the other layer); else terminal contact + short other-layer wire + pin contact + stub. If the terminal does not connect to L: terminal contact + long wire on L; else short other-layer wire + pin contact + stub on L. At paint time layer 1 is tried before layer 2. See Q-LEGACY-21.

Compatibility: exact
(Magic ref: garouter/gaSimple.c:40-530)

#### Scenario: Direct long wire
- **GIVEN** a metal1 terminal (8,0)-(16,12) inside a vertical river channel, crossing P = (8,28) to the north, layer 1 = metal1 width 3, no obstacles
- **WHEN** the simple stem to metal1 is painted
- **THEN** metal1 is painted over (8,12)-(11,28) with no contacts

### Requirement: Maze-routed stems
When a simple stem is impossible (or when maze stems are enabled with `route mazestems`), the router SHALL invoke the maze router with a copy of the `garouter` style modified to: expand endpoints, use only top-level hints, and a bloom limit of 100. The route is confined by a fence covering `bbox(T, P)` expanded by `2 × max active route-layer width` on all sides; the start is P on the first active route layer whose type is in the allowed pin-layer set; the destination is T on the label's type. During stem assignment the route is only checked (no paint); during stem painting the route is painted and copied into the edit cell, followed by redisplay and DRC re-check. If maze routing fails at paint time the feedback `Couldn't maze route final connection` is placed over `bbox(T, P)` (expanded by 1 if degenerate).

Compatibility: behavioral
(Magic ref: garouter/gaMaze.c:85-400, garouter/gaStem.c:1010-1180, garouter/gaInternal.h:42)

#### Scenario: Missing garouter style during maze stems
- **GIVEN** `route mazestems` is on and the technology has no `garouter` mzrouter style
- **WHEN** `route` paints a stem that requires the maze router
- **THEN** feedback `Couldn't maze route final connection` is left for that terminal

### Requirement: Gate-array debugging
The wizard command `*garoute` and its debug flags (`chanonly`, `chanstats`, `maze`, `nosimple`, `paintstems`, `showchans`, `showmaze`, `stems`, `verbose`, `noclean`) SHALL be treated as developer diagnostics; the statistics lines `Total pins: %d, clear: %d (%.1f%%)` etc. are informative.

Compatibility: informative
(Magic ref: garouter/gaTest.c:60-140, garouter/gaTest.c:365-417, garouter/gaChannel.c:330-400)

#### Scenario: Debug flag listing
- **WHEN** `*garoute` lists its debug flags
- **THEN** the ten flag names above are shown
