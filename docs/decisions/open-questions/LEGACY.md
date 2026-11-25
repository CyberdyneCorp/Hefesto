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
- Resolution: fix — compute the terminal-contact clearance flag as the AND of both layer checks and treat an unassigned flag as "not OK", instead of reading an uninitialised value (X-2 tier 1, 2026-11-07).

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
- Resolution: n/a — spec follows Magic's code; documentation is wrong (X-2 tier 3, 2026-11-07).

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
- Resolution: n/a — spec follows Magic's code; documentation is wrong (X-2 tier 3, 2026-11-07).

### Q-LEGACY-35: Spacing listings include undefined type slots
- Kind: bug
- Magic behavior: Spacings are derived for every type index up to the maximum type count; unused indexes get spacing 0, so `iroute spacings` and `saveParameters` print entries for them with a null type name (`(null)` on common C libraries).
- Evidence: mzrouter/mzTech.c:180-186, irouter/irCommand.c:1890-1900, irouter/irCommand.c:2420-2432
- Options: replicate | fix (only defined types)
- Recommendation: fix; emit only defined tile types.
- Resolution: fix — emit spacing entries only for defined tile types, instead of passing a null type name to the output (X-2 tier 1, 2026-11-07).

### Q-LEGACY-36: Displaying a boolean parameter reports an error
- Kind: bug
- Magic behavior: The boolean parameter helper reports `Unrecognized boolean value: "(null)"` whenever it is called only to display a value (no new value), e.g. in `iroute layers`, `iroute wizard estimate` and `saveParameters`.
- Evidence: utils/set.c (SetNoisyBool: result initialised to -2 and checked even when no value given)
- Options: replicate | fix
- Recommendation: fix (suppress the message when no value was supplied).
- Resolution: fix — print no error when the boolean helper is called only to display a value, instead of formatting a null value (X-2 tier 1, 2026-11-07).

### Q-LEGACY-37: The supported PDKs have no `irouter` style
- Kind: platform
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan, `docs/decisions/m10-plot-route-decision-table.md`)
- Magic behavior: `iroute` needs a maze-router style named `irouter`. The Magic technology files of sky130A, gf180mcuD and ihp-sg13g2 at the pinned commits all have an empty `mzrouter` section, so every `iroute` and every `mazeroute` reports "Need irouter style in mzrouter section of technology file to use irouter." on all three PDKs. The M11 agent's routing tools wrap `iroute` (`agent-schematic-driven-layout`), so without a style they cannot route on any supported PDK.
- Evidence: irouter/irMain.c:136-160 (IRAfterTech, `MZFindStyle("irouter")`), irouter/irCommand.c:2580-2585; sky130A.tech:704-705, gf180mcuD.tech (empty `mzrouter … end`), ihp-sg13g2.tech:643
- Options: (a) none: both modes report Magic's error, and users or PDKs add a style | (b) native only: with `magic-compat` OFF and no `irouter` style, Hefesto derives one from the technology; with it ON Magic's error | (c) Hefesto ships a hand-written `mzrouter` overlay per PDK, read in both modes | (d) the derived style of (b) in both modes
- Recommendation: (b). With `magic-compat` OFF and no style named `irouter`, Hefesto derives one when the technology is loaded: route layers are the types of the `lef` section's `routing` lines in order (bottom first), each active with hCost/vCost 32/64 on even positions and 64/32 on odd ones (scmos's `irouter` costs), jogCost 256, hintCost 1, overCost 1; contacts are the types of the `cut` lines whose two residues are both derived route layers, cost 1024; widths and spacings come from DRC exactly as for a written style (`router-maze` "Width and spacing derivation from DRC"). A technology with fewer than one `routing` line gets no style and keeps Magic's error. The first `iroute` of a session prints `ROUTE-STYLE-001` naming the derived style once. With `magic-compat` ON nothing is derived (golden behaviour on the PDKs is Magic's error). A style named `irouter` in the technology always wins. This makes `iroute` work on all three PDKs for the agent (whose scratch sessions run with `magic-compat` OFF unless the user chooses otherwise) without data that has to track PDK releases.

