# M9 decision table: LVS, simulation and advanced analog simulation

M9 ("LVS, simulation and advanced analog simulation → Desktop Beta", `docs/roadmap.md`) cannot start until every open question its specs cite has a `Resolution:` line. This table lists what still has to be decided:

| Group | Count |
|---|---|
| Entries reported as blocking by `python3 scripts/check_milestone_readiness.py --milestone M9` (13 Q-LVS, 15 Q-SIM, 6 Q-SIMADV) | 34 |
| Entries resolved only in draft or in part: Q-SIM-1 (a draft that waits for D-28) and Q-SIMADV-5 (the open `mm_ok` part) | 2 |
| Audit decisions marked "blocks M9" in `docs/decisions/audit/DECISIONS.md` | 7 (D-22 to D-28) |
| New entries raised by the M9 plan (`openspec/changes/m9-lvs-simulation`): Q-LVS-30 to -36, Q-SIM-20 to -28, Q-SIMADV-13, Q-SCHEM-39 | 18 |
| Planning questions from the design's "Open Questions" | 4 |

**Status: drafted 2026-11-22. Nothing here is approved.** Each new entry has `Status: pending user decision` and no `Resolution:` line, and each existing entry keeps its current state. After the user decides, INTEGRATION records the answers (m9 tasks 1.1–1.3), and the readiness check must then report "ready".

**Verify rows at G0.** Four rows (Q-SIM-8, Q-SIMADV-3, -4, -9) are settled by reference runs that need the patched capture image, which is built only after G0. They are therefore approved at G0 as rules, recorded as conditional `Resolution:` lines ("verify: (a) if the D-22 run shows the later `.param` taking effect, otherwise (b)"), which the readiness check counts as resolved. Task 3.3 later adds each row's `Verified` line with the run id. A result that no branch of the approved rule covers goes back to the user before gate G2-SIM.

## Method and sources

No netgen or xschem source was read while drafting this table. For netgen, only the licence file and the public reference manual were fetched. The `Evidence:` lines of `LVS.md` cite netgen source; they are repeated only by entry ID and were not followed. Magic's source (read-only, fc5daa9e) was read for the parts of M9 that touch Magic: `getnode` naming, the netlist window and IRSIM bridge (both M12) and the Tcl glue. The ngspice licence and API documentation are public.

Evidence tags:
- **[E]**: the Hefesto entry in `docs/decisions/open-questions/`;
- **[S]**: a Hefesto capability spec;
- **[D]**: a Hefesto decision (`product.md`, `cross-cutting.md`, `audit/DECISIONS.md`, `roadmap.md`);
- **[code]**: Hefesto at origin/main 072f50c8;
- **[magic]**: Magic 8.3.694 at fc5daa9e;
- **[pdk]**: the pinned PDK trees;
- **[web]**: public documentation (netgen reference manual, ngspice `COPYING` and shared-library page).

## Recommendation kinds

The **Kind** column of the tables below is the kind of the recommendation, as X-2 (`docs/decisions/cross-cutting.md`) defines it, with `netgen-compat` (D-2, default ON) in place of `magic-compat` for the LVS entries. It is not the entry's own `- Kind:` line, which classifies the question (bug, ambiguity, doc-mismatch, platform). The two can differ: Q-LVS-36's entry is a `bug` whose recommendation is a `fix`; Q-SIM-23's entry is a `doc-mismatch` whose recommendation is a process choice (`other`).
- **replicate**: do what netgen does, in both modes.
- **compat**: replicate with `netgen-compat` ON, and use the corrected behaviour with it OFF.
- **fix**: corrected behaviour in both modes. This covers tier 1 (crashes, undefined behaviour, invalid output) and cases where netgen's output is unspecified.
- **platform**: a Hefesto platform decision (X-3, X-4).
- **verify**: settled by a reference run in the capture image (X-6), not by preference.
- **other**: a scope, process or schedule decision.

## Judgement items

Six items need the user's decision on product, legal, schedule or process grounds. Each lists the recommended option.

