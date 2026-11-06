# Open Questions — LEGACY area

Area LEGACY covers the routers (`router-channel`, `router-gate-array`, `router-maze`, `router-maze-iroute`, `router-net2ir`), plowing (`plow`), the wiring tool (`wiring-tool`), netlists (`netlist-window`), simulator interface and node naming (`irsim-interface`), and the informative `scheme-interpreter` and `openaccess` capabilities.

Each entry records a Magic behavior (at commit fc5daa9e) that looks accidental, undocumented, or host-specific. The Hefesto team decides whether to replicate or fix it. IDs are stable; gaps in numbering are intentional (ranges are reserved per sub-area: 1–19 channel router, 20–29 gate-array/net2ir, 30–49 maze router, 50–69 plow/wiring, 70–89 netlist/simulator).

### Q-LEGACY-1: Channel decomposition marks bottom edges using the area top
- Kind: bug
- Magic behavior: When initializing boundary flags, a space tile's bottom edge is marked as a channel boundary only if the tile's BOTTOM equals the routing area's TOP (never true inside the area), so bottom edges on the routing-area boundary are not pre-marked.
- Evidence: router/rtrDcmpose.c:516-521 (`if (BOTTOM(tile) == area->r_ytop)`)
- Options: replicate (needed for identical channel sets) | fix (compare with r_ybot)
- Recommendation: replicate for `route` compatibility; the router is legacy and golden comparisons depend on it.

### Q-LEGACY-2: Vertical split copies NE flag into SE slot
- Kind: bug
- Magic behavior: When a space tile is split at a corner x, the new tile's SE flag is set from the old tile, but when the old SE flag is clear the code clears the NE flag of the new tile instead of SE.
- Evidence: router/rtrDcmpose.c:735-738
- Options: replicate | fix (clear SE)
- Recommendation: replicate; affects only decomposition tie cases.

### Q-LEGACY-3: Obstacle map uses grid-up for columns but grid-down for rows
- Kind: quirk
- Magic behavior: The last blocked column is computed with gridUp(xtop + sepUp - 1) while the last blocked row uses gridDown(ytop + sepUp - 1); obstacles are thus treated asymmetrically in x and y.
- Evidence: router/rtrChannel.c:470-485
- Options: replicate | fix (use gridDown for both)
- Recommendation: replicate.

### Q-LEGACY-4: Hazard marking over horizontal obstacle covers only rows bot-1..bot+1
- Kind: bug
- Magic behavior: For left/right hazards the obstacle body is flagged on rows rb-1..rb+1 instead of rb-1..rt+1 (the vertical case uses the full range).
- Evidence: router/rtrHazards.c:520-523
- Options: replicate | fix (use rt+1)
- Recommendation: replicate (changes routing results).

### Q-LEGACY-5: Stem tip search tests the upper candidate with >= instead of <=
- Kind: bug
- Magic behavior: While searching alternate stem tips, the upper candidate is tried only when its coordinate is >= stem_hi (instead of <=), so candidates above the start are effectively skipped until the range is exceeded, and the loop can test one point beyond the range.
- Evidence: router/rtrStem.c:395-400
- Options: replicate | fix
- Recommendation: replicate for identical routes; document as a known Magic defect.

### Q-LEGACY-6: Net ordering ties depend on heap implementation
- Kind: ambiguity
- Magic behavior: Nets are ordered by bounding box half-perimeter via a binary heap; equal keys come out in an order determined by Magic's heap algorithm, not by netlist order.
- Evidence: utils/netlist.c:306-341, utils/heap.c
- Options: replicate Magic's heap exactly | define stable order (netlist order on ties)
- Recommendation: replicate Magic's heap (sift-up/down) to keep golden results; record in implementation notes.

### Q-LEGACY-7: Global router penalties scaled only once per session
- Kind: quirk
- Magic behavior: Penalty constants are multiplied by the grid spacing the first time global routing runs; later technology reloads or grid changes do not rescale them.
- Evidence: grouter/grouteCrss.c:780-795
- Options: replicate | fix (scale per route)
- Recommendation: fix (compute penalties from current G each run); difference only visible after a tech reload in the same session.

