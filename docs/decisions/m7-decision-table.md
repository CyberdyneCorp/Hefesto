# M7 decision table: the 56 open questions blocking extraction and netlisting

M7 ("Extraction and netlisting", docs/roadmap.md) cannot start until every open question its specs cite has a `Resolution:` line. `python3 scripts/check_milestone_readiness.py --milestone M7` lists 56: 19 in `docs/decisions/open-questions/EXTRACT.md`, 4 in `MISC.md` (antenna check) and 33 in `NETLIST.md`. Each entry was checked against Magic 8.3.694 (fc5daa9e, `/Users/leonardo/trabalho/magic`, read-only) and, where a PDK is named, against the sky130A, gf180mcuD and ihp-sg13g2 tech files. Each entry carries a rewritten `Recommendation:` line with the full reasoning; corrected evidence is marked `(corrected 2026-11-13: ...)`, and evidence that was right gets a `Verified 2026-11-13:` line. **All 56 were approved on 2026-11-13** as drafted, with no option changed; the three closest calls below take their recommended option (a). Each entry now carries `Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)` and a `Resolution:` line, and `python3 scripts/check_milestone_readiness.py --milestone M7` reports `M7: ready`.


**Status as of 2026-11-21.** The 56 entries above stay approved. The M7 implementation plan (`openspec/changes/m7-extraction/`, drafted 2026-11-20) raised nine planning questions and one new behaviour entry, Q-EXTRACT-27. **All ten were approved by the user on 2026-11-21, each with its recommended option**: (a) for Open Questions 1–9 and fix for Q-EXTRACT-27 (sections "Planning questions raised by the M7 plan" and "Entries raised by the M7 plan" below). The spec edits they imply are applied ("Spec changes the answers imply"). `extract-commands` and `extresist-output` now cite Q-EXTRACT-27, so `check_milestone_readiness.py --milestone M7` checks 57 entries and reports ready. The plan's round R0 (tasks section 1) is done. A review of the plan was applied the same day: rows 2, 3, 4, 6 and 9 and Q-EXTRACT-27 below reflect it (the as-landed M10 scaffold, the `atan` fallback, the `ihp-gds-sc` set and size budget, the `extcheck` case mode, the 155-case total, and `.res.ext` in Q-EXTRACT-27).

**Status as of 2026-11-23.** The user approved the R2 follow-ups M7-14 to M7-18 with their recommended options: Q-NETLIST-54 (compat), Q-EXTRACT-30 (fix, confirmed), Q-NETLIST-55 to 58 (compat-neutral) and the plan item M7-18 (section "Entries approved 2026-11-23").