- **J-1 (Q-LVS-30), how the LVS clean room is applied.** The approved `hefesto-licensing` requirement "Two-team clean-room process for specs written from GPL source" already covers specs written from GPL source, which the `lvs-*` specs were, so a single team is not an option. (a) Apply it as M8 did: separate spec and implementation teams, a MODIFIED delta that adds the eight `lvs-*` capabilities, `LVS.md` and `netgen ref:` traces to the requirement, a roster rule that keeps netgen readers off LVSCMD and SCHEMSIM 16.3, a paraphrase review, `check_cleanroom.py` extended, Hefesto-authored golden inputs, and the counsel review joined to the Desktop 1.0 item. (b) would rewrite the specs from the public manual and black-box runs only.
- **J-2 (Q-SIM-20, Q-SIM-22, D-28 desktop part), the ngspice licence boundary on desktop.** (a) Build a shared `libngspice-hefesto` behind a separate adapter plugin, `hefesto-ngspice-adapter`, that in release builds only the worker executable `hefesto-simd` loads (a developer build may load it in the editor for debugging). The editor, the CLI and the core never link ngspice, and a CI check enforces this. On macOS `hefesto-simd` alone disables library validation, so that users can actually replace the library, and it runs sandboxed. The intended LGPL position (a replaceable library in a separate process, no relink kit) goes through D-28's legal review before the Desktop Beta; the review can change the link type without touching the editor.
- **J-3 (Q-SIM-26), when each linking question is settled.** D-28 is about the iPadOS app and already says "must be settled before M14", as the roadmap does; there is no conflict. (a) Decide the new desktop question (J-2) in R0 and review it before the Desktop Beta, and keep D-28's iPadOS choice before M14 after the same review. The roadmap and D-28 gain a sentence naming the desktop part.
- **J-4 (Q-SIM-27), the Desktop Beta sign-off.** (a) A beta flow for each of the three PDKs on Linux x86_64 and macOS arm64:
  - an inverter schematic `beta_inv.sch` drawn in Hefesto and simulated in a shared testbench with a fixed FO4 load;
  - a layout cell `beta_inv` drawn in Hefesto around the library inverter, clean under Hefesto's DRC with the PDK's Magic tech file (KLayout sign-off is not part of Beta), written to GDS;
  - extraction;
  - an LVS match against `beta_inv.sch`;
  - post-layout `rc` simulation of `beta_inv` in the same testbench, with `tpd` within 30 % of the schematic run.
  
  Each flow is an end-to-end CI case, followed by the user's sign-off with real input, as for Desktop Alpha.
- **J-5 (design Open Question 1), how spec changes are recorded.** (a) As spec deltas in the change, synced to the living specs once G0 passes. M7 and M8 edited the living specs after approval instead. Deltas let the recommendations be reviewed before anything is approved.
- **J-6 (design Open Question 2), when M9 starts.** (a) R0 and R1 (decisions, clean room, data sets, capture, engine build, scaffold, worker service) start when the plan is approved. R2–R3 (the LVS core and the simulation engine on SPICE and deck inputs) start in parallel with M7 and M8 implementation. R4 (the layout and schematic sources, cross-probing, schematic integration) starts after M7 and M8 exit.

## Decision table: the blocking entries