### Q-LEGACY-8: Greedy router column-contact skip condition precedence
- Kind: quirk
- Magic behavior: When trying a vertical run, a track needing a column contact is skipped if NOT (near end and wanted by the net) OR (source ends and nothing found yet) — the second clause makes "source ends" a reason to skip rather than an exception, contrary to the comment.
- Evidence: gcr/gcrUnsplit.c:575-579
- Options: replicate | fix to match the comment
- Recommendation: replicate.

### Q-LEGACY-20: garoute continues routing when the netlist has no nets
- Kind: quirk
- Magic behavior: When the netlist builds zero nets, `No nets to route.` is printed but the caller only aborts on a negative count, so channel initialization and an (empty) routing pass still run and channels are cleared.
- Evidence: garouter/gaMain.c:218-221, garouter/gaMain.c:440-447
- Options: replicate | fix (abort with "Couldn't route at all.")
- Recommendation: fix; observable difference is only that channel definitions are consumed.

### Q-LEGACY-21: Simple-stem terminal-contact check overwritten
- Kind: bug
- Magic behavior: The terminal-contact clearance flag is assigned from the layer-1 check and then unconditionally overwritten by the layer-2 check when the terminal connects to neither routing layer; when the terminal is a contact (connects to both) the flag is never assigned (uninitialized).
- Evidence: garouter/gaSimple.c:455-468
- Options: replicate (AND of checks performed last) | fix (logical AND of both checks; false when unassigned)
- Recommendation: fix to AND of both checks and treat unassigned as "not OK"; document possible routing differences.

### Q-LEGACY-22: Channel type keyword matched by first character only
- Kind: quirk
- Magic behavior: `garoute channel ... horse` is accepted as `h`; any word starting with h or v selects a river channel.
- Evidence: router/rtrCmd.c:340-350, router/rtrCmd.c:378-386
- Options: replicate | fix (require h/v or unique prefix of hriver/vriver)
- Recommendation: replicate (harmless, keeps scripts working).

### Q-LEGACY-24: net2ir leaves an unterminated command for a net with one located terminal
- Kind: bug
- Magic behavior: If only the first terminal of a net is located, `:iroute route -slayers ... -sPoint x y ` is written without a newline and the next net's command is appended to the same line.
- Evidence: net2ir/net2ir.c:150-175
- Options: replicate | fix (suppress or terminate the fragment)
- Recommendation: fix (emit nothing for nets with fewer than two located terminals) and warn.

### Q-LEGACY-25: net2ir treats comment lines as terminal names
- Kind: quirk
- Magic behavior: Lines starting with `#` (comments in .net files) are looked up as terminals, producing `No location for terminal #...` diagnostics.
- Evidence: net2ir/net2ir.c:136-150
- Options: replicate | fix (skip comment lines as the netlist reader does)
- Recommendation: fix.

### Q-LEGACY-30: Fence semantics differ from documentation
- Kind: doc-mismatch
- Magic behavior: The route is confined to the same side of fence paint as the first start point (inside if the start is inside fence paint); the command reference says the fence is a boundary "inside of which Magic will not route".
- Evidence: mzrouter/mzMain.c:580-610, doc/html/iroute.html
- Options: replicate code | follow doc
- Recommendation: replicate code behavior; correct the Hefesto documentation.

### Q-LEGACY-31: Malformed mzrouter contact line leaves a half-registered contact
- Kind: bug
- Magic behavior: A `contact` line whose second layer or cost is invalid has already been added to the first layer's contact list (and the route-type list) before the error, so a partially defined contact can be used by the search.
- Evidence: mzrouter/mzTech.c:725-790
- Options: replicate | fix (validate all fields before registering)
- Recommendation: fix; report the error and ignore the whole line.

### Q-LEGACY-32: Maze penalty stored with 0.5 resolution
- Kind: quirk
- Magic behavior: The penalty factor is stored as integer mantissa with exponent 1, i.e. floor(p*2)/2; `search ... 1.3` yields 1.0.
- Evidence: mzrouter/mzTech.c:640-660, irouter/irCommand.c:620-655, mzrouter/mzInternal.h:67-68
- Options: replicate | fix (store as double)
- Recommendation: replicate (affects search order and therefore routes).