### Q-LEGACY-38: Start points on both sides of a fence leave undo disabled
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: `MZAddStart` disables undo while it registers a start, and on the path that prints "Start points on both sides of fence. Arbitrarily choosing those %s fence." it returns without re-enabling it. Undo disabling is a counter, so undo stays off for the rest of the session: every later edit is unrecorded.
- Evidence: mzrouter/mzMain.c:590-620 (the early return at 609-615), utils/undo.c:409-436 (UndoDisable/UndoEnable count)
- Options: replicate | fix (re-enable undo on every path)
- Recommendation: fix in both modes (X-2 tier 1: corrupted editor state). The message and the start choice are Magic's; undo is restored on every path, including errors and cancellation. A unit test routes across a fence and then undoes an unrelated edit.

### Q-LEGACY-39: Malformed `mzrouter` statements leave half-built route types
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: (1) a bare `style` line copies `argv[1]`, which is NULL. (2) A `layer` line that fails a check after its route type was created (a duplicate plane, a type on two planes, a non-positive cost) leaves the half-filled route layer in the style's route-type list, as Q-LEGACY-31 describes for `contact`. (3) Copying a style for `iroute` (`MZCopyParms`) keeps a pointer it cannot translate shared with the original, so the copy can reach freed memory after a reload.
- Evidence: mzrouter/mzTech.c:418 (bare `style`), :586-740 (`layer` checks after `mzInitRouteType`), :1136-1215 (`contact`), mzrouter/mzMain.c:243-251 (`ADDR_TBL` keeps untranslated pointers), mzrouter/mzStart.c:337-355 (dereference of the other layer)
- Options: replicate (not possible: crashes) | fix (validate the whole line before registering anything)
- Recommendation: fix in both modes (X-2 tier 1), together with Q-LEGACY-31. Every `mzrouter` statement is validated in full before anything is registered; a failing statement reports Magic's message and is ignored whole. A bare `style` reports `"style" lines need a style name` and the lines up to the next `style` are skipped. Hefesto's styles are values, so copying one shares nothing.

### Q-LEGACY-40: Choosing among several endpoint nodes when the session cannot ask
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: when several route layers or contacts touch the start point (or a destination point), `irChooseEndPtLayers` prints "Multiple nodes present at %s point:" and asks "Connect to %s? [yes] " for each candidate with `TxGetLine`; an empty answer is yes and the last candidate is taken without asking. In a batch run there is no console line to read, so the command blocks or reads whatever stdin holds. An agent session has no console at all.
- Evidence: irouter/irRoute.c:918-1192 (the prompts at 1114 and 1154)
- Options: replicate (block or read stdin) | fix (when the session cannot ask, take the default answer)
- Recommendation: fix in both modes (X-2 tier 1: a hang). A session that can ask (the desktop console, an interactive terminal) prompts as Magic does. A session that cannot ask (a script host, `-dnull` batch, an agent scratch session) prints the same lines and takes the empty answer, yes, so the first candidate is connected, as Q-SCRIPT-48 treats confirmations that cannot be asked. LEG-MZ-28 captures Magic in batch with stdin at end of file, under a `timeout -s KILL 120` guard so a blocked Magic cannot stall the capture job. If Magic also takes the first candidate there, this row is replicate; if it connects another candidate, fails, or hangs (killed by the guard, exit status 137), the fix stands as written.