### LVS

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-LVS-1 | Legacy `.ext` reader drops modern device records | **other** | Keep `readnet ext` for scripts and replicate its gap. Offer no GUI entry. LVS layout input always goes through the internal `ext2spice lvs` path. | [E]; [S] `lvs-command` |
| Q-LVS-2 | Report order inherits netgen hash and partition order | **fix** | Use a deterministic order. The comparator treats the hash-order sections as sets (X-1). | [E]; [S] `lvs-reports` "Canonical golden comparison" |
| Q-LVS-4 | Hash collisions and pseudo-random labels | **fix** | Use a seeded deterministic generator, plus a structural-signature check that splits colliding classes. | [E] |
| Q-LVS-6 | `property add` single-word and `{key tol}` parsing | **replicate** + tier-1 fix | Copy the single-word rule. A tolerance that is not a number raises an error in both modes (it is uninitialised in netgen). | [E] |
| Q-LVS-10 | Property comparison failure reported as a property error | **replicate** | Keep netgen's codes. Hefesto's structured result also carries the detail `property-check-failed`. | [E] |
| Q-LVS-11 | Pre-match flattening heuristic | **replicate** (behavioural) | Match netgen's behaviour, validated by NG-HIER. Record any divergence as a new entry. | [E] |
| Q-LVS-13 | JSON file naming and escaping | **fix** | Use full JSON escaping and valid empty lists. Keep netgen's naming when the name has an extension, and append `.json` otherwise. | [E] |
| Q-LVS-14 | `.include` home-directory expansion | **platform** | On iPadOS, `~` maps to the user's Hefesto documents root and `~user` is rejected. Desktop keeps netgen's behaviour. | [E]; [D] X-4 |
| Q-LVS-17 | Pin-match results −1 and −2 are never produced | **replicate** | Document them as dead in `lvs-hierarchy`. | [E] |
| Q-LVS-19 | List and JSON values for value-type and expression properties | **fix** | Write the numeric value, or `(unresolved expression)`. Only structured output changes. | [E] |
| Q-LVS-21 | Extra `.` line after a unique match | **replicate** | Keep the exact format. | [E] |
| Q-LVS-26 | No-connect pins merged as one net for parallel detection | **replicate** | This is intended behaviour, and `property parallel connected` turns it off. | [E] |
| Q-LVS-29 | `equate classes -unique` uses a random suffix | **fix** | Use a deterministic unique identity from the seeded generator of Q-LVS-4. | [E] |

### Simulation engine

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-SIM-1 | LGPL components linked into the iPadOS app | **platform** | Confirm the draft through J-3. The iPad answer comes from D-28 before M14. The GPL `table` code models are excluded in every case. | [E]; [D] D-28 |
| Q-SIM-3 | XSPICE code models without `dlopen` | **platform** | (a) A Hefesto patch links the `.cm` sources statically and registers them at `ngSpice_Init`, on every platform. Verify that no supported PDK model needs `table.cm`. | [E] |
| Q-SIM-4 | Crash containment on iPadOS | **platform** | (a) In-process, with a mandatory autosave before each run (M14). A remote engine can come later. | [E] |
| Q-SIM-5 | One instance per process; parallelism | **platform** | (a) Worker processes on desktop and serial runs on iPadOS. The pool size follows Q-SIMADV-13. | [E] |
| Q-SIM-6 | ngspice lowercases names | **fix** (as specified) | Rename on collision to `<name>__c<k>`. Confirm vector naming with SIM-NET-03 and SIM-UI-04. | [E] |
| Q-SIM-7 | ngspice version pin and the CMake build | **other** | Pin ngspice-47 with Hefesto's own CMake wrapper. Checked 2026-11-22: the ngspice-47 tag (`a80f6e3e`) has no `cmake/` directory or top-level `CMakeLists.txt`, and no later release exists, so the choice is not conditional. Move to the first release that ships the upstream CMake build when one exists. Carry the Hefesto patches and offer them upstream. The golden pin (`7aa47164`) and roadmap principle 2 move to the same release (Q-SIM-23), and the SIM spec team re-checks every `(ngspice ref: ...)` trace against it (task 1.4). | [E]; [code] `tools/golden/pins.toml:46-52`; [web] ngspice git tag `ngspice-47` |
| Q-SIM-8 | PDK model descriptors need verification | **verify** | Approve at G0 as a rule: the descriptors are the ones the entry lists, each confirmed or corrected by the D-26 run against the pinned PDK sets (including the sg13g2 HBT, RES and CAP sections); sky130 sets `ng_nomodcheck` only if the run without it fails on `combined`. `Verified` line at G2-SIM. | [E]; [D] D-26 |
| Q-SIM-9 | `.control` sections and the `shell` command | **platform** | Keep the spec's behaviour: strip `.control` by default and block dangerous commands. The iPad subset (`let`, `meas`, `print`, `alter`) is reviewed against App Store guideline 2.5.2 in M14. | [E] |
| Q-SIM-10 | Cross-platform numeric reproducibility | **platform** | Build with `-ffp-contract=off` everywhere and keep tolerance-based golden comparison. Re-tune the tolerances with SIM-XP. | [E] |
| Q-SIM-11 | Recovery after an ngspice error | **platform** | Always replace the worker after any fault on desktop. On iPadOS, use `ngSpice_Reset` with fuzz tests (D-25). | [E]; [D] D-25 |
| Q-SIM-12 | OpenMP on Apple platforms | **platform** | Off on iPadOS. Measure on macOS before bundling libomp. Off on every platform in M9. | [E] |
| Q-SIM-14 | Progress tracks only two thread identities | **other** (as specified) | Combine the status strings with the streamed time point. Check the third run with SIM-RUN-03. | [E] |
| Q-SIM-15 | Callback threads and print latency | **other** (as specified) | Keep the default `low_latency` build. Handlers copy their data and never block. | [E] |
| Q-SIM-16 | Default result memory budgets | **platform** | Keep 512 MiB on iPadOS and 4 GiB on desktop. Revisit them after the `sim-bench` profiles. | [E]; [S] `hefesto-performance` |
| Q-SIM-17 | Raw header fields are not reproducible | **fix** | Golden comparisons ignore `Date:` and `Command:`. Data is always little-endian. | [E] |
| Q-SIM-19 | Background execution on iPadOS | **platform** | Pause and resume as specified, with an in-app notice (M14). | [E] |