### Q-LEGACY-33: mzrouter `width` statement is overridden by DRC-derived width
- Kind: quirk
- Magic behavior: Widths/lengths from `width` lines are overwritten when widths are re-derived from DRC rules after the tech file is read, so the statement has no lasting effect.
- Evidence: mzrouter/mzTech.c:150-170, mzrouter/mzTech.c:660-710
- Options: replicate | fix (explicit width overrides DRC like explicit spacing)
- Recommendation: fix with an explicit-override rule, but log a compatibility note; existing PDKs rarely use `width`.

### Q-LEGACY-34: Documented `iroute route node_name` and `expandDests` do not exist
- Kind: doc-mismatch
- Magic behavior: The command reference documents `iroute route node_name...` and a wizard parameter `expandDests`; the implementation only accepts option flags and names the parameter `expandEndpoints`; the reference also omits the `length` and `overCost` parameters.
- Evidence: doc/html/iroute.html, irouter/irCommand.c:1445-1460, irouter/irCommand.c:2240-2252
- Options: replicate code | add aliases
- Recommendation: replicate code; optionally accept `expandDests` as an alias.

### Q-LEGACY-35: Spacing listings include undefined type slots
- Kind: bug
- Magic behavior: Spacings are derived for every type index up to the maximum type count; unused indexes get spacing 0, so `iroute spacings` and `saveParameters` print entries for them with a null type name (`(null)` on common C libraries).
- Evidence: mzrouter/mzTech.c:180-186, irouter/irCommand.c:1890-1900, irouter/irCommand.c:2420-2432
- Options: replicate | fix (only defined types)
- Recommendation: fix; emit only defined tile types.

### Q-LEGACY-36: Displaying a boolean parameter reports an error
- Kind: bug
- Magic behavior: The boolean parameter helper reports `Unrecognized boolean value: "(null)"` whenever it is called only to display a value (no new value), e.g. in `iroute layers`, `iroute wizard estimate` and `saveParameters`.
- Evidence: utils/set.c (SetNoisyBool: result initialised to -2 and checked even when no value given)
- Options: replicate | fix
- Recommendation: fix (suppress the message when no value was supplied).

### Q-LEGACY-50: Plowing of non-Manhattan (split) tiles
- Kind: ambiguity
- Magic behavior: The plow algorithm works on corner-stitched edges and was written before split (diagonal) tiles existed; its handling of split tiles is not specified by Magic and is untested.
- Evidence: plow/PlowMain.c (no split-tile handling), plow/TODO
- Options: replicate whatever Magic does (requires golden capture) | refuse to plow areas with non-Manhattan geometry | treat diagonals by bounding tiles
- Recommendation: capture golden cases first (LEG-PL-08); until then refuse with a clear message.

### Q-LEGACY-51: Initial wiring type is not "none"
- Kind: bug
- Magic behavior: The current wiring type is initialised to the type number just below the first selectable type (an internal DRC type), while the "no wiring material selected" check tests for type 0, so `wire leg` before any `wire type` does not report the error and attempts to paint that internal type.
- Evidence: wiring/wireOps.c:40-45, wiring/wireOps.c:270-275
- Options: replicate | fix (initialise to "none" and report the error)
- Recommendation: fix.

### Q-LEGACY-52: Wiring operations dereference a missing window
- Kind: bug
- Magic behavior: After painting a leg or contact, the code looks up a window showing the edit cell's root and dereferences it without checking for NULL (crash when no such window exists, e.g. headless scripting).
- Evidence: wiring/wireOps.c:385-392, wiring/wireOps.c:845-850
- Options: replicate (crash) | fix (skip selection when no window)
- Recommendation: fix; also required for headless/iPadOS operation.

### Q-LEGACY-53: wire segment usage message lists coordinates in the wrong order
- Kind: doc-mismatch
- Magic behavior: The usage message says `x1 x2 y1 y2` but the arguments are parsed as `x1 y1 x2 y2 ...`.
- Evidence: commands/CmdTZ.c:2010-2012
- Options: replicate text | fix text
- Recommendation: fix the message (informative text).

