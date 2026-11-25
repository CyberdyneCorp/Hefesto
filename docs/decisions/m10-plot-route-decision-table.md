# M10 decision table: plotting and maze routing

The M10 plotting and routing workstreams (`openspec/changes/m10-plot-route`: `plot-output`, `router-maze`, `router-maze-iroute`) cannot start until every open question their specs cite has a `Resolution:` line. This table lists what has to be decided:

| Group | Count |
|---|---|
| Entries reported as blocking by `python3 scripts/check_milestone_readiness.py --milestone M10` for these three capabilities (4 Q-MISC, 15 Q-LEGACY) | 19 |
| Unresolved plot entries not cited by the spec (Q-MISC-45, Q-MISC-47) | 2 |
| Audit decisions marked "blocks M10" in `docs/decisions/audit/DECISIONS.md` that this change settles | 2 (D-37, D-38) |
| New entries raised by this plan: Q-MISC-48 to -55, Q-LEGACY-37 to -48 | 20 |
| Judgement items (J1–J6) | 6 |
| Planning questions from the design's "Open Questions" | 3 |

**Status: drafted 2026-11-24. Nothing here is approved.** Each new entry has `Status: pending user decision` and no `Resolution:` line, and each existing entry keeps its current state. After the user decides, DECIDE records the answers (tasks 1.1–1.3); the readiness check must then report no entry for these three capabilities. The other M10 entries the check reports (Q-DIGITAL, Q-RELIABILITY) belong to later M10 changes and are not decided here.

**Verify rows at G0.** Q-LEGACY-40 and Q-LEGACY-44 are settled by captures (LEG-MZ-28, LEG-MZ-11). They are approved at G0 as rules with conditional `Resolution:` lines, which the readiness check counts as resolved; task 2.7 adds each `Verified` line with the run id. An outcome that no branch of the approved rule covers goes back to the user before G1-ROUTE.

## Method and sources

Magic 8.3.694 at fc5daa9e (read-only; `git diff fc5daa9e HEAD -- plot irouter mzrouter tcltk commands` in the reference checkout is empty) was read for `plot/`, `irouter/`, `mzrouter/`, `magic/magicps.pro`, `tcltk/mazeroute.tcl`, the build switches in `scripts/configure.in` and the dispatch tables. Nothing was run; rows that depend on running Magic are verify rows. The PDK technology files at the pinned commits were read for their `mzrouter`, `plot` and `lef` sections. No xschem, netgen or ngspice source is involved.

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

- **J1 (D-37, D-38), who owns view export.** (a) *Recommended:* `hefesto-rendering` owns raster and vector export of a view through a new requirement "View export to SVG and PNG", implemented in this change as backend-free core code from the scene model (`display/export/`), with PNG encoded through the zlib already built. `plot svg` uses it; the desktop export and the reliability heat maps use the PNG part; `reliability-visualization` is re-pointed to it. (b) A native PNG format in `plot-output`, and SVG in `plot-output` too. (c) Settle ownership now but defer PNG to the reliability change. (a) closes both audit items with one module and leaves no capability promising an export nobody provides.
- **J2 (Q-LEGACY-37), an `irouter` style for PDKs that have none.** (a) *Recommended:* with `magic-compat` OFF only, derive one from the technology's `lef` section (`routing` lines as route layers with scmos's alternating costs, `cut` lines as contacts, widths and spacings from DRC). (b) No style; users or PDKs add one. (c) Ship hand-written per-PDK overlays. (d) Derive in both modes. (a) makes `iroute` and the M11 agent work on sky130A, gf180mcuD and ihp-sg13g2 while ON still answers as Magic.
- **J3 (Q-MISC-52), the SVG reference.** (a) *Recommended:* Hefesto's own SVG from the scene; a `magic-cairo` capture variant captures MISC-PLOT-12 (compared on name, size and well-formedness) and MISC-PLOT-13 (`plot help`). (b) No Cairo variant; MISC-PLOT-12 is Hefesto-only. (c) Imitate Cairo's output.
- **J4, the router engine interface for M11.** (a) *Recommended:* a pure call `route::maze::route(RouteRequest) -> RouteResult` with no global state, a structured result (code, path, paint, cost, statistics, end reason), a cancellation token, an optional bloom budget and a memory limit; painting, undo and selection stay with the caller. (b) Port Magic's global-state API (`MZInitRoute`/`MZAddStart`/`MZRoute`/`MZClean`) and wrap it per session later. (a) lets several agent scratch sessions route in parallel and makes budgeted routes deterministic.
- **J5, Magic bugs that change plots and routes.** (a) *Recommended:* compat for Q-MISC-41, Q-MISC-43, Q-LEGACY-33, -85, -87 and -89 (one branch point each, design D5). (b) Replicate in both modes, as the entries originally recommended: one code path, but OFF would copy known bugs.
- **J6, the maze helper window of `mazeroute.tcl`.** (a) *Recommended:* a native desktop panel from Options > Maze Router (unrouted and routed nets, route, rip up, verify, save and load failed nets), not on iPadOS. (b) Port only the `mazeroute` command and defer the helper to M12 with `netlist-window`, which needs a MODIFIED delta of the `scripting-auxiliary-tools` requirement.