### Advanced analog simulation

| ID | Question | Kind | Recommendation | Evidence |
|---|---|---|---|---|
| Q-SIMADV-3 | sky130 Monte Carlo switches | **verify** | Approve at G0 as a rule: (a) if the D-22 run shows a later `.param MC_MM_SWITCH=1` taking effect, otherwise (b), a Hefesto-side section. Do not expose the variation-scaling factors in M9. `Verified` line at G2-SIM. | [E]; [D] D-22 |
| Q-SIMADV-4 | gf180mcu statistical switches and defaults | **verify** | Approve at G0 as a rule: explicit `.param` overrides if D-22 shows that the later definition wins, otherwise a Hefesto-side section as in Q-SIMADV-3 (b); `ngbehavior` per D-27. Report the comment mismatch upstream. `Verified` line at G2-SIM. | [E]; [D] D-22, D-27 |
| Q-SIMADV-5 (rest) | `mm_ok=1` for extracted sg13g2 devices | **fix** | (a) Simulation preparation adds `mm_ok=1` when the Monte Carlo mode includes mismatch, inside the deck only (D-23). `ext2spice` output does not change. | [E]; [D] D-23 |
| Q-SIMADV-8 | Monte Carlo around non-typical corners | **other** | (a) Reject `process` and `both` at non-typical corners unless the descriptor names a statistical counterpart. Evaluate (b) with the PDK maintainers later. | [E] |
| Q-SIMADV-9 | Overriding parameters in point decks | **verify** | Approve at G0 as a rule: if the D-22 run shows that the last definition wins, append override lines after the last model line (b); otherwise change only top-level text and report "cannot sweep" (a). `Verified` line at G2-SIM. | [E]; [D] D-22 |
| Q-SIMADV-10 | Hefesto measurements against ngspice `.meas` | **other** | (a) Hefesto's value is authoritative. (b) Debug builds also capture ngspice's value and warn on a difference (SIMADV-MEAS-04). | [E] |
| Q-SIMADV-11 | Waveform retention and stale measurements | **other** | Add the `measured` mode now. It becomes the default above 100 points only after `sim-bench` has measured its storage cost and the user approves (D-24). | [E]; [D] D-24 |

## Audit decisions D-22 to D-28