**Status as of 2026-11-26.** The user approved on 2026-11-25 every row raised in rounds R3 and R4, each with its recommended option: Q-EXTRACT-31 to 35 and 38 to 41 (compat; Q-EXTRACT-41 as implemented, OFF one device per type), Q-EXTRACT-36 (fix, both modes), Q-NETLIST-59, 61, 65, 66, 67 (compat), Q-NETLIST-60 (`dummy#<n>` per cell), Q-NETLIST-62, 64, 68, 69 and Q-MISC-92, 93 (fix, both modes) and Q-NETLIST-63 (fix in both modes; located in round R5: ResReadDevice's substrate node with no record); Q-EXTRACT-37 stays withdrawn. The user also approved a standing rule: a new crafted-input resource limit in both modes is approved with its recommended value, recorded here and reported. Section "Entries approved 2026-11-25" lists them, with the limits and the spec edits (applied 2026-11-26 in round R5). No row is pending.

The standing rule for Magic bugs and quirks is compat: `magic-compat` ON copies Magic, OFF does the intended thing. Most rows follow it. Where they do not, the row says why: replicate when Magic defines no corrected behaviour, the "bug" turns out to be Magic's intended semantics, or the quirk is an internal unit or order choice that byte identity needs; fix when the quirk loses data, is undefined behaviour, or has no Magic output to match. Several compat rows also carry a tier-1 guard that applies in both modes (Q-MISC-4, Q-NETLIST-8, Q-NETLIST-29, Q-NETLIST-30); the guard only changes inputs on which Magic has undefined behaviour.

Recommendation kinds (X-2, docs/decisions/cross-cutting.md):

- **replicate**: do what Magic does, in both modes.
- **compat**: replicate with `magic-compat` ON and use the corrected behavior with it OFF (tier 2).
- **fix**: corrected behavior in both modes (tier 1, or tier 3 when there is no Magic output to match).
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: none of the above.
- **compat-neutral**: Magic's behaviour, the same in both modes, recorded so that a behaviour implemented without a row has one (used from 2026-11-23).

## Judgement items

None remain (all 56 approved as drafted on 2026-11-13). Every item is settled by X-2, X-4, D-7 or an earlier resolution (Q-EXTRACT-5), so no row needed a product or scope decision. The three closest calls are kept below for the record; the user took the recommended option (a) for each.

1. **Q-EXTRACT-12: path-length values depend on tile decomposition.**
   The entry recommended a behavioural golden tolerance. Since Q-EXTRACT-5 already commits Hefesto to Magic's tile decomposition and enumeration order, the draft asks for a byte-exact port of the flood instead. This raises the porting bar for `extract length`; a tolerance would be cheaper but would hide port errors.
   - (a) **Recommended.** replicate tile-exact (`Compatibility: exact`)
   - (b) behavioural comparison within a tolerance (the entry's original recommendation)
2. **Q-EXTRACT-2: diode substrate types parsed from the terminal argument.**
   The entry treated this as a bug against a documented syntax, but that syntax is only in Hefesto's own spec table; Magic's manual has none, and sky130A's photodiode line uses argument 5 as the substrate node name. The draft drops the lint warning the entry proposed.
   - (a) **Recommended.** replicate Magic's syntax `OTHER [SUBNODE]`, no lint warning
   - (b) fix: substrate types = argument 5, name = argument 6 (breaks sky130A)
3. **Q-NETLIST-37: flat SPICE ignores ports for subcircuit marking.**
   Magic 8.3.694 changed this on purpose (commit 9800d98d), and its documentation still says `top auto` wraps cells with ports. The draft follows the standing rule; with magic-compat OFF, flat netlists of cells with ports gain a `.subckt` wrapper, which is the visible product change.
   - (a) **Recommended.** compat: ON as Magic (only `top on` wraps); OFF `top auto` wraps when the root has ports
   - (b) replicate in both modes (the entry's original recommendation)
   - (c) fix: mark subcircuits in flat mode as documented, in both modes

## Decision table

Every row is approved (2026-11-13); the Recommendation column is the decision recorded in the entry's `Resolution:` line. Each row's full text is the entry's `Recommendation:` line. Where the draft changes the entry's original recommendation, that line ends with `(corrected 2026-11-13, was ...)`.

### Extraction technology section

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-1 | defaultsideoverlap shield direction chosen by plane index, not plane order | **compat** | ON: direction by plane index; OFF: by plane order; no PDK affected |
| Q-EXTRACT-2 | Diode substrate types parsed from the terminal argument | **replicate** | OTHER is terminal and substrate types, argument 5 is the substrate node name (Magic's syntax; sky130 photodiode relies on it) |
| Q-EXTRACT-3 | Device perimeter capacitance (gscap) scaled as an area | **compat** | ON: gscap scaled as an area; OFF: as a per-length coefficient; no PDK sets gscap |
| Q-EXTRACT-19 | Truncations in step and halo parsing | **compat** | ON: truncate step/halo; OFF: round after unit conversion; PDKs identical |
| Q-EXTRACT-21 | Fractional parameter limits in lambda units | **compat** | ON: /1000 truncation of limits and offsets; OFF: milli-unit comparison |

### Nodes, labels and hierarchy

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-4 | Name selection when the first attached label is an attribute | **compat** | ON: Magic's list order (latest name wins after an attribute head); OFF: first attached name wins |
| Q-EXTRACT-6 | Temporary unique labels not reverted for some cells | **fix** | revert temporary `_uqN` labels in every renamed cell, top cell included (tier 1 data loss) |
| Q-EXTRACT-22 | Shorted-ports warning suppressed by lumped resistance | **compat** | ON: warning suppressed by lumped resistance; OFF: only by extresist; .ext unchanged |
| Q-EXTRACT-23 | Which node keeps its name under unique renaming | **replicate** | later node keeps the name, earlier port wins over a non-port |
| Q-EXTRACT-26 | Cumulative-area sticky-label merge may create phantom nodes | **replicate** | unify every sticky label in the cumulative def; add array/labelcheck goldens |

### Capacitance

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-17 | Shield fractions accumulate across multiple sideoverlap rules (fringe-halo model) | **compat** | ON: sum shield fractions per rule, unreduced type; OFF: once per tile, residue type |
| Q-EXTRACT-18 | Legacy sideoverlap substrate subtraction uses whole-segment length | **compat** | ON: MIN with whole-segment overlap; OFF: MIN with clipped length, counted once (legacy model only) |
| Q-EXTRACT-20 | Single-precision denominator in sidewall coupling | **replicate** | single-precision offset and `sep + offset` (byte identity of %lg) |
| Q-EXTRACT-24 | Inconsistent concave-corner tests (substrate vs coupling) | **compat** | ON: type test for substrate corners; OFF: coupling test (same node, free quadrant space) |

### Devices, file format and commands

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-10 | Version string written in .ext | **replicate** | write `version 8.3` as the .ext format version; warn on read when it differs |
| Q-EXTRACT-11 | Output path creation and current-directory fallback | **platform** | desktop as Magic; sandboxed hosts: project folder is the cwd, directories via file coordination |
| Q-EXTRACT-12 | Path-length values depend on tile decomposition | **replicate** | port the flood exactly; `distance` byte-identical (Q-EXTRACT-5 fixes the tiles), no tolerance |
| Q-EXTRACT-16 | device cell property dimensions (W L order, upl scaling) | **replicate** | pass property tokens through in written order, truncating upl scaling |
| Q-EXTRACT-25 | Halo/step overrides lost on style reload | **replicate** | overrides change the loaded style, lost on style reload, scaled on grid rescale |

### Antenna check

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-1 | Cumulative antenna model never reports violations | **compat** | ON: cumulative model reports nothing; OFF: accumulate over planes with order ≤ check plane |
| Q-MISC-3 | Antenna line without calc type and no default | **fix** | store 'no calculation type'; layer contributes no area (uninitialised read) |
| Q-MISC-4 | Contact cut count mixes internal and output units | **compat** | no-rule contacts give area 0 and zero pitch one cut in both modes; ON: Magic's unit mix; OFF: consistent units |
| Q-MISC-7 | Connectivity restriction uses plane numbers and one contact type | **compat** | ON: first contact on the plane, plane numbers (gf180 metal4 differs); OFF: all contacts, plane order |

### ext flattening (read and names)

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-8 | Parameter scale and offset handling | **compat** | `*S` offset is 0 in both modes; ON: offset only in the SI scale-1 branch; OFF: in every branch |
| Q-NETLIST-9 | Declared parameter order is reversed on output | **replicate** | reverse token order of declared and verbatim parameters, as Magic |
| Q-NETLIST-10 | Name preference favors the lexically greater name | **replicate** | on a full tie the lexically greater root-first name wins |
| Q-NETLIST-12 | `resist` values are offset by +0.5 mΩ | **compat** | ON: `resist` = 0.5 + rscale×R; OFF: rscale×R, still float |
| Q-NETLIST-30 | Leaf-name conversion deletes characters anywhere | **compat** | ON: delete `!`/`#` anywhere in the leaf; OFF: trailing only; leading-comma underwrite guarded in both |
| Q-NETLIST-31 | Shorted-port devices create a `dummy` node | **compat** | ON: shared `dummy` terminal 0 (tee mode joins all shorted pairs); OFF: anonymous terminal 0 per short device |

### SPICE netlisting

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-4 | Flat writer prints resistance for an unnamed c parameter | **compat** | ON: flat `c` writes the resistance; OFF: the capacitance |
| Q-NETLIST-5 | Flat-mode tee for rsubckt never writes its second half | **compat** | ON: flat tee writes only X<n>A; OFF: also X<n>B as in hierarchical mode |
| Q-NETLIST-6 | Flat merge multipliers desynchronize on 1-terminal devices | **fix** | merge multiplier and killed flag keyed by device; 1-terminal devices get M = 1 (UB in Magic) |
| Q-NETLIST-7 | Terminal area/perimeter divided by M only in scale mode | **compat** | ON: divide area/perimeter by m only in scale mode; OFF: always |
| Q-NETLIST-13 | `ext2spice run` and unknown option words become the cell name | **compat** | ON: `run` and ambiguous words become the cell name; OFF: `run` keyword, ambiguous prefix is an error |
| Q-NETLIST-19 | Hierarchical and flat SPICE formatting disagree | **compat** | compat for flat SI perimeter 1e-12 and hierarchical `%d` w/l; replicate signed thresholds and connected test per mode |
| Q-NETLIST-20 | Hierarchical output omits node capacitances without a `substrate` record | **compat** | ON: no hierarchical node lines without a `substrate` record; OFF: written as in flat mode |
| Q-NETLIST-32 | Consumed `l<n>`/`w<n>` parameters leave a stray space | **compat** | ON: consumed `l<n>`/`w<n>` leave a space; OFF: skipped |
| Q-NETLIST-34 | Flat device names from gate attributes lack a separator | **compat** | ON: `Mu1foo`; OFF: `Mu1/foo` |
| Q-NETLIST-35 | Hierarchical voltage-source counter not reset per cell | **compat** | ON: V counter reset per run only; OFF: per subcircuit |
| Q-NETLIST-37 | Flat SPICE ignores ports for subcircuit marking | **compat** | ON: flat `top auto` never wraps; OFF: wraps when the root has ports; `descend` inert in flat mode |
| Q-NETLIST-41 | `subcircuits descend auto` changes the top setting | **compat** | ON: `descend auto` sets top to auto; OFF: usage error |

### ext2sim

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-15 | ext2sim node-capacitance line lost its GND field | **compat** | ON: `C <name>0 <cap>`; OFF: `C <name> GND <cap>` (irsim) |
| Q-NETLIST-16 | Name-conversion flags leak between commands | **compat** | ON: session-wide OR-only name flags; OFF: per-run flags from each command's base |
| Q-NETLIST-17 | ext2sim merges devices of different class or type | **compat** | ON: ext2sim merge ignores class/type; OFF: also require equal class and type |
| Q-NETLIST-18 | ext2sim `-c` does not enable coupling output | **compat** | ON: flatten caps/resists by option thresholds; OFF: by the run's `-c`/`-r` |
| Q-NETLIST-48 | sim2simp is shipped as source only and is not built | **other** | omit sim2simp; `ext2sim merge` covers it |

### extresist

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-23 | `extresist skip` takes a single type | **fix** | `skip` takes a comma list; bad names leave masks unchanged (OOB write in Magic) |
| Q-NETLIST-24 | Gate load uses (W+W) for source/drain capacitance | **replicate** | keep `2·W·sdCap`: gscap is per edge length and a gate has two edges |
| Q-NETLIST-25 | Path-resistance heap uses integer keys | **compat** | ON: int keys and Magic's heap ties; OFF: double keys, insertion-order ties |
| Q-NETLIST-28 | `mindelay` query reports raw zeptoseconds in the standalone build | **replicate** | query returns ps, as the normative Tcl build (D-7) |
| Q-NETLIST-29 | `attr` records apply to the previous node in extresist | **compat** | ON: `attr` applies to the previous node; OFF: to the named node; orphan attr and res:min overflow guarded in both |
| Q-NETLIST-39 | `.res.ext` device records omit value and L/W for non-MOS classes | **compat** | ON: Magic's .res.ext device record; OFF: resistor/capacitor/caprev get value or L W and extractor terminals |
| Q-NETLIST-40 | FastHenry unnamed-node ids restart for every net | **compat** | ON: FastHenry N/E counters per net; OFF: per file |
| Q-NETLIST-43 | Near-device tile with a single breakpoint loses its area | **compat** | ON: single-breakpoint near-device tile adds no area; OFF: adds width × height |
| Q-NETLIST-44 | Delta-to-star conversion adds 0.5 mΩ instead of rounding | **compat** | ON: star arms + 0.5 mΩ; OFF: no offset |
| Q-NETLIST-46 | Path-resistance heap behaves differently for the first net of a session | **compat** | ON: session-wide heap state; OFF: fresh per-net queue |

## Counts

| Kind | Count | IDs |
|---|---|---|
| compat | 38 | EXTRACT-1, EXTRACT-3, EXTRACT-4, EXTRACT-17, EXTRACT-18, EXTRACT-19, EXTRACT-21, EXTRACT-22, EXTRACT-24, MISC-1, MISC-4, MISC-7, NETLIST-4, NETLIST-5, NETLIST-7, NETLIST-8, NETLIST-12, NETLIST-13, NETLIST-15, NETLIST-16, NETLIST-17, NETLIST-18, NETLIST-19, NETLIST-20, NETLIST-25, NETLIST-29, NETLIST-30, NETLIST-31, NETLIST-32, NETLIST-34, NETLIST-35, NETLIST-37, NETLIST-39, NETLIST-40, NETLIST-41, NETLIST-43, NETLIST-44, NETLIST-46 |
| replicate | 12 | EXTRACT-2, EXTRACT-10, EXTRACT-12, EXTRACT-16, EXTRACT-20, EXTRACT-23, EXTRACT-25, EXTRACT-26, NETLIST-9, NETLIST-10, NETLIST-24, NETLIST-28 |
| fix | 4 | EXTRACT-6, MISC-3, NETLIST-6, NETLIST-23 |
| platform | 1 | EXTRACT-11 |
| other | 1 | NETLIST-48 |

Total: 56. IDs are given without the `Q-` prefix.

Approved 2026-11-22 ("Entries approved 2026-11-22"): compat 4 (EXTRACT-29, MISC-91, NETLIST-49, NETLIST-53), fix 4 (EXTRACT-28, NETLIST-50, NETLIST-51, NETLIST-52), with Q-EXTRACT-27 (fix, 2026-11-21) under "Entries raised by the M7 plan".

Approved 2026-11-23 ("Entries approved 2026-11-23"): compat 1 (NETLIST-54), fix 1 (EXTRACT-30), compat-neutral 4 (NETLIST-55, NETLIST-56, NETLIST-57, NETLIST-58), and the plan item M7-18.

Approved 2026-11-25 ("Entries approved 2026-11-25"): compat 14 (EXTRACT-31, 32, 33, 34, 35, 38, 39, 40, 41, NETLIST-59, 61, 65, 66, 67), fix 8 (EXTRACT-36, NETLIST-62, 63, 64, 68, 69, MISC-92, 93), compat-neutral 1 (NETLIST-60), withdrawn 1 (EXTRACT-37), and the standing rule for crafted-input resource limits.

## Planning questions raised by the M7 plan

These come from planning the implementation, not from the specs. Each is drafted in `openspec/changes/m7-extraction/design.md` "Open Questions" with its options and reasons. **All nine were approved by the user on 2026-11-21, each with its recommended option (a).** Options are listed with the recommended one first.

Evidence tags: **[magic]** Magic 8.3.694 source at fc5daa9e (`/Users/leonardo/trabalho/magic`, read-only); **[spec]** a Hefesto capability spec; **[code]** Hefesto at origin/main 0c5f29fb; **[data]** the pinned PDK sets; **[plan]** an earlier milestone plan or user decision.

| # | Question | Options (recommended first) | Decision (approved 2026-11-21) | Evidence | Shapes |
|---|---|---|---|---|---|
| 1 | Where the extractor and extresist keep per-tile state | (a) one pointer-sized `client` field on `detail::Tile` with `Plane::client`/`set_client`/`reset_clients`, plus the restricted `db/plane_stitch.hpp` for split and join on scratch planes; (b) a per-run hash map keyed by `TileView::tile` (stitch edits still need (a)'s header); (c) an extractor-private copy of the tile module | **(a)**: a one-to-one port, O(1) access as in Magic, no copy of the planes, so Q-EXTRACT-5's order comes for free; its cost (one field, the field Magic's `Tile` has) is measured by `drc-profile`, `gds-bench` and the iPad memory check before R2 | [magic] tiles/tile.h:65-78 (`ti_client`); extract/ExtRegion.c:71-88, 142-209, 533 (about 40 uses in `extract/`); resis/ResFract.c:436-488 (TiSplitX, TiJoinY1), about 60 `ti_client` uses in `resis/`. [code] `db/plane.hpp` (no client access), `src/db/tile_plane.hpp:49-57` (no field), `connect/search.cpp:80-95` (`std::set` of visited tiles) | design D5, D6, D16, D20, D23 |
| 2 | Starting M7 while M10's 3D P1 extends `extract/tech/` | (a) start now: EXTTECH writes its handlers in new files and wires each with a one-line `case` in the existing switch of `StyleReader::apply`; the dispatch-table refactor waits for M10's TECH merge; M7 adds its own editor-side tech-load and `scalegrid` hook; the other workstreams use the scaffold's `ExtStyle` and `StyleBuilder`; (b) start now except EXTTECH, which waits for TECH; (c) hand `extract/tech/` to M7 now | **(a)**: the user asked on 2026-11-20 to run M7 with the 3D work to gain time; while TECH is open the overlap is additive `case` lines, and the one refactor of TECH's pass runs after TECH merges. SPICE and SIM need EXTTECH's resist and device parsing for ExtGetDevInfo; under (a) it is wired through the existing switch, so the R2 netlister replays do not wait for M10 (`StyleBuilder` fixtures until then). M10's `HeightView` becomes a view of `ExtStyle`, with `rescaled` derived from the session's style, so `LayerStack` does not change | [plan] m10-3d-view design D3 (TECH owns `extract/tech/` in P1), D5, tasks 3.1-3.2, Open Question 4 (a), "As-landed scaffold contracts" (`HeightView`, no loader hand-off; `m10-r1-scaffold` 73d4d195); m6 design D10 ("there is one extract reader"). [code] `src/core/src/extract/tech/style_reader.cpp:38` (`kKeywords`), `StyleReader::apply` (the switch). [magic] ext2spice/ext2spice.c:932, ext2sim/ext2sim.c:647 (ExtGetDevInfo, extract/ExtTech.c:312-466) | design D1, D3, D5, D7, D20 |
| 3 | Bit-identical `atan` in the fringe-halo model | (a) a correctly rounded binary64 `atan` (vendored from CORE-MATH, MIT, after a licence check, or written in-house from IEEE operations as for the OASIS CIRCLE), checked against Magic on the capture image on the final `%lg` text, with the image's glibc version recorded; fallback for any remaining text difference: a pinned exception table (argument → glibc's double), or, above 1,000 entries, an approved deviation for the affected cells; (b) the platform `atan` with a tolerance where digits differ; (c) port glibc's `atan` | **(a)**: `.ext` values must match Magic byte for byte on three platforms, and the fringe-halo model is on for every PDK cell (tech format 35 ≥ 34); glibc's `atan` is not correctly rounded (its slow paths were removed), so a last-ULP difference is possible, but `%lg` prints six significant digits and hides most of them; the fallback keeps the output byte-identical; (b) breaks byte identity, (c) is LGPL | [magic] extract/ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875 (`0.6366 * atan(mult * d)`); extract/ExtTech.c:3574-3587 (fringehalo default). [spec] `hefesto-output-determinism`; `extract-tech-section` "Fringe model default chosen from tech format". [data] sky130A, gf180mcuD, ihp-sg13g2 tech `format 35`. [plan] Q-OASIS-31 | design D4, D9, D22 |
| 4 | Scope of the PDK standard-cell golden cases (exit criterion) | (a) every cell of `sky130_fd_sc_hd` (446), `gf180mcu_fd_sc_mcu7t5v0` (229) and `sg13g2_stdcell` (84, via `gds read` from the `ihp-gds-sc` set, not the 456 MB `ihp-gds`): `EXTRACT-PDK-<lib>` and `NETLIST-PDK-<lib>`, 6 cases on 759 cells, about 10–15 MB of uncompressed expected text (budget 15 MB; `tests/golden` is 46 MB today); (b) about 20 cells per library; (c) sky130 only | **(a)**: standard cells extract in milliseconds, the sets are already pinned, and three tech files cover device variants, wells and substrates and the fringe-halo model; `expected/` stays uncompressed so diffs are reviewable, and an over-budget capture becomes an open item before it is committed | [spec] docs/roadmap.md M7 exit criteria. [data] `.mag` counts in the pinned sets (446, 229); IHP has no `.mag` cells, `ihp-gds-sc` holds `sg13g2_stdcell` alone (`scripts/fetch_pdk.py` docstring); `sg13g2_stdcell` has 84 macros (m6 design D2, PDK row). [code] `du -sh tests/golden` = 46 MB | design D21, D25 |
| 5 | How the extraction and SPICE performance target is judged | (a) amend `hefesto-performance` so extraction and SPICE generation are judged as GDS is: at most Magic's callgrind count, then baseline + 2 %, wall time informational; (b) keep wall time, repeated runs on one runner; (c) gate on both | **(a)**: the same requirement already moved DRC and GDS to instruction counts because the wall-clock ratio moves by about 0.1×; the workloads (`extract all`, `ext2spice`, `ext2sim` on the `gds-bench` block) are defined in design D23; this is a spec change | [spec] `hefesto-performance` "Batch throughput relative to Magic" (lines 36-48, scenario "Extraction benchmark"). [plan] m6 Open Question 4 (a) | design D23; `hefesto-performance` |
| 6 | How users run `extcheck` | (a) a small `hefesto-extcheck` executable in `src/cli/extcheck/` on desktop hosts, installed and packaged next to `hefesto-cli`; (b) `hefesto-cli --extcheck`; (c) a console command only | **(a)**: `netlist-extcheck` defines a standalone checker with an exit status; one tiny binary keeps scripts that call `extcheck` working with a rename only. The golden case NL-CHK-01 runs it through a new external-command case mode (program mapped `extcheck` → `hefesto-extcheck`, usage line's program name masked). Magic's `extcheck` may not link in the Tcl build (EFantenna.c in `libextflat.o` references `ExtCurStyle` and Tcl), so the image falls back to a non-Tcl configure for it | [spec] `netlist-extcheck` "Invocation". [magic] Makefile.in:10, 128-130 (`extcheck` not in the default build); extcheck/Makefile.in (links `extflat/libextflat.o` and `utils/libutils.a`); extflat/EFantenna.c:184, 440-466 (`ExtCurStyle`) | design D2, D15 |
| 7 | Desktop scope | (a) enable the top bar's "Extract" button (`extract all` on the active root, progress and cancel), no new boards; (b) also a netlist dialog and an "Extraction" menu, drawn by DESIGN first; (c) leave the button disabled | **(a)**: the button exists, painted and disabled with an M7 tooltip since M1; the console covers the rest, as for M6 | [code] `docs/design/m1-ui-conformance.md:91`. [plan] m6 Open Question 5 (a) | design D19 |
| 8 | Threading | (a) single-threaded in M7, with no globals in `ExtractContext` so per-cell parallelism can come later; (b) parallel per-cell extraction in M7 with serial-order output; (c) parallelism inside a cell by step squares | **(a)**: the port is large, the gate is single-threaded, and Magic's per-cell order fixes console output and incremental decisions | [spec] `hefesto-performance` ("Multithreaded runs SHOULD be faster"); `hefesto-architecture` (results equal to single-threaded runs). [magic] extract/ExtMain.c extExtractStack | design D5, D23 |
| 9 | The DEF-write net cases once Hefesto extracts | (a) keep their Magic-written `.ext` inputs and add one end-to-end case, LEFDEF-336 (`extract all` then `def write` on LEFDEF-317's layout), counted in M7's 155 golden cases; (b) switch the 17 cases to run `extract all` first | **(a)**: each case keeps testing one feature, and an extractor regression does not show up as 17 DEF failures | [plan] m6 Open Question 1 (a), m6 task 1.6 (`ext-inputs` mode). [code] `tests/golden/LEFDEF/CASES.md` (LEFDEF-317..328, 330..334) | design D21, D25 |

Counts: 9 decided (all option (a), 2026-11-21), 0 pending.

### Spec changes the answers imply

Applied 2026-11-21, after the approval:
- **Open Question 5 (a):** `hefesto-performance` "Batch throughput relative to Magic": the bullet "Extraction and SPICE generation: wall time" became "instruction counts", with the text of m7-extraction design D23 and the D23 workloads, citing the decision; the scenario "Extraction benchmark" compares callgrind counts, and a new scenario "SPICE generation benchmark after the baseline is recorded" checks the baseline + 2 % gate. P-24 (`docs/decisions/product.md`), which the requirement cites, now covers extraction and SPICE generation.
- **Q-EXTRACT-27 (fix):** `extract-commands` "Incremental extraction" and "Output path and file location", and `extresist-output` "Overlay file grammar and order", state that each `.ext`, `.res.ext`, `.res.lump` and `.fh` file is written completely or not at all, cite the resolution, and gain a scenario for a cancelled run each.
- The other eight answers need no spec change: they shape the plan only (design and tasks).

## Entries raised by the M7 plan

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-EXTRACT-27 | An interrupted extraction or extresist run leaves truncated output files that later runs treat as current | **fix** (approved 2026-11-21) | both modes: resolve the target path as ExtFileOpen does (`ExtLocalPath`, the `cd_file` directory, the retry with the path stripped), write each `.ext`, `.res.ext`, `.res.lump` and `.fh` under a temporary name in that directory and rename it after the last record; a symbolic link is resolved and its target replaced, so the link survives as when Magic writes through it; if no temporary file can be created there, write in place as Magic does; on cancel or error keep the previous file. Uninterrupted runs are byte-identical to Magic, so no golden case changes | [magic] extract/ExtCell.c:99-116 (file opened first, closed as is), :153 on (ExtFileOpen's path fallbacks), :515-531 (phases skipped on interrupt after the header); extract/ExtMain.c:968-990 (only the first line's timestamp is checked), :295-296 (the cell is then skipped); resis/ResRex.c:1395 (extresist stops on interrupt). [spec] `simulation-netlist-preparation` (`rc` mode trusts a `.res.ext` newer than the `.ext`) |

The entry is in `docs/decisions/open-questions/EXTRACT.md` with `Status: approved (recommendation drafted 2026-11-20 in the M7 plan, approved 2026-11-21)` and a `Resolution: fix` line. Under X-2 tier 1 the decision is a both-modes fix: the truncated `.ext` keeps a valid timestamp, and the truncated `.res.ext` is newer than its `.ext`, so the loss is silent and every netlist read from them is wrong.

## Entries approved 2026-11-22

Raised by the R1 workstreams and the R1 integration for Magic behaviours no row covered, and for plan items that needed the user's acceptance. **All were approved by the user on 2026-11-22, each with its recommended option.** The open-questions entries carry `Status: approved 2026-11-22` and a `Resolution:` line; the plan items are recorded in `openspec/changes/m7-extraction/design.md` and `tasks.md`.

### Behaviour questions

| ID | Question | Kind | Resolution | Spec | Implemented by |
|---|---|---|---|---|---|
| Q-EXTRACT-28 (M7-1) | `extract list`/`listall` with nothing after it crashes | **fix** | both modes: `Wrong number of arguments in "extract" command.  Try "extract help" for help.` | extract-commands "Command syntax and option matching" | scaffold (R1); test retagged in R2 |
| Q-NETLIST-49 (M7-2) | `ext2sim run` becomes the cell name | **compat** | as Q-NETLIST-13: ON `run` and an ambiguous word are run arguments (the word is the cell name); OFF `run` is a keyword and an ambiguous prefix prints `Ambiguous option: <w>` | netlist-sim "Command options and defaults" | scaffold (`netlist/first_word.cpp`); SIM tests |
| Q-MISC-91 (M7-2) | `antennacheck run` becomes the cell name | **compat** | as Q-NETLIST-13; the antenna-check prefix match holds with magic-compat OFF only | antenna-check "Antenna check command" | scaffold; SIM's tagged tests |
| Q-NETLIST-51 (M7-4) | `device vsource` reads an uninitialised token count | **fix** | both modes: parse with `ac = 7` | ext-flattening-read "Device records" | EXTFLAT (task 10.1) |
| Q-NETLIST-50 (M7-5) | standalone extcheck dereferences an unset extraction style on a `scale` line | **fix** | both modes: `hefesto-extcheck` reads the record with the reader's own unit and never crashes | netlist-extcheck "Invocation" | SIM (task 12.4) |
| Q-EXTRACT-29 (M7-6) | TiSplitX on a split (diagonal) tile moves its triangle | **compat** | ON: copy Magic's cut; OFF: refuse. `StitchEditor::split_x` refuses in both modes today; the ON branch lands with RESNET (R4) | none (db-internal; design D6, D16) | RESNET with the MODEL review (R4) |
| Q-NETLIST-53 (M7-7) | ResFracture on a split (diagonal) tile | **compat** | ON: cut it as ResSplitX does (corrected 2026-11-25: TiSplitX keeps the left part's diagonal, the new part gets the left type, the joins take split tiles; the approved text said "make it Manhattan first", which is not Magic); OFF: keep it whole | extresist-network "Tile fracturing" | RESNET (task 13.1) |
| Q-NETLIST-52 (M7-8) | ext2sim reuses a closed `.al`/`.nodes` stream after `alias off`/`labels off` (double free) | **fix** | both modes: a run writes `.al`/`.nodes` only when its option is on and never touches a previous run's stream | netlist-sim "Command options and defaults" | SIM (task 12.1) |

### Plan items

| Item | Question | Resolution |
|---|---|---|
| M7-3 | M4's partial `ext2spice` (PDKINT, `script/pdk/commands.cpp`) and M7's command | M4's command is folded into M7's `ext2spice`; the `scale` option is kept; SPICE moves `scale` into `SpiceSettings` in round R2 (design "Differences from Area contracts", M4's `ext2spice`) |
| M7-9 | Magic's standalone `extcheck` cannot be built at fc5daa9e (golden-capture runs 44961645329, 44964258826: `build=unavailable`) | design R7 option (a): NL-CHK-01's expected output is hand-derived from extcheck/extcheck.c and extflat/EFargs.c (one case, four runs, every line traced to a source line), recorded as hand-derived under this decision in the case (`hand_derived`, provenance; never captured or verified against Magic) and replayed against `hefesto-extcheck` with the new `extcheck-console` comparator (no deviation entry); the `netlist-extcheck` spec is kept; the usage lines compare byte for byte, and the `<prog>` mask applies only to the `<argv[0]>:` prefix `hefesto-extcheck` puts before a diagnostic of its own (design D15, D21) |
| M7-10 | Magic writes flat coupling `cap` lines in address order when the substrate is named | the `spice-cap` comparator with a substrate node list (`spice-cap:VSUB`, `spice-cap:VSUB,c2/VSUB`), as landed in R1 (design D21) |
| M7-11 | The synthetic tech additions of R1's golden authoring | accepted: `exttest.tech` gains `ndc`, `ndiffh`, `thkox` and the seventh plane `oxide`, and the CIF output scale is 0.5 um (design D21) |
| M7-12 | The tile client slot's memory cost on iPad (task 3.4) | the analytical bound of about +240 MB for a 10M-rectangle layout held flat is accepted (under the 3 GB iPad limit, `hefesto-performance` "Memory bound on iPad"); the perf gates passed (dispatch run 44965550726), so task 3.4 is done |
| M7-13 | The editor-side extract-style hook needs one call in `tech/` | design D20 is amended: `tech/commands.cpp` (`install_technology`) and `tech/scale_layout.cpp` (`scale_layout`) each gain one call line and one include, calling `editor/extract_hook.cpp`; this is no longer a difference from the plan |

### Spec changes the answers imply

Applied 2026-11-22, after the approval:
- **Q-EXTRACT-28:** `extract-commands` "Command syntax and option matching" states the both-modes message for a bare `list`/`listall`, with a scenario.
- **Q-NETLIST-49 and Q-NETLIST-52:** `netlist-sim` "Command options and defaults" gives the first-word rule per mode and the both-modes file rule, with a scenario per mode and one for `alias off` after a run.
- **Q-MISC-91:** `antenna-check` "Antenna check command" says that the unique-prefix match is the magic-compat OFF behaviour, with a scenario per mode.
- **Q-NETLIST-50:** `netlist-extcheck` "Invocation" states the `scale` rule, with a scenario.
- **Q-NETLIST-51:** `ext-flattening-read` "Device records" gives `vsource` its token count, with a scenario.
- **Q-NETLIST-53:** `extresist-network` "Tile fracturing" gives the split-tile rule per mode, with a scenario per mode.
- **M7-3:** `netlist-spice-options` "Command options" says the `scale` row is the setting PDK startup files set with `ext2spice scale`; "Run arguments and output file" says which run flags persist (added at integration).
- Q-EXTRACT-29 is a tile-database rule with no capability text; design D6 and D16 carry it.

## Entries approved 2026-11-23

Raised by the R2 workstreams and the R2 review for Magic behaviours no row covered, for a both-modes fix made by the standing rule, and for a plan item. **All were approved by the user on 2026-11-23, each with its recommended option.** The open-questions entries carry `Status: approved 2026-11-23` and a `Resolution:` line; the plan item is recorded in `openspec/changes/m7-extraction/design.md` and `tasks.md`.

### Behaviour questions

| ID | Question | Kind | Resolution | Spec | Implemented by |
|---|---|---|---|---|---|
| Q-NETLIST-54 (M7-14) | `ext2spice` and `ext2sim` run flags are Magic globals that persist across runs | **compat** | one rule for both tools: ON `-M -m -f -B -d -J` (`ext2spice`) and `-A -B -F -L -M -m -f -J` (`ext2sim`) hold for later runs; OFF they apply to one run, as Q-NETLIST-16 and Q-NETLIST-42 | netlist-spice-options "Run arguments and output file"; netlist-sim "Command options and defaults" | SPICE (`netlist/spice/run.cpp`, R3); SIM (R2, retagged in R3); one test per mode per tool |
| Q-EXTRACT-30 (M7-15) | Out-of-range and NaN float-to-int conversions in the extract section are undefined behaviour | **fix** | both modes: saturate to the int range, NaN as 0 (confirmed as implemented in R2) | extract-tech-section "Step size and side halo parsing" | EXTTECH (R2) |
| Q-NETLIST-55 (M7-16) | `-h` runs the tool's own flag parser before the common usage text | **compat-neutral** | both modes as Magic: the tool's parser first (`ext2spice` usage; `ext2sim` `Unrecognized flag: -h` and usage), then the common usage text, and the command fails | ext-flattening-hierarchy "Common netlister arguments" | EXTFLAT, SPICE, SIM (R2) |
| Q-NETLIST-56 (M7-16) | `-o` only for `ext2spice`/`ext2sim`, `-T` only for the standalone program | **compat-neutral** | both modes as Magic: `hefesto-extcheck -o` is a usage error; `ext2spice`/`ext2sim -T` is `Unrecognized flag: -T` | ext-flattening-hierarchy "Common netlister arguments" | EXTFLAT, SIM (R2) |
| Q-NETLIST-57 (M7-16) | EFArgs prints its own messages while parsing | **compat-neutral** | both modes as Magic: second root cell, missing flag value, watched nodes, symbol and symbol-file messages printed before the run | ext-flattening-hierarchy "Common netlister arguments" | EXTFLAT (R2) |
| Q-NETLIST-58 (M7-17) | An unreadable `-S` file is reported with the OS error text in Magic | **compat-neutral** (accepted difference) | both modes: `<file>: No such file or directory` for any unreadable file, the parse goes on; the OS text through `FileSystem::read` is a later improvement (tasks.md 10.6) | ext-flattening-hierarchy "Common netlister arguments" | EXTFLAT (R2) |

### Plan items

| Item | Question | Resolution |
|---|---|---|
| M7-18 | How the M6-S8 instance cap (`extflat::ReadLimits::max_instances`, 4 Mi flattened instances; decision G5 of 2026-11-21) counts when a netlister visits the hierarchy several times | per pass: each traversal of the hierarchy (node flattening, kills, couplings, device, resistor and subcircuit visits) counts its instances and expanded merge elements afresh, so a design of exactly 4 Mi instances flattens however many passes the netlister makes. Wording only (as implemented in R2, `Flattener::begin_pass`): `docs/contributing/security.md` M6-S8 and `docs/decisions/m6-decision-table.md` (G5's `ReadLimits` note); test `tests/unit/extflat/hardening_test.cpp` "a hierarchy with exactly max_instances instances flattens" |

### Spec changes the answers imply

Applied 2026-11-23, after the approval:
- **Q-NETLIST-54:** `netlist-spice-options` "Run arguments and output file" gives the persistence rule per mode, with a scenario per mode (the scenario "Run flags that persist" becomes the magic-compat ON one); `netlist-sim` "Command options and defaults" cites the row and drops the R2 note that the rule was open, with a scenario per mode.
- **Q-NETLIST-55, 56, 57, 58:** `ext-flattening-hierarchy` "Common netlister arguments" already states the behaviour with its scenarios; it now cites the rows.
- Q-EXTRACT-30 and M7-18 need no spec change.

## Entries approved 2026-11-25

Raised by the R3 workstreams (EXTNODE, EXTCAP, EXTDEV, NETLIST), the R3 fix pass and the R4 workstreams (EXTCMD, EXTHIER, RESNET, RESCMD, ANTENNA) for Magic behaviours no row covered and for both-modes fixes made by the standing rule (the R4 rows were numbered at integration: the workstreams' Q-EXTRACT-37/38 collided with the withdrawn Q-EXTRACT-37 and with each other). **All were approved by the user on 2026-11-25, each with its recommended option**, and recorded in round R5 (branch `m7-round5-decide-integ`). The open-questions entries carry `Status: approved 2026-11-25` and a `Resolution:` line; Q-EXTRACT-37 stays withdrawn (`Resolution:` none needed, no difference). Every row is implemented as its resolution says; the last two land in round R5: Q-NETLIST-60 (code in m7-round5-netlist, which completes Q-NETLIST-31's OFF branch) and Q-NETLIST-63 (m7-round5-res located Magic's crash in ResReadDevice and tests the both-modes behaviour).

### Standing rule for crafted-input resource limits (2026-11-25)

The user approved on 2026-11-25, as a standing rule: **a new resource limit on crafted input, applied in both `magic-compat` modes, is approved with its recommended value.** Such a limit changes behaviour only on inputs no real design produces (Magic overflows, allocates without bound or loops on them). Each limit is still recorded here as approved and reported to the user when it is added, with its value, its diagnostic and its test. Limits recorded under this rule:

| Limit | Value | Where | Diagnostic | Recorded |
|---|---|---|---|---|
| `.ext` port number (Q-NETLIST-62) | 2^20 = 1048576 | `extflat::ReadLimits::max_port_index` | EXTFLAT-READ-009 | R3 fix pass; approved 2026-11-25 |
| `fuzz_extract_section` input size | 64 KiB per input, as `fuzz_tech_file` | `tests/fuzz/fuzz_extract_section.cpp` | input ignored (harness only) | R5 (task 19.2) |
| `fuzz_ext_netlist` input size and flattening | 64 KiB per input; `ReadLimits` with 4096 instances, 64 Ki nodes, depth 16 | `tests/fuzz/fuzz_ext_netlist.cpp` | the read fails with the reader's own EXTFLAT-READ code (harness only) | R5 (task 19.2) |
| `fuzz_res_ext_reader` input size | 64 KiB per input | `tests/fuzz/fuzz_res_ext_reader.cpp` | input ignored (harness only) | R5 (task 19.2) |
| `fuzz_extract_layout` layout size | at most 64 rectangles and 8 labels per cell, coordinates within -128..159 lambda | `tests/fuzz/fuzz_extract_layout.cpp` | the decoder stops (harness only) | R5 (task 19.2) |
| `fuzz_ext_reader` flattening (lowered) | `ReadLimits` of 2^14 instances, 2^16 flat names and 2^18 merge elements (were 2^18, 2^20, 2^20): a 100001-element array of a two-node leaf, inside the old caps, flattened linearly but took over 10 s under ASan (CI run 45871540469) | `tests/fuzz/fuzz_ext_reader.cpp` | the read fails with the reader's own EXTFLAT-READ code (harness only); seed `regress-timeout-bcf9c071…` | R5 (task 19.2) |

The fuzz harness caps are harness parameters, not product limits: they bound what one fuzz input can build so that a timeout is a real hang (design D22), and change nothing a user sees. They are recorded here so that every cap added in M7 is listed in one place.

### Behaviour questions (raised in round R3)

| ID | Question | Kind | Resolution | Spec | Implemented by |
|---|---|---|---|---|---|
| Q-EXTRACT-31 | The second side of a two-sided split tile is never a region seed | **compat** | ON: skipped as Magic (EX-P02); OFF: a region starts at any unvisited side | extract-nodes-and-labels "Node connectivity" (scenario per mode) | EXTNODE (R3) |
| Q-EXTRACT-32 | Per-class area and perimeter stored only with lumped resistance on | **compat** | ON: 0 unless lumped resistance is on; OFF: always stored | extract-resistance-lumped "Per-class area and perimeter of a node" (scenario per mode) | EXTCAP (R3) |
| Q-EXTRACT-33 | Corner coupling and fringe-halo shield pieces follow Q-EXTRACT-17's quirk | **compat** | Q-EXTRACT-17's rule for corners and for the shield pieces' coupling fraction | extract-capacitance "Corner fringe coupling" (scenario per mode), "Fringe-halo sideoverlap" | EXTCAP (R3) |
| Q-EXTRACT-34 | A parameter range with only a lower limit is never checked | **compat** | ON: skipped (EX-037 both `nlong`); OFF: the lower limit applies | extract-devices "Model selection by parameter ranges" (scenario per mode) | EXTDEV (R3) |
| Q-EXTRACT-35 | Range matching falls back to the last record tried | **compat** | ON: last record tried; OFF: the starting record | extract-devices "Model selection by parameter ranges" (scenario per mode) | EXTDEV (R3) |
| Q-EXTRACT-36 | Crashes, hangs and undefined behaviour in the node, capacitance and device passes (nine cases) | **fix** (both modes) | (a)-(i) as implemented; m7-round5-extract adds regression tests for (f), (h), (i) where an input reaches them | none | EXTNODE, EXTCAP, EXTDEV (R3); tests for (a)-(e), (g) in the R3 fix pass |
| Q-EXTRACT-37 | The shorted-ports warning truncates the cell's port labels | withdrawn | none needed: Magic restores the cut names (ExtBasic.c:846-847) | none | EXTNODE, EXTCAP (R3) |
| Q-NETLIST-59 | A missing `.ext` file is the command's result and the command succeeds | **compat** (both tools) | ON: the text is the result and the command succeeds; OFF: EXTFLAT-READ-001 | netlist-spice-options, netlist-sim (scenario per mode) | NETLIST (R3 ext2spice, R3 fix pass ext2sim) |
| Q-NETLIST-60 | The name of a shorted-port device's anonymous terminal-0 node under Q-NETLIST-31 OFF | compat-neutral | `dummy#<n>` numbered from 0 per cell; completes Q-NETLIST-31's OFF branch | netlist-spice-devices "Resistor lines" (sentence, scenario per mode) | m7-round5-netlist (R5) |
| Q-NETLIST-61 | The `-p` search path holds for one run in both netlisters | **compat** | one run in both tools; ON keeps the path of an `ext2spice` run that names no cell, OFF drops it | netlist-spice-options, netlist-sim (cited, scenarios per mode) | R3 fix pass |
| Q-NETLIST-62 | A cap on `.ext` port numbers (ReadLimits::max_port_index, 2^20) | **fix** (both modes; crafted-input limit) | EXTFLAT-READ-009 above 2^20; sparse SPICE port lists | ext-flattening-read "Use, port, cap, resist, attr and connect records" (cited) | R3 fix pass |

### Behaviour questions (raised in round R4)

| ID | Question | Kind | Resolution | Spec | Implemented by |
|---|---|---|---|---|---|
| Q-EXTRACT-38 | `extract do\|no unique <word>` with a word other than `notopports` applies DOUNIQUE as a bit mask | **compat** | ON: usage line and Magic's bit-mask side effect; OFF: usage line only | extract-commands "Extraction options" (scenario per mode) | EXTCMD (R4) |
| Q-EXTRACT-39 | The line of a non-primitive `device` cell property has no newline | **compat** | ON: no newline (EX-042); OFF: newline | extract-devices "Device cell property" (scenario per mode) | EXTCMD (R4) |
| Q-EXTRACT-40 | Unifying two hierarchical nodes drops one node's pass-2 adjustments | **compat** | ON: survivor's values only; OFF: summed | extract-hierarchy "Merge records and pass-2 adjustments" (the as-landed note is the rule; scenarios per mode) | EXTHIER (R4) |
| Q-EXTRACT-41 | Two device types touching in one device region make one device (probe EX-P03) | **compat**, as implemented | ON: one device from the representative tile; OFF: one device per type (multi-type gate lines such as `nfet,scnfet` are split per type too) | extract-devices "Device regions" (rule and scenario per mode) | EXTHIER (R4) |
| Q-NETLIST-63 | Magic's `extresist` crashes (exit 139) on nets with nfets (NL-RESNET-05, 10, 13) | **fix** (both modes) | located: ResReadDevice adds the device to its substrate node without checking that the node exists (resis/ResReadExt.c:819-820); Hefesto creates the net in both modes; the three cases are re-authored with a `GND!` node record and replayed | extresist-command "Input model" (sentence and scenario) | RESNET (R4); m7-round5-res (R5: location, test, re-authored cases) |
| Q-NETLIST-64 | Undefined behaviour in the network construction (six cases) | **fix** (both modes) | (a)-(f) as implemented | extresist-network "Contact resistors" | RESNET (R4) |
| Q-NETLIST-65 | A negative `threshold`, `minresist` or `mindelay` is stored | **compat** | ON: stored; OFF: previous value kept | extresist-command "Command options" (scenario per mode) | RESCMD (R4) |
| Q-NETLIST-66 | `extresist debug` and `stats` take any word | **compat** | ON: any word turns them on; OFF: `Value must be either "on" or "off".` | extresist-command "Command options" (scenario per mode) | RESCMD (R4) |
| Q-NETLIST-67 | `extresist cell` runs on the box cell | **compat** | ON: box cell; OFF: selected cell | extresist-command "Run flow" (scenario per mode) | RESCMD (R4) |
| Q-NETLIST-68 | Crashes in the `.ext` read of `extresist` (three cases) | **fix** (both modes) | as implemented | extresist-command "Input model" | RESCMD (R4) |
| Q-NETLIST-69 | ResSortByGate orders by node address | **fix** (both modes; determinism) | first-mention order, stable sort | extresist-command "Net selection and first cutoff" | RESCMD (R4) |
| Q-MISC-92 | `antennacheck <cell>` with no layout window crashes | **fix** (both modes) | `Point to a window or specify a cell name.` | antenna-check "Antenna check command" | ANTENNA (R4) |
| Q-MISC-93 | antennacheck devices with no style device line or no terminals | **fix** (both modes) | the device is skipped | antenna-check "Antenna check command" | ANTENNA (R4) |

### Spec changes the answers imply

Applied 2026-11-26 (round R5), after the approval:
- **Q-NETLIST-59:** `netlist-spice-options` "Run arguments and output file" gains the missing-`.ext` rule per mode; `netlist-sim` "Command options and defaults" cites the row; a scenario per mode in each.
- **Q-NETLIST-60:** `netlist-spice-devices` "Resistor lines" names the OFF node `dummy#<n>`, with a tee-mode scenario per mode (Q-NETLIST-31's OFF branch is now specified).
- **Q-NETLIST-61:** cited in both specs; `netlist-spice-options` gains a scenario per mode for a run without a cell, `netlist-sim` one for the path left by `ext2spice`.
- **Q-NETLIST-62:** cited in `ext-flattening-read`, with the both-modes boundary in the scenario.
- **Q-EXTRACT-31:** `extract-nodes-and-labels` "Node connectivity" states the exception per mode, with a scenario per mode (EX-P02).
- **Q-EXTRACT-32:** `extract-resistance-lumped` "Per-class area and perimeter of a node": the sentence of the Recommendation replaces "These values SHALL be stored even when lumped resistance is disabled.", with a scenario per mode.
- **Q-EXTRACT-33:** `extract-capacitance` "Corner fringe coupling" gains "The shielded fractions follow Q-EXTRACT-17's resolution" and a scenario per mode; "Fringe-halo sideoverlap" states the coupling fraction per mode.
- **Q-EXTRACT-34, 35:** `extract-devices` "Model selection by parameter ranges" states both rules per mode, with two scenarios per row.
- **Q-EXTRACT-38:** `extract-commands` "Extraction options", one sentence and a scenario per mode.
- **Q-EXTRACT-39:** `extract-devices` "Device cell property", the rule and a scenario per mode.
- **Q-EXTRACT-40:** `extract-hierarchy` "Merge records and pass-2 adjustments": the as-landed note is the rule, with a scenario per mode through a later step square.
- **Q-EXTRACT-41:** `extract-devices` "Device regions" states the rule per mode; its scenario cites the row (one THEN per mode).
- **Q-NETLIST-64 to 69, Q-MISC-92, 93:** the as-landed text in `extresist-network`, `extresist-command` and `antenna-check` now cites each row; `extresist-command` gains a scenario per mode for Q-NETLIST-66 and Q-NETLIST-67.
- **Q-NETLIST-63:** `extresist-command` "Input model" gains the substrate-terminal sentence and the scenario "Substrate terminal on a net with no node record" (both modes).
- Q-EXTRACT-36 and Q-EXTRACT-37 need no spec change.

## Notable changes to the entries' original recommendations

- **Q-EXTRACT-1**: replicate → compat.
- **Q-EXTRACT-2**: replicate with a lint warning → replicate.
- **Q-EXTRACT-3**: replicate → compat.
- **Q-EXTRACT-4**: replicate → compat.
- **Q-EXTRACT-12**: behavioral, golden comparison within a tolerance → replicate.
- **Q-EXTRACT-17**: replicate → compat.
- **Q-EXTRACT-18**: replicate → compat.
- **Q-EXTRACT-19**: replicate → compat.
- **Q-EXTRACT-21**: replicate → compat.
- **Q-EXTRACT-22**: fix → compat.
- **Q-EXTRACT-24**: replicate → compat.
- **Q-MISC-4**: replicate by default with a fixed mode; the undefined-behaviour cases are now fixed in both modes → compat.
- **Q-NETLIST-4**: fix → compat.
- **Q-NETLIST-7**: replicate, decided with PDK users → compat.
- **Q-NETLIST-8**: replicate → compat.
- **Q-NETLIST-12**: replicate → compat.
- **Q-NETLIST-13**: fix → compat.
- **Q-NETLIST-15**: fix with a compatibility flag → compat.
- **Q-NETLIST-16**: fix → compat.
- **Q-NETLIST-17**: fix → compat.
- **Q-NETLIST-18**: fix → compat.
- **Q-NETLIST-19**: replicate per mode, perimeter factor fixed behind a flag → compat.
- **Q-NETLIST-20**: replicate → compat.
- **Q-NETLIST-25**: replicate → compat.
- **Q-NETLIST-28**: fix → replicate.
- **Q-NETLIST-29**: fix → compat.
- **Q-NETLIST-30**: replicate → compat.
- **Q-NETLIST-31**: replicate → compat.
- **Q-NETLIST-32**: replicate → compat.
- **Q-NETLIST-34**: replicate → compat.
- **Q-NETLIST-35**: replicate → compat.
- **Q-NETLIST-37**: replicate → compat.
- **Q-NETLIST-39**: fix → compat.
- **Q-NETLIST-40**: fix → compat.
- **Q-NETLIST-41**: fix → compat.
- **Q-NETLIST-43**: fix → compat.
- **Q-NETLIST-44**: replicate → compat.
- **Q-NETLIST-46**: fix → compat.

## Evidence corrections

27 entries had wrong line numbers or an incomplete or wrong description; each entry's `Evidence:` line now carries the corrected lines. The ones that change the substance:

- **Q-EXTRACT-2**: the "documented syntax" is Hefesto's spec table, not Magic's; sky130A-extract.tech:1027 depends on argument 5 being the substrate node name.
- **Q-EXTRACT-6**: wider than described: the top cell of `extract`/`extract cell` is never reverted, and the `_uqN` renames are permanent edits that a later save writes to the .mag file.
- **Q-NETLIST-12**: commit fc62a63d shows the +0.5 is a leftover integer-rounding term, so it is a bug; values are floats, so the entry's "round to integer" option is wrong.
- **Q-NETLIST-23**: a list or bad name runs `TTMaskZero(&ResCopyMask[j])` with j < 0, an out-of-bounds write.
- **Q-NETLIST-24**: the premise is wrong: gscap is per edge length and a gate has two diffusion edges, so `W+W` is intended.
- **Q-NETLIST-29**: `curnode` is uninitialised before the first `node` record, and `res:min=` overflows a 20-byte stack buffer.
- **Q-NETLIST-30**: the trim flags are defined as "delete trailing"; a leading comma in a root prefix writes before the output buffer.
- **Q-NETLIST-31**: the `dummy` node is shared by every short device in a cell and reaches the netlist in resistor tee mode, where it shorts separate shorted-port pairs together; pi mode moves its capacitance to the ports.
- **Q-NETLIST-34**: the missing separator can make two flat device names collide (`Mu11foo`).
- **Q-NETLIST-35**: the counter is reset per run, so it only continues across subcircuits within one run.
- **Q-NETLIST-37**: commit 9800d98d made the change on purpose; `descend` is inert in flat mode.
- **Q-NETLIST-39**: only resistor, capacitor and caprev records are affected; the resistor record is rejected on read, losing the overlay's rewiring.
- Line numbers only, or a minor addition: Q-EXTRACT-3, Q-EXTRACT-4, Q-EXTRACT-20, Q-EXTRACT-26, Q-NETLIST-9, Q-NETLIST-10, Q-NETLIST-15, Q-NETLIST-16, Q-NETLIST-17, Q-NETLIST-18, Q-NETLIST-19, Q-NETLIST-25, Q-NETLIST-28, Q-NETLIST-32, Q-NETLIST-41.

Entries whose evidence was right carry a `Verified 2026-11-13:` line instead. Notable additions:

- **Q-MISC-4**: with no CIF style or no squares/slots rule, the cut parameters are left uninitialised, and a zero pitch divides by zero.
- **Q-MISC-7**: a live PDK case: gf180mcuD's metal4 plane declares mimcc before via4, so only mimcc is restricted at the metal4 check.
- **Q-NETLIST-6**: the misindexing reads uninitialised or out-of-bounds slots (undefined behaviour), not just a wrong M.
- **Q-NETLIST-8**: an explicit `*1` reads the uninitialised offset.
- **Q-EXTRACT-25**: a grid rescale does not reload the style; it scales the overrides.
- **Q-NETLIST-46**: after the first removal the heap stays "built", so the first net of a session is ordered differently.
- PDK checks: Q-EXTRACT-1, Q-EXTRACT-3, Q-EXTRACT-19, Q-EXTRACT-21 and Q-MISC-1 do not affect any of the three PDKs in either mode.


## Spec changes after approval

Applied to `openspec/specs/*/spec.md` after the 2026-11-13 approval ("Align the M7 specs with the approved decisions"). Each spec got the text below, and each entry its `Resolution:` line. Items are listed under the first spec that cites them; the text names any other spec it touches.

### extract-tech-section

- **Q-EXTRACT-1**: extract-tech-section (line 255): replace 'choosing the comparison direction by comparing the plane *indices* of P1 and P2 (see Q-EXTRACT-1)' with: with magic-compat ON the direction is chosen by comparing the plane indices; with it OFF, by comparing plane order. Cite the resolution.
- **Q-EXTRACT-2**: extract-tech-section, class table (line 294): change the diode/pdiode/ndiode row to `OTHER [SUBNODE]`, adding 'substrate types = OTHER (none when OTHER is `None`)', and remove the Q-EXTRACT-2 pointer in favour of the resolution.
- **Q-EXTRACT-3**: extract-tech-section, line 374 (micron conversion) and line 397 (grid rescale): state that gscap is scaled by dscale² and n²/d² with magic-compat ON, and like perimeter caps (dscale, n/d) with it OFF. Replace the 'see Q-EXTRACT-3' references with the resolution.
- **Q-EXTRACT-19**: extract-tech-section, 'Step size and side halo parsing' (line 131): put the truncation under magic-compat ON and add the OFF rounding. Replace 'See Q-EXTRACT-19' with the resolution.
- **Q-EXTRACT-21**: extract-tech-section (line 305), and the rescale text at line 397 'parameter offsets and limits': put the /1000 truncation under magic-compat ON and add the OFF milli-unit comparison. Replace the pointer with the resolution.

### extract-nodes-and-labels

- **Q-EXTRACT-4**: extract-nodes-and-labels, line 91, consequence (b): qualify the attribute-first exception as magic-compat ON only; with it OFF, the first attached name label wins. Correct the Magic refs to ExtRegion.c:315-331 and :489-503.
- **Q-EXTRACT-6**: extract-nodes-and-labels, 'Temporary unique' scenario (line 180): drop '(see Q-EXTRACT-6 for an exception)' and state that every temporarily renamed cell, the top cell included, is reverted, without leaving the cell modified. Update the Magic refs to ExtMain.c:842-848 and :1108-1115. Add a regression scenario for a cell with no isolated substrate.
- **Q-EXTRACT-22**: extract-nodes-and-labels (line 139): state the suppression condition per mode. Replace 'See Q-EXTRACT-22' with the resolution.
- **Q-EXTRACT-23**: extract-nodes-and-labels, 'Unique naming of duplicate labels' (line 172): replace 'See Q-EXTRACT-23' with the resolution, noting that it depends on the node-list order fixed by Q-EXTRACT-5.

### extract-hierarchy

- **Q-EXTRACT-26**: extract-hierarchy, 'Sticky labels across the hierarchy' (line 149): replace 'See Q-EXTRACT-26' with the resolution and fix the Magic refs to ExtHier.c:521-575.

### extract-capacitance

- **Q-EXTRACT-17**: extract-capacitance (line 158): describe the ON accumulation (per rule, only between different nodes, unreduced inside type) and the OFF single-count with residue reduction. Replace 'See Q-EXTRACT-17' with the resolution and fix the refs to ExtCouple.c:1530-1580 and :688.
- **Q-EXTRACT-18**: extract-capacitance (line 144): give the ON formula MIN(accounted, whole-segment overlap) and the OFF one MIN(accounted-once, clipped length). Replace 'See Q-EXTRACT-18' with the resolution and fix the refs to ExtCouple.c:1677-1686 and :1780-1782.
- **Q-EXTRACT-20**: extract-capacitance (line 125): state that the offset is single-precision and that sep+offset is evaluated in float, and fix the ref to extractInt.h:476. Replace 'See Q-EXTRACT-20' with the resolution.
- **Q-EXTRACT-24**: extract-capacitance, 'Corner fringe capacitance to substrate' (line 61): state that with magic-compat ON concave means connecting types, as now, and with it OFF the same node plus a free quadrant that is space. Replace 'See Q-EXTRACT-24' with the resolution.

### extract-devices

- **Q-EXTRACT-16**: extract-devices, 'Device cell property' (line 289): say that D1 and D2 are written in property order (Magic's code reads them as width then length for mosfet, asymmetric, bjt and modelled resistor, and as area then perimeter for fet and modelled capacitor/caprev), with no reordering to L W; lengths, widths and perimeters scale by upl and areas by upl², truncated. State that after scaling urx is incremented when urx ≤ llx, and ury when ury ≤ lly. Replace 'See Q-EXTRACT-16' with the resolution and fix the refs to ExtBasic.c:353-556.

### extract-file-format

- **Q-EXTRACT-10**: extract-file-format, line 89: replace 'see Q-EXTRACT-10' with a fixed `version 8.3` (the format version Hefesto targets) and fix the ref to ExtCell.c:576. ext-flattening-read: the version warning compares against 8.3.

### extract-commands

- **Q-EXTRACT-11**: extract-commands, 'Output path and file location' (line 117): add a platform clause mapping the current directory to the project folder on sandboxed hosts, saying that directory creation goes through file coordination and that out-of-sandbox paths fail as unwritable. Replace the Q-EXTRACT-11 pointer with the resolution.
- **Q-EXTRACT-12**: extract-commands, 'Path length extraction' (line 141): change 'Compatibility: behavioral (see Q-EXTRACT-12)' to 'Compatibility: exact'. Specify the neighbour walk order and the depth-first marking; that marks are kept until every start tile under lab1 is done (ExtLength.c:647-674), so only first-arrival paths reach the destination and set min/max; the `CLIENTDEFAULT` test of the contact branch (:864); and the 1-unit halo search of other planes in the non-contact case (:878-905). The refs are ExtLength.c:640-1040.
- **Q-EXTRACT-25**: extract-commands, 'Style, halo and step size' (line 105): add that overrides survive a grid rescale (scaled) and are lost on style selection or reload. Replace 'See Q-EXTRACT-25' with the resolution.

### antenna-check

- **Q-MISC-1**: antenna-check (line 180): put 'neither sum is accumulated' under magic-compat ON and add the OFF cumulative accumulation. Replace 'see Q-MISC-1' with the resolution.
- **Q-MISC-3**: antenna-check (line 33): replace 'the calculation type is left undefined (see Q-MISC-3)' with 'the layer gets no calculation type and contributes no antenna area'.
- **Q-MISC-4**: antenna-check (line 149): add the both-mode rule for contacts with no cut rule (area 0) and a zero pitch. Put the current formula under magic-compat ON and add the OFF unit-consistent formula. Replace 'see Q-MISC-4' with the resolution and cite CIFtech.c:2339-2369.
- **Q-MISC-7**: antenna-check (line 127): put 'compares plane numbers, applies to one contact type only' under magic-compat ON and add the OFF rule (all contacts on the plane, plane order). Replace 'see Q-MISC-7' with the resolution.

### ext-flattening-read

- **Q-NETLIST-8**: ext-flattening-read (line 172): replace 'The offset is then left undefined; see Q-NETLIST-8' with 'the offset is 0'. netlist-spice-devices parameter text: state that the offset is applied only in the SI, scale-1 branch with magic-compat ON, and in all branches with it OFF.
- **Q-NETLIST-9**: ext-flattening-read, Requirement 'Device parameter declarations' (spec.md:176): keep the text. Add one sentence saying the .ext `parameters` line is already in tech-file order, so output is the reverse of the tech-file declaration order. Update the Magic ref to extflat/EFbuild.c:777-852, :982-985.
- **Q-NETLIST-12**: ext-flattening-read, Requirement 'Use, port, cap, resist, attr and connect records' (spec.md:226): give the `resist` bullet both modes (ON: +0.5; OFF: rscale×R). extresist-output, Requirement 'Consumption by netlisters' (spec.md:74-78): the same, and limit the '0.0005 Ω offset' sentence to magic-compat ON. Update the Magic ref to extflat/EFread.c:626-635.
- **Q-NETLIST-31**: ext-flattening-read, Requirement 'Equivalence, merge and kill records' (spec.md:199): give rule 3 both modes: ON: every short device in the def takes the shared terminal-0 node `dummy`; OFF: each short device gets its own anonymous terminal-0 node that no name lookup can reach. netlist-spice-devices: note that in resistor tee mode the terminal-0 node is the midpoint of the two halves, and in pi mode its capacitance moves to the ports. Add a golden scenario (tee on, `short resistor`, two shorted-port pairs in one cell) per mode. Update the Magic refs to extflat/EFbuild.c:638-648, :1817-1836.

### ext-flattening-names

- **Q-NETLIST-10**: ext-flattening-names, Requirement 'Name preference order' (spec.md:55-63): no behavioural change. Drop the open-question pointer once resolved and update the Magic ref to extflat/EFname.c:510-568, :590-607.
- **Q-NETLIST-30**: ext-flattening-names, leaf-conversion requirement (spec.md:118-129): change 'These deletions apply anywhere in the leaf' to ON: anywhere; OFF: `!`/`#` only as the last character. Add a scenario with `a!b`. Add a note to the prefix rule about the leading-comma guard. Update the Magic ref to ext2spice/ext2spice.c:4307-4375.

### netlist-spice-devices

- **Q-NETLIST-4**: netlist-spice-devices (line 134): replace 'Flat mode wrongly writes the resistance for c (Q-NETLIST-4)' with: with magic-compat ON, flat mode writes the resistance for `c`; with it OFF, it writes the capacitance.
- **Q-NETLIST-5**: netlist-spice-devices (line 179): replace 'In flat mode that second line is never written (Q-NETLIST-5)' with the per-mode rule.
- **Q-NETLIST-6**: netlist-spice-devices (line 209): replace 'devices with exactly one terminal desynchronize the multiplier list (Q-NETLIST-6)' with 'devices with exactly one terminal take part in no merge and are written with M = 1' (devices with no terminals are never written).
- **Q-NETLIST-7**: netlist-spice-devices (line 87): keep the current text under magic-compat ON and add 'with it OFF, the non-scale values are also divided by m'. Replace the Q-NETLIST-7 pointer with the resolution.
- **Q-NETLIST-19**: netlist-spice-devices, parameter table (spec.md:107): give the flat `p`/`p0` SI entry both modes (ON 1e-12; OFF 1e-6). netlist-spice-hierarchy (spec.md:100-106): add 'Modeled resistor in scale mode: ON `w=%d l=%d` truncated; OFF `%g`', and keep the |C| threshold and connectivity bullets as normative per-mode behaviour. Update the Magic refs to ext2spice.c:2286-2299, :4002, :4157-4161, :4180 and ext2hier.c:567-570, :1494, :1655-1660, :1673.
- **Q-NETLIST-32**: netlist-spice-devices (spec.md:118): change 'A blanked entry yields a lone space' to ON: lone space; OFF: nothing written. Update the Magic refs to ext2spice.c:2388, :2433, :2495-2497 and ext2hier.c:379, :424.
- **Q-NETLIST-34**: netlist-spice-devices (spec.md:23): change 'preceded by the raw `/`-joined instance path without a trailing separator' to ON: no separator; OFF: path + `/`. Add one scenario per mode. Update the Magic ref to ext2spice/ext2spice.c:2876-2881.

### netlist-spice-hierarchy

- **Q-NETLIST-20**: netlist-spice-hierarchy, Requirement on section body order (spec.md:62): change item 5 to ON: 'only if a substrate record was read'; OFF: 'always, to the substrate node or `0`'. Update the Magic ref to ext2spice/ext2hier.c:2431-2441.
- **Q-NETLIST-35**: netlist-spice-hierarchy, Counters bullet (spec.md:64): change 'The voltage-source counter is not reset' to ON: not reset per section (reset per run); OFF: reset with the others. Update the Magic refs to ext2hier.c:2453-2459 and ext2spice.c:828.

### netlist-spice-options

- **Q-NETLIST-13**: netlist-spice-options, Requirement 'Command options' (spec.md:17): replace 'If the first word is missing or does not match, all words SHALL be treated as run arguments' with the two-mode rule: ON as now; OFF `run` is a keyword and an ambiguous prefix is an error. Add one scenario per mode for `ext2spice run`.
- **Q-NETLIST-37**: netlist-spice-options, Requirement 'Flat netlist structure' (spec.md:158): change item 1 to ON: 'only `top on` writes a wrapper'; OFF: '`top on`, or `top auto` when the root has port records'. Add a sentence that `descend` has no effect in flat mode. Add one scenario per mode. Update the Magic refs to ext2spice.c:888, :1123-1145 and extflat/EFread.c:469-472.
- **Q-NETLIST-41**: netlist-spice-options, option table (spec.md:31): change the `subcircuits descend` row to ON: 'Giving auto sets top to auto'; OFF: 'auto is a usage error'. Update the Magic ref to ext2spice/ext2spice.c:629-646.

### netlist-sim

- **Q-NETLIST-15**: netlist-sim, Requirement 'File structure and order' (spec.md:57-58): give `nodeline` both forms (ON `name "0 "`; OFF `name " GND "`). Requirement 'Parasitic and node records' (spec.md:139): state both modes. Scenario 'Node capacitance line': add the OFF result `C out GND 5.0`. Update the Magic ref to ext2sim/ext2sim.c:1628-1680.
- **Q-NETLIST-16**: netlist-sim, Requirement 'Name conversion state' (spec.md:159-167): change 'SHALL reproduce this session-state dependence by default' to an ON/OFF statement and add an OFF scenario (after ext2spice, `vdd!` stays `vdd!`). ext-flattening-names: the same per-run rule for ext2spice `-t`. Update the Magic refs to ext2spice.c:1071-1075 and ext2sim.c:495.
- **Q-NETLIST-17**: netlist-sim, Requirement 'File structure and order' (spec.md:64): change 'Merging SHALL ignore device class and type' to ON: ignore; OFF: also require equal class and type. Update the Magic ref to ext2sim/ext2sim.c:1770-1858.
- **Q-NETLIST-18**: netlist-sim, Requirement 'File structure and order' (spec.md:51): change item 2 to ON: 'if cthresh was finite when the options were set'; OFF: 'if the run's cap threshold is finite'. Apply the same to resistor flattening. Update the Magic ref to ext2sim/ext2sim.c:675-677.
- **Q-NETLIST-48**: netlist-sim, Requirement 'Deprecated sim2simp filter' (spec.md:171-): no normative change. Mark the requirement as informative or remove it, and point to the `merge` option. Update the Magic ref to ext2sim/Makefile.in:20.

### extresist-command

- **Q-NETLIST-23**: extresist-command, option table and note (spec.md:16-36): change `skip <type>` to `skip <type>[,<type>…]` and remove the sentence 'skip accepts only a single type'. Add 'An unknown or ambiguous type reports an error and leaves the masks unchanged'. Update the Magic ref to resis/ResRex.c:572-588.
- **Q-NETLIST-28**: extresist-command, option table (spec.md:16): make `mindelay [v]` 'Query returns the value in ps (stored ×1e9 zs)'. Drop the informative note about the non-Tcl build, or keep it marked as informative. Update the Magic ref to resis/ResRex.c:371-390.
- **Q-NETLIST-29**: extresist-command, Requirement 'Input model' (spec.md:87): the `attr` bullet gets both modes. Add 'An attr before any node record is ignored with a warning; res:min values of any length are accepted.' Update the Magic ref to resis/ResReadExt.c:139, :204-210, :1000-1060.

### extresist-network

- **Q-NETLIST-24**: extresist-network, Requirement 'Elmore delay and pruning' (spec.md:541): write the term as `2·W·sdCap`, add 'sdCap is the per-length gate–diffusion capacitance (gscap)', and drop the Q-NETLIST-24 pointer. Update the Magic ref to resis/ResSimple.c:798-802.
- **Q-NETLIST-25**: extresist-network, path-resistance requirement (spec.md:505-518): mark the int truncation and the heap algorithm as magic-compat ON. Add OFF: float keys and node resistances, ties by insertion order. Update the Magic ref to resis/ResSimple.c:1083-1210.
- **Q-NETLIST-43**: extresist-network, near-device cutting pseudo-code (spec.md:330) and the scenario at :368: ON: discard; OFF: add the tile area to the breakpoint's node, then discard. Add an OFF scenario.
- **Q-NETLIST-44**: extresist-network, triangle(N) (spec.md:459-462): mark `+ 0.5` as magic-compat ON only and give the OFF formulas without it. Update the Magic ref to resis/ResMerge.c:630-641, :765-776.
- **Q-NETLIST-46**: extresist-network, heap 'Persistence' bullet (spec.md:516): mark it as magic-compat ON. Add OFF: per-net queue with an insertion-order tie-break. Add to the golden-harness notes that NL-RESNET captures run in a fresh session.

### extresist-output

- **Q-NETLIST-39**: extresist-output, device-record requirement (spec.md:37): change 'absent for other classes, which therefore carry no value or L/W fields' to ON: absent; OFF: resistor/capacitor/caprev get value or L W and their extractor terminal layout. Add a scenario for a resistor overlay in each mode. Update the Magic refs to resis/ResPrint.c:109-185 and extflat/EFbuild.c:1082-1110, :1160-1167.
- **Q-NETLIST-40**: extresist-output, FastHenry requirement (spec.md:104): change 'Unnamed nodes get ids from a per-net counter starting at 1' to ON: per net; OFF: per file. Apply the same to segment numbering. Update the Magic ref to resis/ResRex.c:1077, :1396.
