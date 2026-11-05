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