| Decision | Question | Recommendation | How it closes |
|---|---|---|---|
| D-22 | Does a later top-level `.param` override an earlier one in the pinned ngspice? | Approve the conditional rules of Q-SIMADV-3, -4, -9 at G0; run one reference deck per PDK before G2-SIM (task 3.3). | `Verified` lines under Q-SIMADV-3, -4, -9 |
| D-23 | sg13g2 `mm_ok=1` for extracted devices | (a) Simulation preparation adds it. | the `simulation-netlist-preparation` delta "sg13g2 mismatch flag on extracted devices" |
| D-24 | `measured` retention mode | Add it. It becomes the default only after measurement. | the `simulation-sweeps` delta "Measured waveform retention"; task 19.2 |
| D-25 | Engine recovery after a fault | Replace the worker on desktop. Reset on iPadOS, with fuzz tests (M14). | the `simulation-engine-embedding` deltas "Crash containment on desktop" and "Cancellation and fault states" |
| D-26 | sky130 descriptor: `combined`, `ng_nomodcheck` | Scan `combined`. Decide `ng_nomodcheck` with a reference run. | task 3.3; Q-SIM-8 |
| D-27 | gf180mcu `hs` or `hsa` | Follow the PDK (`hs`) unless a reference run shows a difference that `hsa` fixes. | task 3.3 |
| D-28 | Linking ngspice's LGPL and MPL parts | D-28's own recommendation is a legal review first. The new desktop question is J-2 (a shared library behind the adapter plugin, loaded only by `hefesto-simd` in release builds), reviewed before the Desktop Beta. The iPad part (a framework or static linking) stays before M14, after the same review (J-3). | Q-SIM-20, Q-SIM-26; the `hefesto-licensing` delta; task 20.1 |

Two related audit decisions are already resolved and are used as they stand: D-10 (native undo events for `sim` testbench edits) and D-34 (the `netgen` helper only names `lvs` in a diagnostic).

## Entries raised by the M9 plan

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-LVS-30 | How the approved two-team process applies to the `lvs-*` specs | **other** | (a) The M8 process, a MODIFIED delta of the requirement and a roster rule (J-1) |
| Q-LVS-31 | The IHP setup file is `ihp-sg13g2_setup.tcl` | **fix** (doc) | (a) Correct the texts and add the bare-name scenario |
| Q-LVS-32 | `lvs -layout` extracts incrementally | **other** | (a) `ensure_extracted` (M7 D18) |
| Q-LVS-33 | Name map from netlists back to layout and schematic objects | **platform** | (a) Additive `render_with_map` and `NetlistResult::names` |
| Q-LVS-34 | LVS threading | **platform** | (a) Single-threaded per pair, with no globals |
| Q-LVS-35 | LVS performance target | **platform** | (a) Callgrind counts ≤ netgen, then baseline + 2 % |
| Q-LVS-36 | LVS input and run limits | **fix** | (c) Configurable limits with the proposed defaults |
| Q-SIM-20 | How `hefesto-simd` links ngspice on desktop | **platform** | (a) Shared `libngspice-hefesto` behind the adapter plugin, loaded only by the worker in release builds; replaceable on macOS; legal review before Beta (J-2) |
| Q-SIM-21 | Worker IPC transport and platform contract | **platform** | (a) `platform::WorkerHost`, framed pipes, heartbeats |
| Q-SIM-22 | In-process simulation on desktop | **platform** | (a) Developer builds only, through the same adapter plugin |
| Q-SIM-23 | Reference ngspice build for SIM and SIMADV captures | **other** | (a) Patched build from the engine's pin, with OpenVAF |
| Q-SIM-24 | Node names for layout probes before M12 | **platform** | (a) Bring the naming function forward, with no length limit; `getnode` stays M12 |
| Q-SIM-25 | Simulation input and transport limits | **fix** | (a) Configurable limits with the proposed defaults |
| Q-SIM-26 | When D-28 must be settled | **other** | (a) Desktop question in R0 and reviewed before Beta; iPad part before M14, as D-28 says (J-3) |
| Q-SIM-27 | Desktop Beta sign-off | **other** | (a) Three PDK flows with Hefesto-drawn schematic and layout, Hefesto DRC, FO4 testbench; CI plus the user's sign-off (J-4) |
| Q-SIM-28 | Simulation performance gate | **platform** | (a) Overhead ≤ 10 % against ngspice's `rusage` analysis time; Monte Carlo scaling ≥ 0.6 × `w` for `w` up to `min(4, cores − 1)` of the runner |
| Q-SIMADV-13 | Worker pool size and result order | **platform** | (a) A memory-aware pool, results in point-index order |
| Q-SCHEM-39 | SCHEM-LVS-01 needs the LVS engine | **other** | (a) Defer to M9, as SCHEM-SIM-01/02 were |