### Q-LEGACY-70: Netlist write order depends on platform word size and char signedness
- Kind: quirk
- Magic behavior: Net and terminal order in written `.net` files (and the routers' net order) follows Magic's string hash table, whose hash uses `unsigned long` arithmetic (64-bit on LP64 hosts, 32-bit elsewhere) and plain `char` (signed on x86, unsigned on ARM) for non-ASCII bytes; the same netlist can therefore be written in different orders on different builds.
- Evidence: utils/hash.c:218-262, netmenu/NMnetlist.c:470-575, utils/netlist.c:60-80
- Options: replicate the LP64/signed-char variant exactly | write nets in a stable order (e.g. file order)
- Recommendation: replicate the 64-bit, signed-char behaviour (the common Linux x86-64 build) so golden files match; document that order is otherwise not semantically significant.

### Q-LEGACY-71: [platform] Simulator and helper processes on iPadOS
- Kind: platform
- Magic behavior: `rsim`/`startrsim` fork/exec the simulator and talk over pipes (path from `RSIM` env var or install dir); the Tcl build loads IRSIM as a Tcl package; the Scheme interpreter offers `spawn`. iPadOS forbids fork/exec and has no Tcl.
- Evidence: sim/SimRsim.c:140-280, doc/html/irsim.html, lisp/lispIO.c
- Options: platform alternative — link a simulator engine in-process (e.g. IRSIM core as a library) or connect to a remote simulation service; on Linux/macOS a child-process backend may also be offered
- Recommendation: define a simulator-backend interface in Hefesto; ship an in-process backend for iPadOS and allow a child-process backend on desktop. Node naming (exact) is shared by all backends.
- Resolution: decided by X-4 and P-13 (2026-11-06). IRSIM and the Scheme interpreter are desktop-only; on iPadOS embedded ngspice (P-7) provides simulation. Tcl is available on every platform (X-3), but sandboxed, so no process is spawned from scripts.

### Q-LEGACY-72: verify/cull short-circuit feedback text ends with a newline
- Kind: quirk
- Magic behavior: The feedback text for shorts is formatted with a trailing `\n` (`Net "a" shorted to net "b".\n`), unlike all other feedback messages; it appears in `feedback why` output and saved feedback files.
- Evidence: netmenu/NMwiring.c:986-990, netmenu/NMwiring.c:1162-1165
- Options: replicate | fix (drop the newline)
- Recommendation: fix in UI text; feedback text is informative.

### Q-LEGACY-73: getnode documentation vs. option words
- Kind: doc-mismatch
- Magic behavior: The reference documents `getnode globals`, while the code accepts any word beginning with `global`; `getnode alias`/`global` with one argument only print the current setting, and `abort` with no name clears the list — none of which is documented.
- Evidence: commands/CmdFI.c:1160-1230, doc/html/getnode.html
- Options: replicate | tighten parsing
- Recommendation: replicate (lenient parsing) and document.

### Q-LEGACY-74: Greedy router collapse evaluation reuses a stale split-distance cache
- Kind: bug
- Magic behavior: The tie-break between collapse/uncollapse patterns with equal freed-track counts compares "distance of the n-th uncollapsed split net from the channel edge" against a cache computed for the current best pattern. The cache is reset only once per channel, not when a new best is adopted on a freed-count win nor between columns, and the n-th entry is computed from the loop index rather than from the previous split position. Patterns are therefore compared against values belonging to earlier patterns/columns.
- Evidence: gcr/gcrColl.c:142-290 (gcrInitCollapse, gcrEvalPat, gcrNextSplit), gcr/gcrRoute.c:95
- Options: replicate | fix (recompute the best pattern's split distances on every comparison)
- Recommendation: replicate for golden compatibility; record the cache as explicit router state.

### Q-LEGACY-75: Reduce-range downward scan tests the wrong track for "clear"
- Kind: bug
- Magic behavior: When moving the highest track of a split net downward, the "source track is free of obstacles" test reads the flags of the low-side loop index i instead of the track being moved (W+1-i).
- Evidence: gcr/gcrColl.c:368-372
- Options: replicate | fix (test track W+1-i)
- Recommendation: replicate (changes jog choices near obstacles).

### Q-LEGACY-76: Vacate step uses a different "near the end" definition
- Kind: quirk
- Magic behavior: Everywhere else a column c is near the end when L+1-c <= E; the vacate step uses L-c <= E, i.e. one more column counts as near the end there.
- Evidence: gcr/gcrUnsplit.c:73, gcr/gcr.h:239
- Options: replicate | fix (use the common definition)
- Recommendation: replicate.

### Q-LEGACY-77: Right-edge wanted tracks never marked when E > L
- Kind: quirk
- Magic behavior: Right-pin tracks are marked "wanted" at near-end columns only if E < L or the previous column was not near the end. When E >= L+1, column 0 already counts as near the end, so the marking is never performed (only the per-net wanted marking at feasible connections and the first column applies).
- Evidence: gcr/gcrRoute.c:171-173
- Options: replicate | fix (always mark at the first near-end column)
- Recommendation: replicate; large end constants on short channels change results.

### Q-LEGACY-78: Downward corner distance one unit longer in channel decomposition
- Kind: bug
- Magic behavior: When measuring the vertical distance from a cell corner downward, a stop caused by a tile whose edge coincides with the corner x leaves the measured position one unit below the bottom of the last good tile, so the downward distance is one unit larger than the equivalent upward distance. This affects the `xDist < yDist` choice on ties.
- Evidence: router/rtrDcmpose.c:849-940 (rtrYDist)
- Options: replicate | fix (reset to the tile bottom as for the other stop reasons)
- Recommendation: replicate (channel shapes are golden-tested).

### Q-LEGACY-79: River tiles blocked only when both sides are full
- Kind: doc-mismatch
- Magic behavior: The comment says a river tile whose pins are completely blocked on one side becomes blocked; the code blocks it only when no usable pin exists on either side.
- Evidence: grouter/grouteChan.c:1025-1065 (glChanRiverBlock)
- Options: replicate | fix (block when either side is full)
- Recommendation: replicate.

### Q-LEGACY-80: Density-blockage scan reads one entry past the density vector
- Kind: bug
- Magic behavior: The run-extension loop reads density[hi] before checking hi < size, reading one element past the end when a saturated run reaches the last column/row. The bounds check still stops the loop, so geometry is unaffected unless the read faults.
- Evidence: grouter/grouteChan.c:667-760 (glChanBlockDens)
- Options: fix (check the bound first)
- Recommendation: fix; no observable change.

### Q-LEGACY-81: Uncollapse "connect to existing track" jog uses the wrong net
- Kind: bug
- Magic behavior: For an empty wanted track i, when the scan finds a track `to` already holding the wanted net, the jog is made with no net argument, so the net is taken from want[to], which may be empty or a different net; the vertical wiring may then be recorded with no net.
- Evidence: gcr/gcrEdge.c:235-240, gcr/gcrLib.c:115-118
- Options: replicate | fix (pass want[i])
- Recommendation: replicate pending golden evidence; flag in tests.

### Q-LEGACY-82: Top/bottom conflict priority differs from "shorter connection wins"
- Kind: quirk
- Magic behavior: When top and bottom pins of a column compete, the top pin wins if botTarget > W - topTarget - 1, i.e. unless the bottom jog is at least two tracks shorter than the top jog (W+1-topTarget). Equal or one-shorter bottom connections lose.
- Evidence: gcr/gcrFeas.c:145-160
- Options: replicate | fix (compare jog lengths directly)
- Recommendation: replicate.

### Q-LEGACY-83: Vacate search accepts occupied tracks above
- Kind: quirk
- Magic behavior: When looking for a track to vacate to, downward candidates must be empty, but upward candidates only need to be free of conflicting vertical wiring/obstacles and not wanted by another net; an occupied upper track can be returned, after which the vacate run fails because the full run cannot reach it.
- Evidence: gcr/gcrUnsplit.c:145-260 (gcrLook)
- Options: replicate | fix (require empty upward candidates)
- Recommendation: replicate.

### Q-LEGACY-84: Global search recursion into split pieces uses the original tile for crossings
- Kind: quirk
- Magic behavior: When a channel is split into several map tiles by density blockage, expansion recurses through the channel's own pieces; crossings into another channel found from a piece are enumerated using the shared edge between the ORIGINAL tile of the search point and the far tile, which may not be adjacent (the shared range can be empty or wrong).
- Evidence: grouter/grouteMaze.c:365-470 (glMazeTile), grouter/grouteCrss.c:292-370 (glCrossEnum)
- Options: replicate | fix (use the piece adjacent to the far tile)
- Recommendation: replicate; only matters for density-saturated channels.

### Q-LEGACY-85: Initial LR contact uses an inverted length test
- Kind: bug
- Magic behavior: When creating initial paths, an LR (horizontal-plane) contact at the start point is offered only when the free run to the right is NOT long enough (`xtop - x <= length - width`); the UD (vertical) variant uses the correct test (`ytop - y > length - width`). The in-search contact extension uses the correct test for both.
- Evidence: mzrouter/mzStart.c:330-350 vs mzrouter/mzStart.c:355-370, mzrouter/mzSearch.c:880-885
- Options: replicate | fix (use `>` like the other three sites)
- Recommendation: replicate for golden compatibility; consider a fix behind a compatibility flag.

### Q-LEGACY-86: Same-node zero-cost rule overwrites the parent path's estimated total
- Kind: quirk
- Magic behavior: After choosing an extension end point that lies in same-node area, if the origin path has no predecessor or its predecessor's estimated total is 0, the origin path's stored estimated total (cost so far + estimate) is overwritten with 0. The new path's total is then computed from 0 minus the origin's estimate-to-go, and the origin's hash entry now holds total 0, so any later path to that key is rejected.
- Evidence: mzrouter/mzXtndRght.c:555-560, mzXtndLeft.c:560-565, mzXtndUp.c:553-558, mzXtndDown.c:557-562, mzSearch.c:1290-1310
- Options: replicate | fix (track a separate "cost so far" and zero only that)
- Recommendation: replicate; it determines which start-node point routes leave from.

### Q-LEGACY-87: Upward extension ignores closer stop points in the free case
- Kind: bug
- Magic behavior: In the upward extension, when the next blockage tile is free and space is not constricting, the end point is unconditionally set to that tile's bottom after pruning, overriding a closer bounds edge, without updating the reason set. Right/left/down do not do this.
- Evidence: mzrouter/mzXtndUp.c:230-236
- Options: replicate | fix (remove the unconditional assignment)
- Recommendation: replicate; it changes routes and can run past generated blockage.

### Q-LEGACY-88: Same-node paint of a contact onto its own route type blocks half of it
- Kind: quirk
- Magic behavior: When start/destination material of a contact type c is painted as same-node into route type c itself, the first of the two same-node rectangles is painted same-node but the second is painted `blocked`.
- Evidence: mzrouter/mzBlock.c:410-450
- Options: replicate | fix (paint both same-node)
- Recommendation: replicate; document as Magic behavior.

### Q-LEGACY-89: Contact same-node exemption tests the wrong tile
- Kind: bug
- Magic behavior: When painting blocked material into a contact route type over same-node area, the code looks up the layer plane tile but tests the contact plane tile again, so the exemption applies whenever the material type is either layer of the first contact (contact order) of that type, regardless of the layer plane.
- Evidence: mzrouter/mzBlock.c:380-405
- Options: replicate | fix (test the layer plane tile)
- Recommendation: replicate the effective rule (as specified).

### Q-LEGACY-90: Integer overflow in estimate and adjusted-cost arithmetic
- Kind: bug
- Magic behavior: Dijkstra edge weights and dominance tests multiply two 32-bit ints before widening (`rate * distance`, `hcost * |dx|`), which overflows for distances above ~2^31 / cost; the adjusted-cost key multiplies the 64-bit estimate-to-go by the penalty mantissa, which overflows when the estimate is COST_MAX (unreachable point).
- Evidence: mzrouter/mzEstimate.c:1680, 1740, 1300-1310; mzrouter/mzSearch.c:519-524
- Options: replicate (wraparound) | fix (64-bit/ saturating arithmetic)
- Recommendation: fix with saturating 64-bit arithmetic; results are identical for realistic layouts.

### Q-LEGACY-91: Contact-to-contact spacing checked only within one contact class
- Kind: quirk
- Magic behavior: LR contact placement checks spacing only against the most recent LR contact ('O') and UD placement only against the most recent UD contact ('X'); interesting-point pruning also considers inactive contacts of the layer.
- Evidence: mzrouter/mzSearch.c:830-845, 1060-1075; mzrouter/mzXtndRght.c:425-445
- Options: replicate | fix (check the most recent contact of either class; skip inactive)
- Recommendation: replicate.

### Q-LEGACY-92: Start point is the lower-left of the first enumerated start tile
- Kind: quirk
- Magic behavior: The initial point of a start term is the lower-left corner of the first same-node/walk/destination tile found in the layer's horizontal blockage plane within the term rectangle grown by 1 at left/bottom; that corner can lie outside the term (merged tiles) and depends on corner-stitched enumeration order (upper-left first).
- Evidence: mzrouter/mzStart.c:60-66, 160-180
- Options: replicate (including tile enumeration order) | define a canonical point
- Recommendation: replicate; specify enumeration order in the tile-plane capability.

### Q-LEGACY-93: Destination contact-walk passes run with an invalid argument
- Kind: bug
- Magic behavior: Destination-area building runs the LR/UD contact-walk callbacks once directly over the destination-area cell with a tree-search context in place of the route type; no route type ever matches so the passes are no-ops. The effective contact walks come from the nested pass.
- Evidence: mzrouter/mzBlock.c:870-880, 1291-1330
- Options: replicate (no-op) | remove
- Recommendation: remove; no observable effect.

### Q-LEGACY-94: Vertical extensions use horizontal-strip contact blockage
- Kind: quirk
- Magic behavior: Contact-related interesting points are computed from each contact's horizontal-strip blockage plane in all four directions, so vertical moves stop at every horizontal strip boundary of contact blockage; layer planes use the strip form perpendicular to the move.
- Evidence: mzrouter/mzXtndUp.c:434, mzXtndDown.c:437
- Options: replicate | fix (use vertical-strip plane for vertical moves)
- Recommendation: replicate; it affects the set of interesting points and thus routes.

### Q-LEGACY-95: Stale paths still consume blooms
- Kind: quirk
- Magic behavior: A path taken from a heap to start a bloom increments the bloom counter (which drives the window offset) before the hashed-best check; if it is stale it is then discarded with no extension.
- Evidence: mzrouter/mzSearch.c:595-680, 166-200, 700-712
- Options: replicate | fix (check staleness before blooming)
- Recommendation: replicate; the window position depends on the bloom count.

### Q-LEGACY-96: Straight-shot estimators follow corner-neighbour chains only
- Kind: quirk
- Magic behavior: A straight-shot estimator looks for a destination only along the chain of neighbours at one corner of the tile (e.g. right neighbours at the top-right corner), so a destination reachable straight across from another part of the edge is missed.
- Evidence: mzrouter/mzEstimate.c:1080-1200
- Options: replicate | fix (search the whole edge)
- Recommendation: replicate; estimates steer the search order.

### Q-LEGACY-97: Plowed subcells use a truncated "unset move" sentinel
- Kind: bug
- Magic behavior: A use's plow move starts as the 64-bit client sentinel `-(2^62 - 4)`. When a cell edge is processed, the move is compared after truncation to `int`, which on 64-bit hosts yields 4, so a cell moves only if its required move exceeds 4 the first time. The "find cells" rule (paint pushing or dragging cells) and the cell rule never queue a use whose move is still the sentinel, so paint never pushes a cell that has not already been moved by the plow itself. Write-back skips uses whose move is the sentinel or 0.
- Evidence: plow/PlowMain.c:1548, plow/PlowRules2.c:1089-1091, plow/PlowYank.c:462, database/DBcellname.c:2095, tiles/tile.h:171-172, utils/magic.h:81
- Options: replicate (cells move only when move > 4 initially; paint never pushes unmoved cells) | fix (unset = "not moved": any positive move counts, paint pushes cells)
- Recommendation: capture LEG-PL-20 first; if confirmed, fix (treat unset as 0) and record the deviation, since the behavior depends on host word size.

### Q-LEGACY-98: Box plow clamps edges to box width + 1
- Kind: quirk
- Magic behavior: The plow rectangle is widened by 1 to the left to catch edges under the plow's left side, and the queue's per-edge distance clamp is taken from the widened rectangle, so an edge may move up to (box width + 1) instead of the box width.
- Evidence: plow/PlowMain.c:690-694, plow/PlowMain.c:751, plow/PlowQueue.c:170-180
- Options: replicate | fix (clamp to box width)
- Recommendation: replicate; it only matters when the layout already violates rules.

### Q-LEGACY-99: Boundary overshoot for edges left of the boundary
- Kind: bug
- Magic behavior: For a moving edge whose original x is left of the boundary, the recorded overshoot is `max(newx, boundary.xbot) - x`, which is positive (at least the distance to the boundary) even when the edge never reaches the boundary. Any such move therefore shrinks the plow and forces another pass, possibly repeatedly.
- Evidence: plow/PlowMain.c:1380-1385
- Options: replicate | fix (`max(0, newx - boundary.xbot)` or "edges left of the boundary may not move")
- Recommendation: capture a golden case (LEG-PL-19) before deciding; document the chosen semantics.

### Q-LEGACY-100: Penumbra-only rules are applied in the umbra
- Kind: quirk
- Magic behavior: Edge rules whose corner types differ from their right types generate an extra rule flagged "penumbra only", but the flag is checked only by the inside-sliver rule; umbra, penumbra and sliver rules apply these rules everywhere, so plowing is more conservative than the DRC rule.
- Evidence: plow/PlowTech.c:536-547, plow/PlowRules3.c:218, plow/PlowMain.c:1700-1735
- Options: replicate | fix (skip penumbra-only rules in the umbra)
- Recommendation: replicate for geometric compatibility.

### Q-LEGACY-101: touching_illegal spacing rule uses the wrong outline set
- Kind: bug
- Magic behavior: For `spacing A B d touching_illegal`, the forward rules (LHS in A) get outline types `T(q) ∩ B` instead of `T(q) ∩ A` (both directions use the complement of the reverse right-hand set), so the penumbra and sliver outlines are traced around the wrong material.
- Evidence: plow/PlowTech.c:380-395
- Options: replicate | fix (use A for the forward rules)
- Recommendation: replicate; compatibility of plowed geometry depends on it.

### Q-LEGACY-102: Inside-sliver rule searches original, not final, coordinates
- Kind: quirk
- Magic behavior: The search for violating material in the inside-sliver rule is named a "final area" search but uses the original tile coordinates, ignoring moves already recorded.
- Evidence: plow/PlowRules3.c:393-401
- Options: replicate | fix (search using trail/lead coordinates)
- Recommendation: replicate.

### Q-LEGACY-103: Jog horizon extends to the horizon on a tile boundary
- Kind: quirk
- Magic behavior: The upward jog-horizon walk stops when the next left tile starts at or beyond the horizon; if a left tile ends exactly at the horizon the edge is extended to the horizon even though the boundary continues straight there, which creates a jog at the horizon. Whether this happens depends on where the canonical tiling splits the left-hand material.
- Evidence: plow/PlowMain.c:1205-1240
- Options: replicate | fix (extend only to real jogs)
- Recommendation: replicate.

### Q-LEGACY-104: Labels are not moved by straightening
- Kind: quirk
- Magic behavior: `straighten` (and automatic straightening after a plow) repaints the changed paint but never moves labels, so labels on straightened material can end up off their paint. Magic's TODO lists "Move labels with straighten".
- Evidence: plow/PlowMain.c:332-436, plow/TODO
- Options: replicate | fix (apply the plow label rule to straightened tiles)
- Recommendation: fix (UI-visible defect with no data-format impact), recorded as a deviation.

### Q-LEGACY-105: Incremental loading may hide material from the plow
- Kind: ambiguity
- Magic behavior: Plowing works on a lazily grown copy of the edit cell. Growth is triggered by heuristic margins (1 unit around rule areas, H around edges); searches that reach beyond the loaded area without triggering growth see space. The real-width search extends to the cell's bounding box and is retried after growth.
- Evidence: plow/PlowYank.c:107-220, plow/PlowMain.c:1745-1790
- Options: replicate the loading heuristics exactly | load the whole cell
- Recommendation: load the whole cell, and use golden cases with distant material (LEG-PL-21) to detect any divergence.
