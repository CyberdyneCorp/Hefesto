# M10 decision table: plotting and maze routing

The M10 plotting and routing workstreams (`openspec/changes/m10-plot-route`: `plot-output`, `router-maze`, `router-maze-iroute`) cannot start until every open question their specs cite has a `Resolution:` line. This table lists what has to be decided:

| Group | Count |
|---|---|
| Entries reported as blocking by `python3 scripts/check_milestone_readiness.py --milestone M10` for these three capabilities (4 Q-MISC, 15 Q-LEGACY) | 19 |
| Unresolved plot entries not cited by the spec (Q-MISC-45, Q-MISC-47) | 2 |
| Audit decisions marked "blocks M10" in `docs/decisions/audit/DECISIONS.md` that this change settles | 2 (D-37, D-38) |
| New entries raised by this plan: Q-MISC-48 to -55, Q-LEGACY-37 to -49, Q-LEGACY-54 | 22 |
| Judgement items (J1–J6) | 6 |
| Planning questions from the design's "Open Questions" | 3 |

**Status: approved 2026-11-25.** Drafted 2026-11-24; the user approved every row of this table, the judgement items J1–J6 and the three planning questions with the recommended option on 2026-11-25 (the narrower wording of Q-LEGACY-41, "the timeout leak only", and of Q-LEGACY-47, "the wizard penalty and the search statement", is the recommendation's own scope). DECIDE recorded the answers in round R0 (tasks 1.1–1.3): each entry has `Status: approved 2026-11-25` and a `Resolution:` line in `docs/decisions/open-questions/MISC.md` or `LEGACY.md`, D-37 and D-38 are resolved in `docs/decisions/audit/DECISIONS.md`, the **Status** column of each table below gives the decision, and the spec deltas are synced into the living specs (section "Spec changes"). `check_milestone_readiness.py --milestone M10` reports no entry for these three capabilities; the other M10 entries it reports (Q-DIGITAL, Q-RELIABILITY) belong to later M10 changes and are not decided here.

**Verify rows at G0.** Q-LEGACY-40 and Q-LEGACY-44 are settled by captures (LEG-MZ-28, LEG-MZ-11). They are approved at G0 as rules with conditional `Resolution:` lines, which the readiness check counts as resolved; task 2.7 adds each `Verified` line with the run id. An outcome that no branch of the approved rule covers goes back to the user before G1-ROUTE. Both rules were approved on 2026-11-25 and recorded as conditional `Resolution:` lines naming the option for each outcome: Q-LEGACY-40 (LEG-MZ-28) — first candidate connected: replicate; another candidate connected, command failed, or hang killed by the guard: fix in both modes. Q-LEGACY-44 (LEG-MZ-11) — Magic fails on the leading `:`: compat, with `script::source_line_prefix` accepting `:` only with `magic-compat` OFF; Magic restores the parameters: the `:` is accepted in both modes.

## Method and sources

Magic 8.3.694 at fc5daa9e (read-only; `git diff fc5daa9e HEAD -- plot irouter mzrouter tcltk commands` in the reference checkout is empty) was read for `plot/`, `irouter/`, `mzrouter/`, `magic/magicps.pro`, `tcltk/mazeroute.tcl`, the build switches (`scripts/configure.in` and the device defines in `plot/plotInt.h`) and the dispatch tables. The plot device set was re-checked on 2026-11-25: `plotInt.h:28` defines `VERSATEC` unconditionally, so the default build has `plot versatec` and the HP/Versatec parameters (Q-MISC-51); the comments that say otherwise are stale. Nothing was run; rows that depend on running Magic are verify rows. The PDK technology files at the pinned commits were read for their `mzrouter`, `plot` and `lef` sections. No xschem, netgen or ngspice source is involved.

Evidence tags:
- **[E]**: the Hefesto entry in `docs/decisions/open-questions/`;
- **[S]**: a Hefesto capability spec;
- **[D]**: a Hefesto decision (`product.md`, `cross-cutting.md`, `audit/DECISIONS.md`, `roadmap.md`);
- **[code]**: Hefesto at origin/main 54fbc6d4;
- **[magic]**: Magic 8.3.694 at fc5daa9e;
- **[pdk]**: the pinned PDK trees.

## Recommendation kinds

The **Kind** column is the kind of the recommendation, as X-2 (`docs/decisions/cross-cutting.md`) defines it; it is not the entry's own `- Kind:` line.
- **replicate**: do what Magic does, in both modes.
- **compat**: replicate with `magic-compat` ON, and use the intended behaviour with it OFF.
- **fix**: corrected behaviour in both modes. This covers tier 1 (crashes, undefined behaviour, hangs, invalid or truncated output).
- **platform**: a Hefesto platform decision (X-3, X-4).
- **verify**: settled by a capture in the golden image (X-6).
- **n/a**: the behaviour does not exist in Hefesto.
- **other**: a scope or process decision.

**Standing rule applied.** Where an existing entry recommends "replicate" for a Magic bug that changes a result (Q-MISC-41, Q-MISC-43, Q-LEGACY-33, -85, -87, -89), this table recommends **compat** instead: `magic-compat` ON copies Magic, OFF does the intended thing (J5). Quirks with no clear intended behaviour stay replicate.

## Judgement items

- **J1 (D-37, D-38), who owns view export and how PNG is drawn.** (a) *Recommended:* `hefesto-rendering` owns raster and vector export of a view through a new requirement "View export to SVG and PNG", implemented in this change as backend-free core code from the scene model (`display/export/`), with PNG drawn by a CPU scan-converter and encoded through the zlib already built. `plot svg` uses it; the desktop export and the reliability heat maps use the PNG part; `reliability-visualization` is re-pointed to it. (b) A native PNG format in `plot-output`, and SVG in `plot-output` too (D-37's audit option (b)). (c) Settle ownership now but defer PNG to the reliability change. (a) closes both audit items with one module and leaves no capability promising an export nobody provides. On the drawing method, D-37's audit option (a) is "an offscreen render" (a GPU readback); the CPU scan-converter is a different design, listed as D-37 (c) below for the user to choose.
  **Decision (approved 2026-11-25):** (a): `hefesto-rendering` owns SVG and PNG view export, PNG drawn by a CPU scan-converter (D-37 option (c)).
- **J2 (Q-LEGACY-37), an `irouter` style for PDKs that have none.** The letters are Q-LEGACY-37's. (a) No style; users or PDKs add one. (b) *Recommended:* with `magic-compat` OFF only, derive one from the technology's `lef` section (`routing` lines as route layers with scmos's alternating costs, `cut` lines as contacts, widths and spacings from DRC). (c) Ship hand-written per-PDK overlays. (d) Derive in both modes. (b) makes `iroute` and the M11 agent work on sky130A, gf180mcuD and ihp-sg13g2 while ON still answers as Magic.
  **Decision (approved 2026-11-25):** (b): with `magic-compat` OFF only, derive an `irouter` style from the technology's `lef` section (Q-LEGACY-37).
- **J3 (Q-MISC-52), the SVG reference.** (a) *Recommended:* Hefesto's own SVG from the scene; a `magic-cairo` capture variant captures MISC-PLOT-12 (compared on name, size and well-formedness) and MISC-PLOT-13 (`plot help`). (b) No Cairo variant; MISC-PLOT-12 is Hefesto-only. (c) Imitate Cairo's output.
  **Decision (approved 2026-11-25):** (a): Hefesto writes its own SVG; the `magic-cairo` variant captures MISC-PLOT-12 and -13, compared on name and numeric size at 512 × 484; `plot svg` with `magic-compat` ON is a documented deviation (Q-MISC-52).
- **J4, the router engine interface for M11.** (a) *Recommended:* a pure call `route::maze::route(RouteRequest) -> RouteResult` with no global state, a structured result (code, path, paint, cost, statistics, end reason), a cancellation token and a memory limit, with the style's `bloomLimit` (Magic's own parameter and stop rule) as the deterministic bound; painting, undo and selection stay with the caller. (b) Port Magic's global-state API (`MZInitRoute`/`MZAddStart`/`MZRoute`/`MZClean`) and wrap it per session later. (a) lets several agent scratch sessions route in parallel, and a route with a bloom limit gives the same result on every machine.
  **Decision (approved 2026-11-25):** (a): a session-scoped engine (a pure call, no global state) with a structured result; the style's `bloomLimit` is the deterministic bound.
- **J5, Magic bugs that change plots and routes.** (a) *Recommended:* compat for Q-MISC-41, Q-MISC-43, Q-LEGACY-33, -85, -87 and -89 (one branch point each, design D5). (b) Replicate in both modes, as the entries originally recommended: one code path, but OFF would copy known bugs.
  **Decision (approved 2026-11-25):** (a): compat for Q-MISC-41, Q-MISC-43, Q-LEGACY-33, -85, -87 and -89 (`magic-compat` ON copies Magic, OFF fixes).
- **J6, the maze helper window of `mazeroute.tcl`.** (a) *Recommended:* a native desktop panel from Options > Maze Router (unrouted and routed nets, route, rip up, verify, save and load failed nets), not on iPadOS. (b) Port only the `mazeroute` command and defer the helper to M12 with `netlist-window`, which needs a MODIFIED delta of the `scripting-auxiliary-tools` requirement.
  **Decision (approved 2026-11-25):** (a): a native desktop panel from Options > Maze Router, not on iPadOS.

## Decision table: the blocking entries

### plot-output

| ID | Question | Kind | Recommendation | Evidence | Status |
|---|---|---|---|---|---|
| Q-MISC-41 | `mr`/`mx` dropped when the lower-left corner is outside the area | **compat** | ON: Magic's rule (goldens). OFF: written when any part overlaps; `rectclip` trims (J5). | [E]; [magic] plot/plotPS.c:405-420; [S] "PostScript paint output" | approved 2026-11-25: compat |
| Q-MISC-42 | Label and cell text not escaped | **fix** | Escape `(`, `)`, `\` in both modes: Magic's file is invalid PostScript (tier 1). MISC-PLOT-03 becomes a documented deviation. | [E]; [magic] plot/plotPS.c:1027, :1087, :1106 | approved 2026-11-25: fix |
| Q-MISC-43 | Label margins mix relative and absolute coordinates | **compat** | ON: Magic's formula (BoundingBox and scale match the goldens). OFF: relative coordinates throughout (J5). | [E]; [magic] plot/plotPS.c:869-872 | approved 2026-11-25: compat |
| Q-MISC-44 | Spooling, temporary files, library-path resources | **platform** | Embed `magicps.pro` byte for byte; no spooling and no process on any platform. Bundle `mos.7bit.dstyle` and `mos.7bit.std.cmap`, which the PNM `default` and `draw <type>` colours come from when no display style set is loaded (Magic's `GrStyleTable` and colour map; a `-dnull` run loads these two), unless the display assets already carry them; and bundle `mos.7bit.mraster.cmap` and `_dstyle`, which only `color` and `dstyle` lines without a file name read. | [E]; [S] "Platform mechanism of plot output"; [magic] plot/plotPNM.c:1033-1060, :1150-1165, :1261-1279, :1305-1318, graphics/grMain.c:215-216 | approved 2026-11-25: platform |

### router-maze

| ID | Question | Kind | Recommendation | Evidence | Status |
|---|---|---|---|---|---|
| Q-LEGACY-6 | Heap ties depend on the heap algorithm | **replicate** | Port `utils/heap.c` (sift-up, sift-down, lazy ordering). | [E]; [S] "Heap ordering" | approved 2026-11-25: replicate |
| Q-LEGACY-31 | Malformed `contact` line leaves a half-registered contact | **fix** | Validate the whole statement before registering anything; report and ignore it (with Q-LEGACY-39). | [E]; [magic] mzrouter/mzTech.c:1136-1215 | approved 2026-11-25: fix |
| Q-LEGACY-32 | Penalty stored at 0.5 resolution | **replicate** | floor(p × 2)/2; it decides search order. | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-33 | `width` overridden by DRC derivation | **compat** | ON: DRC wins (Magic). OFF: an explicit `width` overrides, like explicit `spacing` (J5). | [E]; [magic] mzrouter/mzMain.c:235-325 | approved 2026-11-25: compat |
| Q-LEGACY-85 | Inverted length test for the initial LR contact | **compat** | ON: Magic's `<=`. OFF: `>`, as the other three sites (J5). | [E]; [magic] mzrouter/mzStart.c:330-370 | approved 2026-11-25: compat |
| Q-LEGACY-86 | Same-node zero-cost rule overwrites the parent's estimated total | **replicate** | It decides which start-node point routes leave from; no clear intended behaviour. | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-87 | Upward extension ignores closer stop points in the free case | **compat** | ON: Magic's unconditional assignment. OFF: nearest candidate, as the other directions (J5). | [E]; [magic] mzrouter/mzXtndUp.c:230-236 | approved 2026-11-25: compat |
| Q-LEGACY-88 | Same-node paint of a contact onto its own route type blocks half | **replicate** | Documented as Magic behaviour. | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-89 | Contact same-node exemption tests the wrong tile | **compat** | ON: Magic's effective rule (as specified). OFF: tests the layer-plane tile (J5). | [E]; [magic] mzrouter/mzBlock.c:380-405 | approved 2026-11-25: compat |
| Q-LEGACY-91 | Contact-to-contact spacing checked within one class | **replicate** | — | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-92 | Start point is the lower-left of the first enumerated tile | **replicate** | With the canonical tile enumeration order of `layout-geometry-model`. | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-93 | Destination contact-walk passes with an invalid argument | **fix** (remove) | Not written: the passes have no effect. | [E] | approved 2026-11-25: fix (removed) |
| Q-LEGACY-94 | Vertical extensions use horizontal-strip contact blockage | **replicate** | — | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-95 | Stale paths still consume blooms | **replicate** | The window position depends on the bloom count. | [E] | approved 2026-11-25: replicate |
| Q-LEGACY-96 | Straight-shot estimators follow corner-neighbour chains only | **replicate** | Estimates steer the search order. | [E] | approved 2026-11-25: replicate |

## Plot entries not cited by the spec

| ID | Question | Kind | Recommendation | Status |
|---|---|---|---|---|
| Q-MISC-45 | colorversatec stipple byte swap | **follows Q-MISC-51** | The behaviour exists in Magic's default build. Under Q-MISC-51 (a) or (b) Hefesto parses no `colorversatec` stipples, so the row is n/a; under (c) it is decided with the Versatec port (recommendation then: fix, as the entry says). | approved 2026-11-25: n/a (Q-MISC-51 (a)) |
| Q-MISC-47 | PNM downsampling message's memory formula | **replicate** | Print Magic's numbers, computed in 64-bit so they match wherever Magic does not overflow; the console comparison then needs no mask. | approved 2026-11-25: replicate |

## Audit decisions D-37 and D-38

| ID | Question | Recommendation | Status |
|---|---|---|---|
| D-37 | Who provides PNG export for reliability heat maps | (c), a new option: `hefesto-rendering`, drawn from the scene model by a CPU scan-converter, delivered in this change (J1). The audit's options are (a) a native PNG export from an offscreen render in `hefesto-rendering` (a GPU readback) and (b) a PNG format in `plot-output`. (c) keeps (a)'s owner but changes the design: it works headless and on iPadOS without a GPU, and its bytes do not depend on the GPU or driver. Choosing (a) instead moves task 8.2 onto the Vulkan offscreen target and drops PNG from the headless CLI. | approved 2026-11-25: option (c) |
| D-38 | Who owns SVG and vector export | `hefesto-rendering`, requirement "View export to SVG and PNG"; `plot svg` in `plot-output` calls it (J1). | approved 2026-11-25: `hefesto-rendering` |

## Entries raised by this plan

| ID | Question | Kind | Recommendation | Status |
|---|---|---|---|---|
| Q-MISC-48 | Zero-area PostScript plot divides by zero | **fix** | Refuse with "Nothing to plot: the plotted area has zero width or height."; write nothing. | approved 2026-11-25: fix |
| Q-MISC-49 | PNM memory arithmetic overflows and can hang | **fix** | 64-bit sizes, `pnmmaxmem` 1..4 194 304 KB, a strip of at least 2·scale_over_2 + 1 rows, downsampling capped at 2^15, 65 535 pixels per side. | approved 2026-11-25: fix |
| Q-MISC-50 | Malformed `plot` style lines | **fix** / **replicate** | Fix the bare `map` crash, unchecked numeric fields and undefined colour/pattern references (report and ignore); replicate the silent ignoring of long `draw` lines and unknown pnm keywords. | approved 2026-11-25: fix ((1)–(3)); (4) as Magic |
| Q-MISC-51 | Plot devices and parameters Hefesto offers | **platform** | Magic's default build has `postscript`, `pnm` and `versatec` (`plotInt.h:28`), plus `svg` with Cairo and the Tcl wrapper. (a) Hefesto deviates: `postscript`, `pnm`, `svg`, `parameters`, `help`; no `plot versatec`, HP RTL output or HP/Versatec parameters; device tech lines ignored. Golden cost: masked parts of MISC-PLOT-05, -13 and -17. (b) Surface parity: the option and parameters as Magic, `plot versatec` and RTL refused with `PLOT-DEV-001`; no golden masks. (c) Implement Versatec and HP RTL. Recommended: (a). | approved 2026-11-25: platform (a) |
| Q-MISC-52 | SVG content and its golden reference | **platform** | (a) Native SVG from the scene; `magic-cairo` capture variant compared on name and numeric size, both windows set to 512 × 484 (J3). `plot svg` also works with `magic-compat` ON on every display, where Magic needs the Cairo display: an ON-mode deviation. | approved 2026-11-25: platform (a) |
| Q-MISC-53 | Interrupted and failed plots leave partial files | **fix** | Temporary file and rename; an old file of the same name stays unchanged. | approved 2026-11-25: fix |
| Q-MISC-54 | Byte identity across platforms | **platform** | (a) Committed Lanczos table generated in the capture image; `-ffp-contract=off`; correctly rounded float formatting. | approved 2026-11-25: platform (a) |
| Q-MISC-55 | `.pnm` appended only when the whole path has no dot | **compat** | ON: whole path. OFF: the last path component. | approved 2026-11-25: compat |
| Q-LEGACY-37 | The supported PDKs have no `irouter` style | **platform** | (b) Derived from the `lef` section with `magic-compat` OFF only (J2). | approved 2026-11-25: platform (b) |
| Q-LEGACY-38 | Fence start points leave undo disabled for the session | **fix** | Undo restored on every path; message and start choice kept. LEG-MZ-29 is a documented deviation. | approved 2026-11-25: fix |
| Q-LEGACY-39 | Bare `style`, half-built `layer` lines, shared pointers in style copies | **fix** | Whole-statement validation; a bare `style` reports and skips to the next `style`; styles are values. | approved 2026-11-25: fix |
| Q-LEGACY-40 | Several endpoint nodes when the session cannot ask | **verify** / **fix** | Take the empty answer (yes, first candidate) and never block; LEG-MZ-28 shows whether Magic does the same (then replicate). | approved 2026-11-25: conditional on LEG-MZ-28 (replicate if Magic connects the first candidate; otherwise fix); verified 2026-11-26 by LEG-MZ-28 (run 45817717718): Magic connected the first candidate (metal2), exit 0 — **replicate** |
| Q-LEGACY-41 | `-timeout` is wall-clock and can stay armed | **fix** | The deadline lives in the command's cancel token (no leak). No new budget: the style's `bloomLimit` (Magic's quiet stop) is the deterministic bound for the agent, tests and goldens (`iroute wizard bloomLimit N` in both tools). | approved 2026-11-25: fix (the timeout leak only) |
| Q-LEGACY-42 | Already routed prints "Route Failed." | **compat** | ON: `Route Failed.` OFF: `Already routed.` (information). Script result unchanged. | approved 2026-11-25: compat |
| Q-LEGACY-43 | Saved spacings in internal units read back as lambda | **compat** | ON: bare integer. OFF: the `i` suffix (precedent Q-UI-24). | approved 2026-11-25: compat |
| Q-LEGACY-44 | Sourcing a `saveParameters` file in a Tcl session | **verify** | Replicate if Magic's Tcl `source` restores the parameters (LEG-MZ-11); otherwise compat (OFF accepts the leading `:`). | approved 2026-11-25: conditional on LEG-MZ-11 (compat if Magic fails on the leading `:`; otherwise replicate); verified 2026-11-26 by LEG-MZ-11 (run 45817717718): `invalid command name ":iroute"` — **compat** |
| Q-LEGACY-45 | Route coordinates reject unit suffixes; `-dRect` not normalised | **compat** | ON: Magic. OFF: any coordinate form, rectangle normalised. | approved 2026-11-25: compat |
| Q-LEGACY-46 | `iroute` parameter and message quirks | **replicate** | Value cycling, partial spacing sets, Magic's texts and help as they are. | approved 2026-11-25: replicate |
| Q-LEGACY-47 | The maze-router penalty is unchecked | **fix** / **replicate** | Reject values whose mantissa overflows 32 bits, in `iroute wizard penalty` and in the technology `search` statement; keep negative values for `wizard penalty`. | approved 2026-11-25: fix (`wizard penalty` and the `search` statement) |
| Q-LEGACY-48 | Maze search memory is unbounded | **platform** | (a) `router.max-search-memory` (1 GiB desktop, 256 MiB iPadOS), ending as an interrupt. | approved 2026-11-25: platform (a) |
| Q-LEGACY-49 | The maze helper routes a net with fewer than two pins | **fix** | The net stays in the unrouted list with a console note; Magic's helper stops with an unset-variable error. `mazeroute` itself is unchanged. | approved 2026-11-25: fix |
| Q-LEGACY-54 | The maze helper's parameter window cannot open | **other** | (a) Not provided: Magic's `genmazeparams` never opens (undefined variable, a widget Tk lacks); parameters stay on `iroute` subcommands. (b) A new working parameter panel. | approved 2026-11-25: other (a) |

## Planning questions raised by this plan

| # | Question | Recommendation | Status |
|---|---|---|---|
| 1 | How spec changes are recorded | (a) As this change's deltas, synced into the living specs at G0, then `skip_specs: true` (M9's approach). | approved 2026-11-25: (a) |
| 2 | When this change starts relative to the 3D view | (a) R0 now; R1 as soon as G0 passes, in parallel with the 3D view's rounds. No shared module, but shared files (capture image and workflow, `m10_cases.py`, MISC `CASES.md`, core area and registration lists, `EditorState`, `menu_bar_model.cpp`, `perf.yml`, `THIRD_PARTY_NOTICES.md`), edited additively under design D1's merge protocol. | approved 2026-11-25: (a) |
| 3 | Desktop scope | (a) The export dialog and the maze helper panel, with DESIGN boards first. | approved 2026-11-25: (a) |

## Counts

| Kind | Count | IDs (without `Q-`) |
|---|---|---|
| replicate | 11 | LEGACY-6, LEGACY-32, LEGACY-86, LEGACY-88, LEGACY-91, LEGACY-92, LEGACY-94, LEGACY-95, LEGACY-96, LEGACY-46, MISC-47 |
| compat | 10 | MISC-41, MISC-43, MISC-55, LEGACY-33, LEGACY-85, LEGACY-87, LEGACY-89, LEGACY-42, LEGACY-43, LEGACY-45 |
| fix | 12 | MISC-42, MISC-48, MISC-49, MISC-50 (in part), MISC-53, LEGACY-31, LEGACY-38, LEGACY-39, LEGACY-41, LEGACY-47 (in part), LEGACY-49, LEGACY-93 |
| platform | 6 | MISC-44, MISC-51, MISC-52, MISC-54, LEGACY-37, LEGACY-48 |
| verify | 2 | LEGACY-40, LEGACY-44 |
| follows another row | 1 | MISC-45 (Q-MISC-51) |
| other | 1 | LEGACY-54 |

Total: 43 entries (19 blocking, Q-MISC-45, Q-MISC-47 and 22 new), plus D-37, D-38, six judgement items and three planning questions.

## Spec changes the recommendations imply

These were recorded as spec deltas in `openspec/changes/m10-plot-route/specs/` and were applied to the living specs at G0 (task 1.2, 2026-11-26) after the rows were approved; section "Spec changes" lists what was applied:

| Capability | Delta | Rows |
|---|---|---|
| `plot-output` | MODIFIED "Plot command and plotted region", "Plot parameters", "Raster printer and legacy plot devices" | Q-MISC-49, -51, -44, -45 |
| `plot-output` | MODIFIED "PostScript labels" | Q-MISC-42 |
| `plot-output` | MODIFIED "SVG plot" | Q-MISC-52, -55; D-38 |
| `plot-output` | MODIFIED "Platform mechanism of plot output" | Q-MISC-44 |
| `plot-output` | ADDED "Plot behaviour with magic-compat off" | Q-MISC-41, -43, -55 |
| `plot-output` | ADDED "Plot output safety and limits" | Q-MISC-48, -49, -50, -53, -54 |
| `plot-output` | ADDED "Plot export from the desktop" | D-37, Open Question 3 |
| `router-maze` | MODIFIED "mzrouter section grammar" | Q-LEGACY-31, -33, -39 |
| `router-maze` | ADDED "Maze router behaviour with magic-compat off" | Q-LEGACY-33, -85, -87, -89 |
| `router-maze` | ADDED "Maze router engine interface" | J4, Q-LEGACY-41 |
| `router-maze` | ADDED "Maze search safety and limits" | Q-LEGACY-38, -47, -48, -90 |
| `scripting-auxiliary-tools` | MODIFIED "Netlist maze-routing script" (helper one-pin nets, no parameter window) | Q-LEGACY-49, -54 (J6) |
| `router-maze-iroute` | MODIFIED "iroute availability and dispatch" | Q-LEGACY-37 |
| `router-maze-iroute` | MODIFIED "Route context and endpoints" | Q-LEGACY-40 |
| `router-maze-iroute` | ADDED "Derived irouter style" | Q-LEGACY-37 (J2) |
| `router-maze-iroute` | ADDED "Route timeout" | Q-LEGACY-41 |
| `router-maze-iroute` | ADDED "iroute behaviour with magic-compat off" | Q-LEGACY-42, -43, -44, -45 |
| `hefesto-rendering` | ADDED "View export to SVG and PNG" | D-37, D-38 (J1) |
| `hefesto-performance` | ADDED "Plot and maze-route throughput" | — |
| `reliability-visualization` | MODIFIED "Touch and platform parity" (PNG export through `hefesto-rendering`) | D-37 |

Edits that deltas cannot express: the `tech-file-format` note that the `plot` and `mzrouter` sections are now interpreted (no requirement changes; outside DECIDE's files, handed to INTEGRATION as an open item at G0); the M4 deferral closed in `docs/decisions/m4-decision-table.md` when MAZETOOL lands.

## Verification runs needed before G1-ROUTE

| Row | Case | Question the capture answers |
|---|---|---|
| Q-LEGACY-40 | LEG-MZ-28 | Does Magic in a batch run with stdin at end of file connect the first candidate, another one, fail, or hang? The case runs under `timeout -s KILL 120`, so a hang is recorded (exit status 137) instead of stalling the job. |
| Q-LEGACY-44 | LEG-MZ-11 | Does `source p.ir` in Magic's Tcl build restore the parameters despite the leading `:`? |

Both answered by golden-capture run 45817717718 on 2026-11-26; the `Verified` lines are in `docs/decisions/open-questions/LEGACY.md`. Q-LEGACY-40 is replicate (the first candidate), Q-LEGACY-44 is compat.

## Spec changes

Applied at G0 on 2026-11-26 (tasks 1.2 and 1.3). The deltas were first amended to the approved answers, then synced by running `openspec archive m10-plot-route` on a scratch copy of the repository (+11 added, ~11 modified, 0 removed, 7 capabilities; no new capability folder) and copying the seven resulting specs back. `skip_specs: true` is set in `openspec/changes/m10-plot-route/.openspec.yaml`, and the synced `specs/` folder was removed from the change, because `openspec` 1.13 fails `--strict` when `skip_specs: true` sits beside delta files (M9's precedent). A dry-run archive of the branch on a scratch copy changes no spec.

Amendments to the deltas before the sync:

| Capability | Requirement | Edit | Row |
|---|---|---|---|
| `plot-output` | ADDED "Plot behaviour with magic-compat off" | The OFF label overhang makes all four bounds relative to the area origin (llx = trunc(−x/s0) and lly = trunc(−y/s0) join urx and ury; the draft named only urx and ury). | Q-MISC-43 |
| `plot-output` | ADDED "Plot behaviour with magic-compat off" | Each compat point has an ON and an OFF scenario: "Cell outline across the left edge with magic-compat off/on", "Margins away from the origin with magic-compat off/on" (the ON one new), "Dot in a directory name with magic-compat off/on". | Q-MISC-41, -43, -55 |
| `router-maze` | ADDED "Maze router behaviour with magic-compat off" | New ON and OFF scenarios "Initial left-right contact", "Upward extension" and "Contact same-node exemption" (with magic-compat off/on); "Width statement honoured" names Magic for ON. | Q-LEGACY-33, -85, -87, -89 |
| `router-maze` | ADDED "Maze router engine interface" | The Q-AGENT-1 citation is replaced by a reference to capability `agent-schematic-driven-layout`: Q-AGENT-1 is an open M11 entry, and citing it would block the M10 readiness check. | J4 |
| `router-maze-iroute` | ADDED "Derived irouter style" | Q-AGENT-1 citation removed, as above. | J2 |
| `router-maze-iroute` | MODIFIED "Route context and endpoints" | States that the non-blocking prompt rule holds in every approved branch of the conditional rule (replicate if Magic connects the first candidate, fix otherwise). | Q-LEGACY-40 |
| `router-maze-iroute` | ADDED "Route timeout" | States the narrower scope: only the leak is fixed; `-timeout` keeps Magic's syntax, messages and wall-clock meaning, and no other work budget is added. | Q-LEGACY-41 |
| `router-maze-iroute` | ADDED "iroute behaviour with magic-compat off" | The Q-LEGACY-44 point names `script::source_line_prefix` and the ON failure; new scenarios "Saved spacings with magic-compat on", "Already routed with magic-compat off/on" and "Sourcing a saved file with magic-compat off"; existing ones renamed "Suffixed coordinates in both settings" and "Saved spacings after scalegrid with magic-compat off". | Q-LEGACY-42, -43, -44, -45 |
| `plot-output`, `router-maze`, `router-maze-iroute` | the MODIFIED and ADDED requirements with Magic traces | Trace line numbers corrected (section "Magic traces checked"). | task 1.3 |

Q-LEGACY-47's approved scope (`iroute wizard penalty` and the technology `search` statement) is the draft's, so "Maze search safety and limits" needed no edit.

Applied to the living specs (`openspec/specs/`):

| Capability | Added | Modified |
|---|---|---|
| `plot-output` | "Plot behaviour with magic-compat off", "Plot output safety and limits", "Plot export from the desktop" | "Plot command and plotted region", "Plot parameters", "PostScript labels", "SVG plot", "Raster printer and legacy plot devices", "Platform mechanism of plot output" |
| `router-maze` | "Maze router behaviour with magic-compat off", "Maze router engine interface", "Maze search safety and limits" | "mzrouter section grammar" |
| `router-maze-iroute` | "Derived irouter style", "Route timeout", "iroute behaviour with magic-compat off" | "iroute availability and dispatch", "Route context and endpoints" |
| `hefesto-rendering` | "View export to SVG and PNG" (ADDED, so it cannot conflict with the 3D view's edits) | — |
| `hefesto-performance` | "Plot and maze-route throughput" | — |
| `reliability-visualization` | — | "Touch and platform parity" (PNG export through `hefesto-rendering`, D-37) |
| `scripting-auxiliary-tools` | — | "Netlist maze-routing script" (the maze helper as a desktop panel; Q-LEGACY-49, -54) |

After the sync, the task 1.3 trace corrections of requirements the deltas do not touch were made directly in the living specs (next section). `docs/specs-index.md` was regenerated with `scripts/gen_specs_index.py` (169 capabilities, 2036 requirements, 3661 scenarios).

## Magic traces checked

Task 1.3 checked every `(Magic ref: …)` trace of `plot-output`, `router-maze` and `router-maze-iroute` against Magic 8.3.694 at fc5daa9e (the reference checkout's `plot`, `irouter`, `mzrouter`, `utils`, `graphics`, `dbwind`, `database`, `commands` and `magic` directories are unchanged from fc5daa9e). A range is kept when it covers the cited code; it is corrected when it starts or ends in the wrong function or misses the code. Nothing was built or run. File names below are under Magic's source tree.

| Capability | Requirement | Trace before | Trace now |
|---|---|---|---|
| `plot-output` | Plot command and plotted region | plotCmd.c:50-362, CmdSubrs.c:486-590 | plot/plotCmd.c:89-361 (CmdPlot), commands/CmdSubrs.c:485-640 (CmdParseLayers) |
| `plot-output` | Plot parameters | plotMain.c:300-650, plotPS.c:120-140, plotPNM.c:60-75, plotVers.c:95-140 | plot/plotMain.c:280-650, plot/plotPS.c:123-135, plot/plotPNM.c:77-82, plot/plotVers.c:108-142 |
| `plot-output` | Technology plot section structure | plotMain.c:40-200 | plot/plotMain.c:55-260 |
| `plot-output` | PostScript style lines | plotPS.c:160-290 | plot/plotPS.c:175-323 |
| `plot-output` | PostScript file layout | plotPS.c:1110-1334, magicps.pro:1-72 | plot/plotPS.c:1126-1334, magic/magicps.pro:1-72 |
| `plot-output` | PostScript paint output | plotPS.c:290-760, :1265-1280 | plot/plotPS.c:336-747, :1277-1293 |
| `plot-output` | PostScript cell outlines | plotPS.c:1000-1060, :1283-1289, DBcellbox.c:62-104 | plot/plotPS.c:1050-1105, :1295-1302, database/DBcellbox.c:62-107 |
| `plot-output` | PostScript labels | plotPS.c:760-1000, :1150-1160, :1291-1312, DBWdisplay.c:121-122 | plot/plotPS.c:761-1030 (`plotPSLabel`, which writes the text, was outside), :1170-1177, :1304-1324; DBWdisplay.c:121-122 kept |
| `plot-output` | PostScript label margins | plotPS.c:850-930, :1140-1158 | plot/plotPS.c:857-929, :1179-1195 |
| `plot-output` | PNM style lines | plotPNM.c:1088-1300, :1300-1507, :1040-1085 | plot/plotPNM.c:1087-1279, :1300-1507, :1013-1067 |
| `plot-output` | PNM rasterisation and file format | plotPNM.c:120-230, :235-545, :545-1040, plotCmd.c:300-340 | plot/plotPNM.c:147-293, :314-535, :555-996, plot/plotCmd.c:290-338 |
| `plot-output` | SVG plot | grTCairo1.c:354-404, plotCmd.c:265-282 | graphics/grTCairo1.c:354-404 (kept), plot/plotCmd.c:270-287 |
| `plot-output` | Raster printer and legacy plot devices | plotInt.h:28, plotVers.c:241-498, :1063-1490, plotGremln.c:131-217, plotPixels.c:149-183, plotPNM.c:700-760 | plot/plotInt.h:28, plot/plotVers.c:212-498, :1063-1493, plot/plotGremln.c:131-217 (kept), plot/plotPixels.c:149-181, plot/plotPNM.c:745-815, :895-954 (the RTL paths) |
| `plot-output` | Platform mechanism of plot output | plotPS.c:1180-1192, :1221-1232, the plotPNM.c ranges, DBWdisplay.c:1750-1765, grMain.c:215-216, plotVers.c:1475-1485 | plotPS.c:1180-1192 removed (it is the label-margin scale, not the prologue); plot/plotVers.c:1474-1483; the others kept |
| `router-maze` | mzrouter section grammar | mzTech.c:180-260, :300-800, :1136-1215 | mzrouter/mzTech.c:334-1113, :1136-1281 |
| `router-maze` | Style defaults and lookup | mzTech.c:262-300 (and mzInternal.h:60-120, mzMain.c:272-440, irMain.c:120-160) | mzrouter/mzTech.c:517-562 (`mzSetParmDefaults`); the others kept |
| `router-maze` | Width and spacing derivation from DRC | mzTech.c:150-228, DRCtech.c:4620-4745 | mzrouter/mzTech.c:235-302 (`MZAfterTech`), :1393-1409, drc/DRCtech.c:4620-4722 |
| `router-maze` | Derived search quantities | mzSubrs.c:60-270, mzTech.c:455-495, :1170-1205, mzMain.c:280-370 | kept |
| `router-maze` | Blockage model | mzBlock.c:140-650, mzInit.c:180-260, mzInternal.h:140-170, mzMain.c:571-640 | kept |
| `router-maze` | Lazy blockage generation | mzBlock.c:651-840, mzSearch.c:236-280, mzMain.c:820-850 | mzrouter/mzBlock.c:651-799, mzSearch.c:302-339, mzMain.c:803-813 |
| `router-maze` | Hint, fence and rotate layers | mzHint.c:40-197, mzXtndRght.c:470-520, DBtechtype.c:85-95 | kept |
| `router-maze` | Start and destination registration | mzMain.c:571-710, mzSubrs.c:290-530 | kept |
| `router-maze` | Destination areas | mzBlock.c:842-1487, mzNumLine.c:60-240, mzMain.c:742-760 | kept |
| `router-maze` | Cost estimation | mzEstimate.c:20-1530, :1530-1900, :1900-1960 | mzrouter/mzEstimate.c:201-1568, :1591-1915, :1934-2038 |
| `router-maze` | Initial paths | mzStart.c:60-416, mzMain.c:800-850 | mzrouter/mzStart.c:62-415, mzMain.c:801-830 |
| `router-maze` | Path records and the hashed-best rule | mzSearch.c:1211-1350, :166-200 | mzrouter/mzSearch.c:1211-1443, :266-300 (the stale-path check) |
| `router-maze` | Segment cost model | mzXtndRght.c:550-634, the other extenders, mzWalk.c:60-430 | mzWalk.c:57-515; the others kept |
| `router-maze` | Interesting points | the four extenders, mzInternal.h:460-495, mzSearch.c:730-800 | kept |
| `router-maze` | Contact extension | mzSearch.c:800-1210 | kept |
| `router-maze` | Walk extension | mzWalk.c:60-690 | kept |
| `router-maze` | Windowed bloom search | mzSearch.c:60-730, :1350-1490, mzTech.c:985-1010, mzInternal.h:60-70 | mzrouter/mzSearch.c:182-710, :1462-1473; the others kept |
| `router-maze` | Heap ordering | utils/heap.c:60-420 | utils/heap.c:114-518 |
| `router-maze` | Path to paint conversion | mzMain.c:880-1250, mzSubrs.c:589-651 | kept |
| `router-maze` | Maze router diagnostics | mzTestCmd.c:530-622, mzDebug.c | kept |
| `router-maze-iroute` | iroute availability and dispatch | irCommand.c:2573-2670, irMain.c:120-160 | kept |
| `router-maze-iroute` | iroute route options | irCommand.c:1445-1740 | irouter/irCommand.c:1445-1725 |
| `router-maze-iroute` | Route context and endpoints | irRoute.c:95-470, :505-760, :900-1192 | irouter/irRoute.c:95-480, :500-862, :884-1192 |
| `router-maze-iroute` | Route results | irRoute.c:360-470, irCommand.c:1700-1740 | irouter/irRoute.c:360-480, irCommand.c:1694-1725 |
| `router-maze-iroute` | Layer and contact parameter tables | irCommand.c:200-520, :780-1070, :1177-1440, utils/set.c | kept |
| `router-maze-iroute` | Search, verbosity, version and wizard parameters | irCommand.c:530-780, :1756-1856, :2135-2355 | irouter/irCommand.c:2135-2331; the others kept |
| `router-maze-iroute` | Spacings subcommand | irCommand.c:1856-2130 | kept |
| `router-maze-iroute` | saveParameters file format | irCommand.c:2356-2470, irInternal.h:40, utils/set.c | irouter/irCommand.c:2356-2478; the others kept |
| `router-maze-iroute` | iroute help | irCommand.c:1072-1176, :2480-2570 | kept |
| `router-maze-iroute` | Interactive router diagnostics | irTestCmd.c:215-314 | kept |
| `router-maze-iroute` | Route timeout (ADDED) | irCommand.c:1683-1696, signals.c:145-198 | irouter/irCommand.c:1664-1678 (the `-timeout` option), :1695, :1721-1725 (`leaveClean`, which leaves the timer armed), utils/signals.c:124-199 |

The other ADDED requirements of the three capabilities carry Hefesto-decision traces only. The `Evidence:` lines of the open-questions entries were not re-checked.