The full options and reasons are in each entry.

## Planning questions raised by the M9 plan

These come from `openspec/changes/m9-lvs-simulation/design.md` "Open Questions". Options are listed with the recommended one first.

| # | Question | Options (recommended first) | Shapes |
|---|---|---|---|
| 1 | How spec changes are recorded | (a) the change's `specs/` deltas, synced at G0; (b) direct edits after approval with `skip_specs: true` (the M7 and M8 precedent); (c) both | design D1; tasks 1.2 |
| 2 | When implementation starts relative to M7 and M8 | (a) R0–R1 now, R2–R3 in parallel with M7 and M8, R4 after their exits; (b) everything after M8 exits; (c) only R1, then re-plan | design D1 |
| 3 | Desktop scope | (a) LVS browser and waveform boards before SHELL, including the run-set, corner and Monte Carlo views; (b) minimal panels with no boards; (c) browser and viewer now, run-set views in M10 | design D23; tasks 17 |
| 4 | Who owns the additive M7 and M8 APIs | (a) a MAPS workstream of the M7 and M8 owners after each exit; (b) M9 workstreams edit them directly; (c) add them before M7 and M8 exit | design D3, D24; tasks 11 |

## Counts

| Kind | Count | IDs (without `Q-`) |
|---|---|---|
| replicate | 6 | LVS-6, LVS-10, LVS-11, LVS-17, LVS-21, LVS-26 |
| fix | 10 | LVS-2, LVS-4, LVS-13, LVS-19, LVS-29, SIM-6, SIM-17, SIMADV-5 (rest), LVS-36, SIM-25 |
| fix (doc) | 1 | LVS-31 |
| platform | 20 | LVS-14, SIM-1, SIM-3, SIM-4, SIM-5, SIM-9, SIM-10, SIM-11, SIM-12, SIM-16, SIM-19, LVS-33, LVS-34, LVS-35, SIM-20, SIM-21, SIM-22, SIM-24, SIM-28, SIMADV-13 |
| verify | 4 | SIM-8, SIMADV-3, SIMADV-4, SIMADV-9 |
| other | 13 | LVS-1, SIM-7, SIM-14, SIM-15, SIMADV-8, SIMADV-10, SIMADV-11, LVS-30, LVS-32, SIM-23, SIM-26, SIM-27, SCHEM-39 |

Total: 54 entries (34 blocking, Q-SIM-1, the rest of Q-SIMADV-5, and 18 new), plus D-22 to D-28 and four planning questions. No row in this table recommends compat. The four `netgen-compat` branch points (Q-LVS-7, -9, -20, -24) were resolved compat on 2026-11-07, before this table. Design D5 says how they are implemented.

## Spec changes the recommendations imply

These are recorded as spec deltas in `openspec/changes/m9-lvs-simulation/specs/`, and they are applied to the living specs at G0 (task 1.2) once the matching rows are approved:

| Capability | Delta | Rows |
|---|---|---|
| `lvs-netlist-input` | ADDED "In-memory netlist sources", "Input limits" | Q-LVS-33, -36 |
| `lvs-setup-file` | ADDED "PDK setup files evaluate unchanged", "Setup evaluation is bounded and cancellable" | Q-LVS-31, -36 |
| `lvs-comparison-algorithm` | ADDED "Single-threaded comparison per pair" | Q-LVS-34, -4 |
| `lvs-device-reduction` | ADDED "Reduction cost" | Q-LVS-35 |
| `lvs-property-matching` | ADDED "Platform-independent property arithmetic" | X-1 |
| `lvs-hierarchy` | ADDED "Hierarchy depth limit" | Q-LVS-36 |
| `lvs-reports` | ADDED "Layout object references in structured results" | Q-LVS-33 |
| `lvs-command` | MODIFIED "Layout netlist from extraction" (incremental, with the stale-header rule), "Setup file and input resolution"; ADDED "LVS status chip" | Q-LVS-31, -32, -33 |
| `simulation-engine-embedding` | MODIFIED "ngspice embedded as a library", "Crash containment on desktop", "ngspice build configuration and license hygiene", "Cancellation and fault states" (reset only on iPadOS), "In-process execution on iPadOS" (link type per D-28), "Multiple simulations and corner parallelism" (the pool formula); ADDED "Worker channel limits", "Worker confinement" | Q-SIM-5, -7, -11, -20, -21, -22, -25, -26, Q-SIMADV-13, D-25, D-28 |
| `simulation-netlist-preparation` | MODIFIED "Net name map" (layout point and `layoutName` computed at it); ADDED "sg13g2 mismatch flag on extracted devices", "Deck limits" | Q-SIM-24, D-23, Q-SIM-25 |
| `simulation-analyses-and-results` | ADDED "Chunked result transfer", "Raw files in golden comparisons" | Q-SIM-21, -17 |
| `simulation-ui-and-crossprobe` | MODIFIED "Probing a layout net" (lookup by `layoutName`, then connectivity); ADDED "Probe names without length limits", "Simulate button" | Q-SIM-24, Q-LVS-33 |
| `simulation-command` | ADDED "Schematic launcher calls" | m8 D13 |
| `simulation-sweeps` | ADDED "Measured waveform retention", "Results in point order" | D-24, Q-SIMADV-13 |
| `simulation-corners` | ADDED "Corner jobs on the worker pool" (the pool size is referenced, not repeated) | Q-SIMADV-13 |
| `simulation-monte-carlo` | ADDED "Sample independence from worker assignment", "Sample count limit" | Q-SIMADV-13, Q-SIM-25 |
| `simulation-measurements-and-statistics` | ADDED "ngspice measure cross-check in debug builds", "Platform-independent statistics" | Q-SIMADV-10 |
| `hefesto-licensing` | MODIFIED "Two-team clean-room process for specs written from GPL source" (netgen added); ADDED "Process boundary for external engines" | Q-SIM-20, -22, -26, Q-LVS-30, D-28 |
| `hefesto-architecture` | ADDED "Long-lived worker processes" | Q-SIM-21 |
| `hefesto-performance` | ADDED "LVS and simulation throughput" | Q-LVS-35, Q-SIM-28 |

Deltas cannot carry these edits, so they are made directly at G0:
- the `lvs-setup-file` Purpose names `ihp-sg13g2_setup.tcl` (Q-LVS-31);
- `tests/golden/LVS/CASES.md` uses that name;
- the roadmap's M9 "Early de-risking" line and D-28 gain a sentence naming the desktop linking question (Q-SIM-20), decided in R0 and reviewed before the Desktop Beta; D-28's iPadOS deadline is unchanged (Q-SIM-26);
- roadmap principle 2 names the new ngspice pin (Q-SIM-7, Q-SIM-23);
- `docs/contributing/cleanroom.md` covers netgen (Q-LVS-30);
- `tests/golden/deviations.toml` defers SCHEM-LVS-01 to M9 (Q-SCHEM-39).

## Verification runs needed before G2

| Run | Settles | Where it is recorded |
|---|---|---|
| `.param` precedence, one deck per PDK | D-22, Q-SIMADV-3, -4, -9 (approved at G0 as conditional rules) | `Verified` lines in `SIMADV.md` |
| sky130 with and without `ng_nomodcheck` on `combined` | D-26, Q-SIM-8 (approved at G0 as a conditional rule) | `Verified` line in `SIM.md` |
| gf180mcu `hs` against `hsa` on the PDK testbenches | D-27, Q-SIMADV-4 | `SIMADV.md` |
| `ihp-sg13g2_setup.tcl` evaluated by netgen 1.5.327 | Q-LVS-31 | NG-PDK capture |