### Q-LEGACY-41: `iroute route -timeout` uses wall-clock time and can stay armed
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: `-timeout n` arms a SIGALRM timer of n seconds that interrupts the search, which then ends as "best before interrupt" or "interrupted". A parse error after `-timeout` jumps past the code that clears the timer, so a later command can be interrupted by it. The result of a timed route depends on machine speed, so `mazeroute` (which always passes `-timeout 3`) can route differently on different machines.
- Evidence: irouter/irCommand.c:1683-1696 and the `leaveClean` path, utils/signals.c:145-198, tcltk/mazeroute.tcl:11-33
- Options: replicate | fix the leak only | fix the leak and add a second, interrupt-style work budget to the engine
- Recommendation: fix the leak only, in both modes (X-2 tier 1: an interrupt of an unrelated command). `-timeout` keeps Magic's syntax, messages and meaning: a wall-clock deadline that ends the search like an interrupt; the deadline lives in the route's cancellation token and dies with the command on every path. No new budget is added: Magic already has a deterministic one, the style's `bloomLimit` (`iroute wizard bloomLimit`, irouter/irCommand.c:584-589, :2242), which stops the search quietly once the bloom count exceeds the limit ("Bloom limit (%d) hit." at verbosity ≥ 1) and keeps the cheapest complete path found (result success, else failure; mzrouter/mzSearch.c:208, :409-418, :688-706). The engine honours it as Magic does, the agent sets it in its request's style copy, and golden cases that need a limit run `iroute wizard bloomLimit N` in both Magic and Hefesto. SCRIPT-AUX-02 replays `mazeroute` with Magic's `-timeout 3`; its routes take milliseconds in both tools, so the clock does not decide them.

### Q-LEGACY-42: An already-routed endpoint prints "Route Failed."
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: when the start already connects to the destination, `MZRoute` returns no path with the code "already routed"; `irRoute` sees no path and prints "Route Failed." (an error, shown even at verbosity 0), while the script result is "Route already routed".
- Evidence: mzrouter/mzMain.c:724-875 (`MZ_ALREADY_ROUTED`), irouter/irRoute.c:360-470
- Options: replicate | compat (ON as Magic; OFF prints an accurate message)
- Recommendation: compat (standing rule: printed output differs). With `magic-compat` ON, "Route Failed." is printed as Magic; with it OFF, "Already routed." is printed as information (suppressed at verbosity 0). The script result is "Route already routed" in both modes.

### Q-LEGACY-43: `saveParameters` writes spacings in internal units that `source` reads as lambda
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: `iroute saveParameters` writes each spacing as the internal-unit integer, while `iroute spacings` parses values with the standard coordinate parser, which reads a bare number in the current units (lambda by default). After `scalegrid`, or with `units` set to something else, sourcing the file sets different spacings from the ones saved.
- Evidence: irouter/irCommand.c:2420-2432 (write), :1856-2120 (`cmdParseCoord` on read)
- Options: replicate | compat (ON as Magic; OFF write the value with the `i` suffix, so it reads back in internal units under any `units` setting)
- Recommendation: compat (standing rule: the file is a compatibility surface). With `magic-compat` ON the bare integer is written, as Magic; with it OFF the value carries the `i` suffix (`:iroute spacings metal2 metal2 4i`), as `feedback save` does (Q-UI-24). Both read back unchanged in Hefesto when internal units equal lambda.