## Decision table: the blocking entries

### plot-output

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-MISC-41 | `mr`/`mx` dropped when the lower-left corner is outside the area | **compat** | ON: Magic's rule (goldens). OFF: written when any part overlaps; `rectclip` trims (J5). | [E]; [magic] plot/plotPS.c:405-420; [S] "PostScript paint output" |
| Q-MISC-42 | Label and cell text not escaped | **fix** | Escape `(`, `)`, `\` in both modes: Magic's file is invalid PostScript (tier 1). MISC-PLOT-03 becomes a documented deviation. | [E]; [magic] plot/plotPS.c:1027, :1087, :1106 |
| Q-MISC-43 | Label margins mix relative and absolute coordinates | **compat** | ON: Magic's formula (BoundingBox and scale match the goldens). OFF: relative coordinates throughout (J5). | [E]; [magic] plot/plotPS.c:869-872 |
| Q-MISC-44 | Spooling, temporary files, library-path resources | **platform** | Embed `magicps.pro` byte for byte; bundle `mos.7bit.mraster.cmap` and `_dstyle`; no spooling and no process on any platform. | [E]; [S] "Platform mechanism of plot output" |

### router-maze

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-LEGACY-6 | Heap ties depend on the heap algorithm | **replicate** | Port `utils/heap.c` (sift-up, sift-down, lazy ordering). | [E]; [S] "Heap ordering" |
| Q-LEGACY-31 | Malformed `contact` line leaves a half-registered contact | **fix** | Validate the whole statement before registering anything; report and ignore it (with Q-LEGACY-39). | [E]; [magic] mzrouter/mzTech.c:1136-1215 |
| Q-LEGACY-32 | Penalty stored at 0.5 resolution | **replicate** | floor(p × 2)/2; it decides search order. | [E] |
| Q-LEGACY-33 | `width` overridden by DRC derivation | **compat** | ON: DRC wins (Magic). OFF: an explicit `width` overrides, like explicit `spacing` (J5). | [E]; [magic] mzrouter/mzMain.c:235-325 |
| Q-LEGACY-85 | Inverted length test for the initial LR contact | **compat** | ON: Magic's `<=`. OFF: `>`, as the other three sites (J5). | [E]; [magic] mzrouter/mzStart.c:330-370 |
| Q-LEGACY-86 | Same-node zero-cost rule overwrites the parent's estimated total | **replicate** | It decides which start-node point routes leave from; no clear intended behaviour. | [E] |
| Q-LEGACY-87 | Upward extension ignores closer stop points in the free case | **compat** | ON: Magic's unconditional assignment. OFF: nearest candidate, as the other directions (J5). | [E]; [magic] mzrouter/mzXtndUp.c:230-236 |
| Q-LEGACY-88 | Same-node paint of a contact onto its own route type blocks half | **replicate** | Documented as Magic behaviour. | [E] |
| Q-LEGACY-89 | Contact same-node exemption tests the wrong tile | **compat** | ON: Magic's effective rule (as specified). OFF: tests the layer-plane tile (J5). | [E]; [magic] mzrouter/mzBlock.c:380-405 |
| Q-LEGACY-91 | Contact-to-contact spacing checked within one class | **replicate** | — | [E] |
| Q-LEGACY-92 | Start point is the lower-left of the first enumerated tile | **replicate** | With the canonical tile enumeration order of `layout-geometry-model`. | [E] |
| Q-LEGACY-93 | Destination contact-walk passes with an invalid argument | **fix** (remove) | Not written: the passes have no effect. | [E] |
| Q-LEGACY-94 | Vertical extensions use horizontal-strip contact blockage | **replicate** | — | [E] |
| Q-LEGACY-95 | Stale paths still consume blooms | **replicate** | The window position depends on the bloom count. | [E] |
| Q-LEGACY-96 | Straight-shot estimators follow corner-neighbour chains only | **replicate** | Estimates steer the search order. | [E] |

## Plot entries not cited by the spec

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-45 | colorversatec stipple byte swap | **n/a** | Hefesto has no Versatec output (Q-MISC-51); the tech lines are ignored. |
| Q-MISC-47 | PNM downsampling message's memory formula | **replicate** | Print Magic's numbers, computed in 64-bit so they match wherever Magic does not overflow; the console comparison then needs no mask. |

## Audit decisions D-37 and D-38

| ID | Question | Recommendation |
|---|---|---|
| D-37 | Who provides PNG export for reliability heat maps | (a) `hefesto-rendering`, from the scene model on the CPU (not a GPU readback, so it works headless and on iPadOS), delivered in this change (J1). |
| D-38 | Who owns SVG and vector export | `hefesto-rendering`, requirement "View export to SVG and PNG"; `plot svg` in `plot-output` calls it (J1). |

## Entries raised by this plan

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-48 | Zero-area PostScript plot divides by zero | **fix** | Refuse with "Nothing to plot: the plotted area has zero width or height."; write nothing. |
| Q-MISC-49 | PNM memory arithmetic overflows and can hang | **fix** | 64-bit sizes, `pnmmaxmem` 1..4 194 304 KB, a strip of at least 2·scale_over_2 + 1 rows, downsampling capped at 2^15, 65 535 pixels per side. |
| Q-MISC-50 | Malformed `plot` style lines | **fix** / **replicate** | Fix the bare `map` crash, unchecked numeric fields and undefined colour/pattern references (report and ignore); replicate the silent ignoring of long `draw` lines and unknown pnm keywords. |
| Q-MISC-51 | Plot devices and parameters Hefesto offers | **platform** | (a) The default Magic build plus `svg`: `postscript`, `pnm`, `svg`, `parameters`, `help`; no raster devices or HP/Versatec parameters; device tech lines ignored. |
| Q-MISC-52 | SVG content and its golden reference | **platform** | (a) Native SVG from the scene; `magic-cairo` capture variant compared on name and size (J3). |
| Q-MISC-53 | Interrupted and failed plots leave partial files | **fix** | Temporary file and rename; an old file of the same name stays unchanged. |
| Q-MISC-54 | Byte identity across platforms | **platform** | (a) Committed Lanczos table generated in the capture image; `-ffp-contract=off`; correctly rounded float formatting. |
| Q-MISC-55 | `.pnm` appended only when the whole path has no dot | **compat** | ON: whole path. OFF: the last path component. |
| Q-LEGACY-37 | The supported PDKs have no `irouter` style | **platform** | (b) Derived from the `lef` section with `magic-compat` OFF only (J2). |
| Q-LEGACY-38 | Fence start points leave undo disabled for the session | **fix** | Undo restored on every path; message and start choice kept. LEG-MZ-29 is a documented deviation. |
| Q-LEGACY-39 | Bare `style`, half-built `layer` lines, shared pointers in style copies | **fix** | Whole-statement validation; a bare `style` reports and skips to the next `style`; styles are values. |
| Q-LEGACY-40 | Several endpoint nodes when the session cannot ask | **verify** / **fix** | Take the empty answer (yes, first candidate) and never block; LEG-MZ-28 shows whether Magic does the same (then replicate). |
| Q-LEGACY-41 | `-timeout` is wall-clock and can stay armed | **fix** + **platform** | The deadline lives in the command's cancel token (no leak); a deterministic bloom budget in the engine interface only. |
| Q-LEGACY-42 | Already routed prints "Route Failed." | **compat** | ON: `Route Failed.` OFF: `Already routed.` (information). Script result unchanged. |
| Q-LEGACY-43 | Saved spacings in internal units read back as lambda | **compat** | ON: bare integer. OFF: the `i` suffix (precedent Q-UI-24). |
| Q-LEGACY-44 | Sourcing a `saveParameters` file in a Tcl session | **verify** | Replicate if Magic's Tcl `source` restores the parameters (LEG-MZ-11); otherwise compat (OFF accepts the leading `:`). |
| Q-LEGACY-45 | Route coordinates reject unit suffixes; `-dRect` not normalised | **compat** | ON: Magic. OFF: any coordinate form, rectangle normalised. |
| Q-LEGACY-46 | `iroute` parameter and message quirks | **replicate** | Value cycling, partial spacing sets, Magic's texts and help as they are. |
| Q-LEGACY-47 | `wizard penalty` is unchecked | **fix** / **replicate** | Reject values whose mantissa overflows 32 bits; keep negative values. |
| Q-LEGACY-48 | Maze search memory is unbounded | **platform** | (a) `router.max-search-memory` (1 GiB desktop, 256 MiB iPadOS), ending as an interrupt. |

## Planning questions raised by this plan

| # | Question | Recommendation |
|---|---|---|
| 1 | How spec changes are recorded | (a) As this change's deltas, synced into the living specs at G0, then `skip_specs: true` (M9's approach). |
| 2 | When this change starts relative to the 3D view | (a) R0 now; R1 as soon as G0 passes, in parallel with the 3D view's P2; no shared files. |
| 3 | Desktop scope | (a) The export dialog and the maze helper panel, with DESIGN boards first. |

## Counts

| Kind | Count | IDs (without `Q-`) |
|---|---|---|
| replicate | 11 | LEGACY-6, LEGACY-32, LEGACY-86, LEGACY-88, LEGACY-91, LEGACY-92, LEGACY-94, LEGACY-95, LEGACY-96, LEGACY-46, MISC-47 |
| compat | 10 | MISC-41, MISC-43, MISC-55, LEGACY-33, LEGACY-85, LEGACY-87, LEGACY-89, LEGACY-42, LEGACY-43, LEGACY-45 |
| fix | 10 | MISC-42, MISC-48, MISC-49, MISC-50 (in part), MISC-53, LEGACY-31, LEGACY-38, LEGACY-39, LEGACY-47 (in part), LEGACY-93 |
| fix + platform | 1 | LEGACY-41 |
| platform | 6 | MISC-44, MISC-51, MISC-52, MISC-54, LEGACY-37, LEGACY-48 |
| verify | 2 | LEGACY-40, LEGACY-44 |
| n/a | 1 | MISC-45 |

Total: 41 entries (19 blocking, Q-MISC-45, Q-MISC-47 and 20 new), plus D-37, D-38, six judgement items and three planning questions.

## Spec changes the recommendations imply

These are recorded as spec deltas in `openspec/changes/m10-plot-route/specs/`, and they are applied to the living specs at G0 (task 1.2) once the matching rows are approved:

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
| `router-maze-iroute` | MODIFIED "iroute availability and dispatch" | Q-LEGACY-37 |
| `router-maze-iroute` | MODIFIED "Route context and endpoints" | Q-LEGACY-40 |
| `router-maze-iroute` | ADDED "Derived irouter style" | Q-LEGACY-37 (J2) |
| `router-maze-iroute` | ADDED "Route timeout" | Q-LEGACY-41 |
| `router-maze-iroute` | ADDED "iroute behaviour with magic-compat off" | Q-LEGACY-42, -43, -44, -45 |
| `hefesto-rendering` | ADDED "View export to SVG and PNG" | D-37, D-38 (J1) |
| `hefesto-performance` | ADDED "Plot and maze-route throughput" | — |
| `reliability-visualization` | MODIFIED "Touch and platform parity" (PNG export through `hefesto-rendering`) | D-37 |

Edits that deltas cannot express, made at G0: the `tech-file-format` note that the `plot` and `mzrouter` sections are now interpreted (no requirement changes); the M4 deferral closed in `docs/decisions/m4-decision-table.md` when MAZETOOL lands.

## Verification runs needed before G1-ROUTE

| Row | Case | Question the capture answers |
|---|---|---|
| Q-LEGACY-40 | LEG-MZ-28 | Does Magic in a batch run with stdin at end of file connect the first candidate, another one, or fail? |
| Q-LEGACY-44 | LEG-MZ-11 | Does `source p.ir` in Magic's Tcl build restore the parameters despite the leading `:`? |