### Q-LEGACY-44: Sourcing a `saveParameters` file in a Tcl session
- Kind: ambiguity
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: every command line of the file starts with `:` (`:iroute verbosity 0`). Magic's own command reader strips a leading `:`, so Magic's `source` of the file works; whether a Tcl-level `source` (Magic's Tcl build, Hefesto's scripting layer) accepts a command named `:iroute` is not visible from the C source and was not run.
- Evidence: textio/txCommands.c:1414-1416 (leading `:` stripped), irouter/irCommand.c:2356-2500
- Options: verify (LEG-MZ-11 captures `source p.ir` in Magic's Tcl build)
- Recommendation: verify: if the capture shows the parameters restored, Hefesto's `source` accepts a leading `:` on a command line in both modes (replicate); if Magic's Tcl `source` fails, replicate the failure with `magic-compat` ON and accept the `:` with it OFF (compat). The file content stays Magic's in both cases.

### Q-LEGACY-45: `iroute route` coordinates reject unit suffixes and `-dRect` is not normalised
- Kind: quirk
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: each `-sPoint` and `-dRect` value must pass a plain-number test before the coordinate parser sees it, so `10um` or `5i` is rejected ("Nonnumeric coordinate: %s"; the first `-dRect` field says "Non coordinate: %s"), although every other command accepts suffixed coordinates. `-dRect` takes the corners as given, so `-dRect 10 10 0 0` gives an inverted destination rectangle.
- Evidence: irouter/irCommand.c:1580-1650 (`StrIsNumeric` before `cmdParseCoord`), irouter/irRoute.c:629-750
- Options: replicate | compat (ON as Magic; OFF accept every coordinate form and normalise the rectangle)
- Recommendation: compat (standing rule: the command's result differs). With `magic-compat` ON the values are rejected as Magic and the rectangle is used as given; with it OFF any value the standard coordinate parser accepts is taken and `-dRect` is normalised (lower-left, upper-right). The agent's typed wrapper always passes plain internal-unit integers, so it behaves the same in both modes.

### Q-LEGACY-46: `iroute` parameter and message quirks
- Kind: quirk
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: (1) values given to `iroute layers`/`contacts` with `*` wrap around the list, then warn. (2) `iroute spacings` stops at a value below −1 but skips other bad values, so a set can apply in part; an odd argument count prints a usage line naming `*iroute spacing`. (3) `iroute layers <type>` reports a non-route type with the contact wording, and the error for an unknown layer says "route layer or contact". (4) `iroute help route` omits `-timeout`. (5) Repeated `-sLayers`/`-dLayers` append duplicates. (6) The comment of `irSetNoisyAutoInt` promises a `(FILE*)1` mode that only the layer and contact setters have. (7) The `-list` option of `layers`/`contacts` is honoured only by the all-types form.
- Evidence: irouter/irCommand.c:795-799 (NEXTVALUE), :1856-2120 (spacings), :1177-1428 (layers), :1072-1130 (help), :1500-1560 (layer lists), :99-196 (irSetNoisyAutoInt)
- Options: replicate | compat | fix
- Recommendation: replicate in both modes. These are Magic's texts and parameter semantics that scripts and saved files depend on; none is a crash or corrupt output, and duplicates in a layer list do not change the route. (6) has no observable effect.

### Q-LEGACY-47: The maze-router penalty is unchecked
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: the penalty is stored as an integer mantissa floor(p × 2) with exponent 1 (Q-LEGACY-32). `iroute wizard penalty` has no range check: a negative penalty makes the adjusted cost grow as paths approach the destination, and a value whose mantissa does not fit in an `int` is an undefined float-to-int conversion. The technology statement `search <rate> <width> <penalty>` rejects negative values ("Bad penalty value") but makes the same unchecked `(int)(value * 2)` conversion for large ones.
- Evidence: irouter/irCommand.c:620-656, mzrouter/mzTech.c:987-1008, mzrouter/mzSearch.c:519-524
- Options: replicate | fix the overflow only | fix both
- Recommendation: fix the overflow in both modes (X-2 tier 1), at both entry points: a value with |p × 2| ≥ 2^31 is rejected with "Penalty value out of range: %s" and the penalty is unchanged (for `search`, the whole statement defines nothing, by the whole-statement validation of Q-LEGACY-31 and -39). Replicate negative values for `wizard penalty` in both modes (they are legal in Magic and only change the search order; the adjusted-cost arithmetic is saturating, Q-LEGACY-90); the `search` statement keeps Magic's rejection of negative values.

### Q-LEGACY-48: Maze search memory is unbounded
- Kind: platform
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: the search allocates path records, hash entries and blockage tiles until it finishes or the user interrupts; with `bloomLimit 0` (the default) a long route over a large, congested area can exhaust memory and abort Magic.
- Evidence: mzrouter/mzSearch.c:182-720, mzrouter/mzMain.c:724-875, mzrouter/mzBlock.c
- Options: (a) a memory budget per route that ends the search like an interrupt | (b) no limit, as Magic
- Recommendation: (a), in both modes. A route whose search memory exceeds the budget (setting `router.max-search-memory`, default 1 GiB; 256 MiB on iPadOS) ends as an interrupt does ("Search Interrupted!" with the best complete path, otherwise "Route Interrupted!"), and prints `ROUTE-MZ-002` naming the budget. Golden routes stay far below it.

### Q-LEGACY-49: The maze helper routes a net with fewer than two pins
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: the maze helper's Route action (`routenet` in `mazeroute.tcl`) routes each selected net as a chain over its pins. For a net with one pin the chain loop does not run, so the variable that holds the route result is never set, and the next test stops the helper with Tcl's "can't read "rresult": no such variable" (if an earlier net set it, that net's result is reused instead). `scripting-auxiliary-tools` "Netlist maze-routing script" says the helper is reproduced but does not mention this case.
- Evidence: tcltk/mazeroute.tcl:477-541 (the loop at 500-517, the test at 520)
- Options: replicate (stop with the error) | fix (a net with fewer than two pins is left in the unrouted list, with a note in the console, and the next selected net is routed)
- Recommendation: fix in both modes. A Tcl error that aborts the action, or a result carried over from another net, is a defect of the helper with no behaviour to keep. The `mazeroute` command (the golden surface, SCRIPT-AUX-02) is not affected: its chain over one terminal routes nothing, as Magic's. The m10-plot-route change carries the rule as a MODIFIED delta of `scripting-auxiliary-tools` "Netlist maze-routing script", synced only if this row is approved.

### Q-LEGACY-50: Plowing of non-Manhattan (split) tiles
- Kind: ambiguity
- Magic behavior: The plow algorithm works on corner-stitched edges and was written before split (diagonal) tiles existed; its handling of split tiles is not specified by Magic and is untested.
- Evidence: plow/PlowMain.c (no split-tile handling), plow/TODO
- Options: replicate whatever Magic does (requires golden capture) | refuse to plow areas with non-Manhattan geometry | treat diagonals by bounding tiles
- Recommendation: capture golden cases first (LEG-PL-08); until then refuse with a clear message.

### Q-LEGACY-51: Initial wiring type is not "none"
- Kind: bug
- Magic behavior: The current wiring type is initialised to the type number just below the first selectable type (an internal DRC type), while the "no wiring material selected" check tests for type 0, so `wire leg` before any `wire type` does not report the error and attempts to paint that internal type.
- Evidence: wiring/wireOps.c:48, wiring/wireOps.c:328, wiring/wireOps.c:574, database/database.h.in:130-136
- Options: replicate | fix (initialise to "none" and report the error)
- Recommendation: fix.
- Resolution: fix — evidence corrected (the initialiser is wiring/wireOps.c:48, `TT_SELECTBASE-1` = 5 = `TT_ERROR_PS`, and the type-0 checks are at wiring/wireOps.c:328 and :574), so before any `wire type` Magic tries to paint a DRC error type into the edit cell, a meaningless result that no script can rely on; Hefesto starts with no wiring type and reports `no wiring material selected` (verified against wiring/wireOps.c:WireAddLeg on 2026-11-09; approved 2026-11-09).

### Q-LEGACY-52: Wiring operations dereference a missing window
- Kind: bug
- Magic behavior: After painting a leg or contact, the code looks up a window showing the edit cell's root and dereferences it without checking for NULL (crash when no such window exists, e.g. headless scripting).
- Evidence: wiring/wireOps.c:385-392, wiring/wireOps.c:845-850
- Options: replicate (crash) | fix (skip selection when no window)
- Recommendation: fix; also required for headless/iPadOS operation.
- Resolution: fix — skip the redisplay when no window shows the edit cell's root, instead of dereferencing a missing window (X-2 tier 1, 2026-11-07).

### Q-LEGACY-53: wire segment usage message lists coordinates in the wrong order
- Kind: doc-mismatch
- Magic behavior: The usage message says `x1 x2 y1 y2` but the arguments are parsed as `x1 y1 x2 y2 ...`.
- Evidence: commands/CmdTZ.c:2036-2037 (usage text), commands/CmdTZ.c:1718 (help table, correct order)
- Options: replicate text | fix text
- Recommendation: fix the message (informative text).
- Resolution: fix — evidence corrected (the usage text is at commands/CmdTZ.c:2036-2037; the `wire help` table at commands/CmdTZ.c:1718 already says `x1 y1 x2 y2`), and the message is informative text that no script parses (verified against commands/CmdTZ.c:CmdWire on 2026-11-09; approved 2026-11-09).

### Q-LEGACY-54: The maze helper's parameter window cannot open
- Kind: bug
- Status: pending user decision (raised 2026-11-24 by the m10-plot-route plan)
- Magic behavior: the maze helper's Params button runs `genmazeparams`, which fills each route-layer row from `$contact`, a variable not yet set in that loop (Tcl error), and creates its Active cells with `checkbox`, which is not a Tk widget (`checkbutton` is). The window therefore fails before it shows anything, and it has no apply action even if it opened. `scripting-auxiliary-tools` "Netlist maze-routing script" lists the helper's lists and actions but not this window.
- Evidence: tcltk/mazeroute.tcl:65 (the button), :101-145 (`genmazeparams`: `$contact` at 129, `checkbox` at 127 and 140)
- Options: (a) not provided: the helper has no Params button; the route parameters stay reachable through `iroute layers`, `iroute contacts`, `iroute spacings` and `iroute wizard` | (b) a working parameter panel showing `iroute layers -list` and `iroute contacts -list` and applying edits through those commands
- Recommendation: (a). Magic's window never worked, so there is no behaviour to reproduce, and a new parameter editor is native UI that belongs with the M11 routing UI if it is wanted. The m10-plot-route delta of "Netlist maze-routing script" states that the helper has no parameter window.

### Q-LEGACY-70: Netlist write order depends on platform word size and char signedness
- Kind: quirk
- Magic behavior: Net and terminal order in written `.net` files (and the routers' net order) follows Magic's string hash table, whose hash uses `unsigned long` arithmetic (64-bit on LP64 hosts, 32-bit elsewhere) and plain `char` (signed on x86, unsigned on ARM) for non-ASCII bytes; the same netlist can therefore be written in different orders on different builds.
- Evidence: utils/hash.c:218-262, netmenu/NMnetlist.c:470-575, utils/netlist.c:60-80
- Options: replicate the LP64/signed-char variant exactly | write nets in a stable order (e.g. file order)
- Recommendation: replicate the 64-bit, signed-char behaviour (the common Linux x86-64 build) so golden files match; document that order is otherwise not semantically significant.
- Resolution: compat — the `.net` netlist order is name-keyed hash order and is reproduced exactly with Magic's string hash table and signed key bytes on every platform (X-1 "Name-keyed hash-table order", as amended by D-1, 2026-11-07).

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
- Resolution: n/a — spec follows Magic's code; documentation is wrong (X-2 tier 3, 2026-11-07).

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
- Resolution: n/a — spec follows Magic's code; the source comment is wrong (X-2 tier 3, 2026-11-07).

### Q-LEGACY-80: Density-blockage scan reads one entry past the density vector
- Kind: bug
- Magic behavior: The run-extension loop reads density[hi] before checking hi < size, reading one element past the end when a saturated run reaches the last column/row. The bounds check still stops the loop, so geometry is unaffected unless the read faults.
- Evidence: grouter/grouteChan.c:667-760 (glChanBlockDens)
- Options: fix (check the bound first)
- Recommendation: fix; no observable change.
- Resolution: fix — check the index bound before reading the density vector; geometry is unchanged (X-2 tier 1, 2026-11-07).

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
- Resolution: fix — use saturating 64-bit arithmetic for edge weights, dominance tests and the adjusted-cost key; results are identical for realistic layouts (X-2 tier 1, 2026-11-07).

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
