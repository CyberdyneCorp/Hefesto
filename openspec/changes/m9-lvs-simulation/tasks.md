# Tasks

How this plan runs:
- **Section 1 comes first.** No decision this plan cites is approved yet (`docs/decisions/m9-decision-table.md`). The tasks below follow the recommended options. If the user picks another option, the affected tasks are revised in section 1 before gate G0 passes.
- Gates and rounds follow design D1:
  - G0: section 1 is done.
  - G1: the LVS clean room, section 2.
  - G2: golden capture, section 3.
  - G3: M7 exit plus `render_with_map`.
  - G4: M8 exit plus `NetlistResult::names`.
  
  Each section names its round and its gate.
- **LVS clean room (Q-LVS-30).** Each LVS section names its team. LVS implementation tasks never read netgen source, never follow `Evidence:` lines or `(netgen ref: ...)` traces, and raise spec gaps with the LVS spec team. SCHEMSIM follows the M8 xschem clean room.
- Each task ends with how it is verified.
- Workstreams edit only what design D3 assigns them. Shared-file needs go to INTEGRATION as open items.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Requirements that cite a `Resolution: compat` entry get one test per `netgen-compat` mode.
- The last task of each capability workstream runs `scripts/check_requirement_coverage.py --capability <cap>`. The milestone target is 184 of 184, plus the M9 part of `schematic-simulation-integration`.
- Every function stays at cognitive complexity 15 or below (25–35 for the readers), measured with the cognitive-complexity skill. Irreducible functions are flagged in the pull request.
- Unimplemented paths return `CORE-NOTIMPL-009` until their workstream lands. No such path remains at exit.

## 1. Decisions (R0; gate G0)

- [ ] 1.1 (INTEGRATION) Present `docs/decisions/m9-decision-table.md` to the user, and record the answers:
  - `Status:`/`Resolution:` lines on the 34 blocking entries, on Q-SIM-1, on the rest of Q-SIMADV-5 and on the 18 entries raised by this plan;
  - D-22 to D-28 in `docs/decisions/audit/DECISIONS.md`;
  - the design's Open Questions 1–4.
  
  Verify with `check_milestone_readiness.py --milestone M9` ("ready") and `openspec validate --all --strict`.
- [ ] 1.2 (SPEC) Apply the approved spec changes. Under Open Question 1 (a), sync this change's `specs/` deltas into `openspec/specs/` and add the edits that deltas cannot express:
  - the `lvs-setup-file` Purpose names `ihp-sg13g2_setup.tcl` (Q-LVS-31);
  - the roadmap's M9 de-risking line and D-28 say "desktop part in M9, iPad part before M14" (Q-SIM-26).
  
  Revise any delta whose answer differs from the recommendation. Verify that `openspec validate --all --strict` passes and that the decision table's "Spec changes" section lists each edit.
- [ ] 1.3 (INTEGRATION) Q-SCHEM-39: if it is approved (a), set `[SCHEM-LVS-01] deferred = "M9"` in `tests/golden/deviations.toml` with D13's reason, and update `tests/golden/SCHEM/CASES.md`, m8 design D21 and the roadmap M8 exit line. Verify that the golden harness reports SCHEM-LVS-01 as deferred to M9 and that the M8 exit list no longer waits for it.

## 2. CLEANROOM: netgen two-team process (LVS spec team; R1; gate G1)

- [ ] 2.1 Extend `docs/contributing/cleanroom.md` to netgen:
  - the LVS spec team and the LVS implementation team (roles, not names, for agents);
  - which workstreams belong to each (design D3);
  - the session rules of design D2 (no netgen repository, no `/opt/src/netgen`, no `Evidence:` or `netgen ref` paths; the public manual and black-box runs allowed).
  
  Verify that a reviewer outside both teams confirms each rule against `hefesto-licensing`.
- [ ] 2.2 Paraphrase review of the eight `lvs-*` specs and `docs/decisions/open-questions/LVS.md`, starting with `lvs-comparison-algorithm` and `lvs-device-reduction`. Rewrite each finding as behaviour, and record the findings in `docs/decisions/m9-paraphrase-review.md`. Verify `openspec validate --all --strict` and a second spec-team signature.
- [ ] 2.3 Extend `scripts/check_cleanroom.py` and its tests:
  - netgen's GPL header in any tracked file;
  - `netgen ref:` traces under `src/` and `tests/`;
  - `provenance` on every `tests/golden/LVS/` case.
  
  Verify with fixture trees for each failure and a clean run on the branch.
- [ ] 2.4 Add the LVS item to the pull-request template's clean-room section ("I have not read netgen's source"). Add LVS to the M13 counsel row of the roadmap's de-risking schedule. Verify that the template renders and the roadmap row names Q-LVS-30.

## 3. PDK and GOLDEN: data sets, capture image, comparators, captures (R1; gate G2)

- [ ] 3.1 (PDK) Add the data sets `gf180mcu-netgen`, `ihp-netgen`, `sky130-ngspice` (`libs.tech/combined` and `libs.tech/ngspice`), `gf180mcu-ngspice` and `ihp-ngspice` (with the Verilog-A sources) to `scripts/fetch_pdk.py`, each with pins, subsets, tree digests and cache keys. Add their paths to `tests/unit/common/reference_data.hpp` with clean skips, and document them in `docs/building.md`. Verify that a fresh install matches the digests, that a second run is a no-op, and that the `scripts/tests/` tests pass.
- [ ] 3.2 (GOLDEN) Capture image (Q-SIM-23 (a)):
  - build ngspice from the engine's pin (Q-SIM-7) with the patch series of task 5.1 and the spec feature table, as both the executable and `--with-ngshared`;
  - add a pinned OpenVAF;
  - update `tools/golden/pins.toml` and `tools/golden/tests/test_pins.py`.
  
  Verify the CI image build and a log line reporting `<release>-hefesto<n>`.
- [ ] 3.3 (GOLDEN, with SPEC) Reference runs on the image, each recorded as a `Verified` line:
  - D-22, `.param` precedence: one deck per PDK, with the result recorded in `SIMADV.md` under Q-SIMADV-3, -4 and -9;
  - D-26, sky130 `ng_nomodcheck`, recorded in `SIM.md` under Q-SIM-8;
  - D-27, gf180mcu `hs` against `hsa`.
  
  Verify that each entry has a `Verified` line with the run id. If a result contradicts the approved option, raise it to the user.
- [ ] 3.4 (GOLDEN) Comparators `tools/golden/compare/lvs_canonical.py`, `raw_tolerance.py` and `meas_table.py` (design D19), registered with the case tool, with tests on hand-written equal and unequal inputs:
  - a reordered hash section passes and a changed device count fails;
  - a `Date:`-only difference passes and a 1 % voltage error fails;
  - a column within its tolerance passes.
  
  Verify the tests.
- [ ] 3.5 (GOLDEN, LVS implementation team for inputs) Author the 77 LVS cases of `tests/golden/LVS/CASES.md` as `case.toml` plus inputs, each with `provenance`. Hand-written netlists are written from the specs only. NG-PDK uses the three setup files (Q-LVS-31). Verify `check_cleanroom.py` and that every case has a requirement tag.
- [ ] 3.6 (GOLDEN) Author the 53 SIM cases and the 40 M9 SIMADV cases. The DUT netlists come from Magic in the image; the decks are Hefesto-written. Verify that every case has a requirement tag and declared tolerances.
- [ ] 3.7 (GOLDEN) Capture `expected/` for LVS, SIM and SIMADV in three `golden-capture.yml` runs, and verify each with a second run on a fresh image build. Verify the run ids, and that every case has stdout, stderr and an exit code plus its declared outputs.
- [ ] 3.8 (SPEC) Record the texts that depend on captures: the `comp.out` texts the LVS specs leave to capture (Q-LVS-2 set sections, Q-LVS-13 naming), the vector names of SIM-NET-03 and SIM-UI-04 (Q-SIM-6), and progress on the third run (SIM-RUN-03, Q-SIM-14). Verify `openspec validate --all --strict`, and that no `Resolution:` line changed (otherwise raise it to the user).

## 4. SCAFFOLD (R1; after G0)

- [ ] 4.1 Add the areas `lvs` (subdirectories `net read setup cmp reduce prop hier report cmd`) and `sim` (`engine deck result probe cmd runset meas`), each with `sources.cmake`, `codes.inc` and test directories. Add `docs/diagnostics/codes/{lvs,sim}.md` and `CORE-NOTIMPL-009` (constant `diag::codes::kM9NotImplemented`). Verify that the build passes in `macos-debug` and that the code-table test covers both areas.
- [ ] 4.2 Write the public headers of design "Area contracts" with stubs that return `CORE-NOTIMPL-009`. Add `lvs::Session` and `sim::Session` to `EditorState`. Record the as-landed contracts in the design. Verify that the headers compile and that `scripts/check_core_includes.py` passes.
- [ ] 4.3 Add `scripts/check_engine_boundary.py` (design D2): no `ngSpice_` symbol in `hefesto-core`, `hefesto-cli` or the desktop executable, and `sharedspice.h` included only under `src/simd/adapter/`. Run it in CI. Verify with a fixture that violates each rule, and a clean run.

## 5. NGBUILD, SIMD, PLATFORM: engine build, worker, worker service (R1–R2)

- [ ] 5.1 (NGBUILD) Add `third_party/ngspice/` with:
  - the pin of Q-SIM-7;
  - the patch series (static OSDI registry, static code-model registration, portable seeding, deterministic initialisation);
  - a CMake wrapper with the feature table, `-ffp-contract=off` and OpenMP off;
  - the shared library `libngspice-hefesto` (Q-SIM-20 (a));
  - `ngspice-manifest.json`.
  
  Verify the build on the three CI legs, that the manifest lists the patches and licences, and the `simulation-engine-embedding` scenarios "No GPL code linked" and "Patch list in the manifest".
- [ ] 5.2 (NGBUILD) Compile the IHP OSDI modules (PSP103, `cap_cmomi`, `cap_cmomf`) from the `ihp-ngspice` set with the pinned OpenVAF, and register them statically. Verify "Version mismatch avoided" and an sg13g2 `op` deck.
- [ ] 5.3 (PLATFORM) Add `platform::WorkerHost` and `WorkerChannel` (design D9) with hosts in `src/cli/` and the desktop shell (`posix_spawn` and pipes). The core default reports it unsupported. Verify unit tests with a test worker that echoes frames, exits, crashes and hangs, and the `hefesto-architecture` scenarios of "Long-lived worker processes".
- [ ] 5.4 (SIMD) Add `src/simd/`: the engine adapter (the only includer of `sharedspice.h`; it copies every string and array before returning), the frame codec (`hefesto/sim/engine/frames.hpp`), the IPC server and the heartbeat. Verify the adapter tests and "Engine adapter boundary", "Callback contract" and "Deterministic engine initialisation" through the worker.
- [ ] 5.5 (SIMENG) Add `sim/engine/`: the `Engine` interface, `WorkerEngine` and `JobManager`, with the fault policy of D-25 (replace the worker on any fault) and the limits of Q-SIM-25. The in-process engine is built in developer builds only (Q-SIM-22). Verify:
  - "ngspice segmentation fault";
  - "Unrecoverable error";
  - a missed heartbeat;
  - an oversize frame;
  - `fuzz_sim_frames` for 10 minutes with no finding;
  - the requirement coverage of `simulation-engine-embedding`, with the iPadOS-only requirements listed as deferred to M14.

## 6. LVSREAD: lvs-netlist-input (LVS implementation team; R2; after G1, G2)

- [ ] 6.1 Netlist database, device classes, format selection, name matching with the unsigned-byte fold (Q-LVS-3), and SI values. Verify the requirement tests and NG-NAME-01..03.
- [ ] 6.2 SPICE and CDL: lexical structure, subcircuit definitions, primitive cards, calls and placeholders, globals, includes and parameters (Q-LVS-14), library and black-box reads, and in-memory sources and limits (Q-LVS-36). Verify the tests, NG-READ cases and `fuzz_lvs_spice`.
- [ ] 6.3 Structural Verilog, `.sim`, and legacy `.ext` per Q-LVS-1 (no GUI entry). Verify the tests, the remaining NG-READ cases, `fuzz_lvs_verilog` and the coverage of `lvs-netlist-input`.

## 7. LVSSETUP: lvs-setup-file (LVS implementation team; R2)

- [ ] 7.1 Setup language and runtime in the X-3 sandbox, evaluation, cell reference syntax, `permute`, and every `property` sub-command (Q-LVS-6). Verify the tests, one test per mode for each compat entry, and NG-SET cases.
- [ ] 7.2 `equate classes` (Q-LVS-29 fix), `ignore`, `model`, `global`/`convert`, `flatten`, and the query and output commands. Verify the tests, NG-PDK-01..03 with the three PDK setup files, `fuzz_lvs_setup`, and the coverage of `lvs-setup-file`.

## 8. LVSALG: lvs-comparison-algorithm (LVS implementation team; R2)

- [ ] 8.1 Comparison graph, class identity and pin labels, initial partition, legality, and iterative refinement with the seeded generator and the collision check (Q-LVS-4). Verify the tests and NG-ALG cases.
- [ ] 8.2 Match verification, symmetry resolution, `verify`, forced equivalences, deterministic arbitrary pairing, long runs and cancellation, and reset and stepping (Q-LVS-21). Verify the tests, the single-threaded determinism test (the same pair 100 times, identical reports), and the coverage of `lvs-comparison-algorithm`.

## 9. LVSRED and LVSPROP: lvs-device-reduction, lvs-property-matching (LVS implementation team; R3)

- [ ] 9.1 Combination flags, schedule, parallel and series reduction (Q-LVS-26), permutable pins, zero-valued removal, merged counts, and preservation of pin order and names. The O(n log n) bound is checked by a 1M-finger test. Verify the tests, NG-RED cases and the coverage of `lvs-device-reduction`.
- [ ] 9.2 Properties of interest, value types, tolerance, missing properties, timing of checks, pin-associated properties, series/parallel reconciliation, property errors (Q-LVS-10), and strict and relaxed topology. Verify the tests, NG-PROP cases and the coverage of `lvs-property-matching`.

## 10. LVSHIER and LVSREP: lvs-hierarchy, lvs-reports (LVS implementation team; R3)

- [ ] 10.1 Compare queue, cell-pair preparation, pre-match flattening (Q-LVS-11), flattening semantics, duplicate and shorted pins, the per-pair flow, pin matching (Q-LVS-17), black boxes, the final result, and the iterative depth limit. Verify the tests, NG-HIER and NG-PIN cases, and the coverage of `lvs-hierarchy`.
- [ ] 10.2 Log and console routing, the two-column layout, the summary, net, device, pin and property reports, the list result, JSON (Q-LVS-13, Q-LVS-19), classification, the canonical comparison (Q-LVS-2), and layout references in the structured result. Verify the tests, NG-REP cases through `lvs_canonical.py`, and the coverage of `lvs-reports`.

## 11. MAPS: name maps from M7 and M8 (R4; after M7 and M8 exits)

- [ ] 11.1 (M7 owner) `netlist/name_map.hpp` and `netlist::spice::render_with_map` (Q-LVS-33 (a)). `render` stays unchanged. Verify that the text equals `render`'s on every NETLIST golden case, and that every net and device of the NETLIST-PDK cases has a map entry with a layout point inside its node.
- [ ] 11.2 (M8 owner, M8 clean room) `schem::NetlistResult::names`. Verify that the netlist text is unchanged on every SCHEM-NL case, and that every net and instance maps to a schematic object.

## 12. LVSCMD: lvs-command (LVS implementation team; R4; after G3, G4)

- [ ] 12.1 `lvs` syntax, the `netgen` namespace (D-34), and setup and input resolution (Q-LVS-31, with the `ihp-sg13g2` scenario). Verify the tests and NG-CMD-01.
- [ ] 12.2 The layout source through `ensure_extracted` and `render_with_map` (Q-LVS-32), the `.sch` source through `schem::netlist`, the execution model on the background worker with the database read-only, and headless LVS. Verify "One-step LVS on iPad" at the core level on Linux, an incremental rerun that re-extracts only an edited cell, and `hefesto-cli` LVS of sky130 `inv_1`.
- [ ] 12.3 The result model, the browser view model, cross-probing (report → layout, report → schematic) and the status chip with its stale state. Verify the view-model tests and the coverage of `lvs-command`.

## 13. SIMDECK and SIMRES: preparation, analyses and results (R2 for raw and SPICE sources, R4 for layout and schematic)

- [ ] 13.1 (R2) The `spice` and `raw` sources, testbench composition, stimuli, deck validation and sanitisation (Q-SIM-9), deck limits (Q-SIM-25), and deck delivery. Verify the tests and SIM-NET cases on SPICE sources.
- [ ] 13.2 (R2) Model descriptors in the PDK store for the three PDKs (Q-SIM-8, D-26, D-27), model library resolution (Q-SIM-13), PDK initialisation commands, and corners in the descriptor. Verify the descriptor tests against the pinned data sets and SIM-NET PDK cases.
- [ ] 13.3 (R2, SIMRES) Analyses, job lifecycle, progress (Q-SIM-14), pause, resume and cancel, final vectors, saved vectors and the memory budget (Q-SIM-16), storage, raw format (Q-SIM-17), CSV, raw import with its cap, and waveform measurements. Verify the tests, SIM-RES and SIM-RUN cases, `fuzz_sim_raw`, and the coverage of `simulation-analyses-and-results`.
- [ ] 13.4 (R4, after G3, G4) The layout DUT through `ensure_extracted` and `render_with_map`, parasitic modes `none`, `c` and `rc` (`extresist::run`), the net name map, case collisions (Q-SIM-6), the schematic source through `schem::build_deck`, and `mm_ok=1` for sg13g2 (D-23). Verify the tests, the remaining SIM-NET cases, and the coverage of `simulation-netlist-preparation`.

## 14. SIMPROBE and SIMCMD (R3 for the command, R4 for probes)

- [ ] 14.1 (R3, SIMCMD) Option matching, the subcommand table, numeric syntax, script returns and errors, headless and batch use, the engine subcommand, testbench persistence (with D-10's undo events), and schematic launcher calls. Verify the tests, SIM-CMD cases, "CI corner sweep", and the coverage of `simulation-command`.
- [ ] 14.2 (R4) Hierarchical node naming brought forward from `irsim-interface` (Q-SIM-24; tests tagged to that requirement; a name longer than 256 bytes), layout and device-current probes, trace highlighting, operating-point annotation as overlays, and the 3D overlay feed. Verify the tests and SIM-UI cases.

## 15. SIMADV and SIMMEAS: sweeps, corners, Monte Carlo, measurements (R3)

- [ ] 15.1 (SIMADV) Run sets, sweep variables, point generation, point decks with overrides per D-22 (Q-SIMADV-9), DOE, execution on the pool (Q-SIMADV-13), storage, result views, command options, and the `measured` retention mode (D-24, Q-SIMADV-11). Verify the tests, SIMADV-SW cases, the 1-against-4-worker determinism test, and the coverage of `simulation-sweeps`.
- [ ] 15.2 (SIMADV) Corners: descriptor corners, bundled tables, user corners and sets, the corner dimension, pass and fail, the matrix, and run all and re-run failed. Verify the tests, SIMADV-CO cases, and the coverage of `simulation-corners`.
- [ ] 15.3 (SIMADV) Monte Carlo: modes (Q-SIMADV-3, -4, -5, -8), the MC dimension, sample seeds, the portable RNG, reproducibility, parallel and sequential runs, failed samples and early stop, user statistical parameters, and options. Verify the tests, SIMADV-MC cases, a sample's result being identical on worker 1 of 1 and worker 3 of 4, and the coverage of `simulation-monte-carlo`.
- [ ] 15.4 (SIMMEAS) Definitions, `.meas` import, expressions, specs, descriptive statistics, yield, Cpk, histograms, correlation, export and options, with fixed-order compensated sums and the debug-build `.meas` cross-check (Q-SIMADV-10). Verify the tests, SIMADV-MEAS cases including MEAS-04, `fuzz_sim_meas`, and the coverage of `simulation-measurements-and-statistics`.

## 16. SCHEMSIM: schematic-simulation-integration, M9 part (M8 implementation team, xschem clean room; R4; after G4)

- [ ] 16.1 Run the schematic circuit source, make the launchers complete (remove the not-implemented answer that M8 leaves on the `sim` path), and add `xschem simulate` and `xschem raw ...`. Verify the requirement tests and SCHEM-SIM-01.
- [ ] 16.2 Operating-point annotation, embedded graphs, waveform cross-probing, and results interchange with xschem (raw read and write). The spec team pins the exact outputs of `to_eng`, `tabulate` and `ngspice::` from SCHEM-SIM-02. Verify the tests and SCHEM-SIM-02.
- [ ] 16.3 Schematic-to-layout cross-probing and the schematic-driven LVS command. Verify the tests, SCHEM-LVS-01 (under Q-SCHEM-39 (a)), and `check_requirement_coverage.py --capability schematic-simulation-integration` with nothing deferred.

## 17. DESIGN and SHELL: desktop (R3 boards, R4 screens; design Open Question 3)

- [ ] 17.1 (DESIGN) Draw `docs/design/mockups/LVS-Browser.dc.html` and `Waveforms.dc.html` (design D23), and seed `docs/design/m9-ui-conformance.md`. Verify the board review note.
- [ ] 17.2 (SHELL) The LVS status chip, the result browser and cross-probe highlights; the Simulate button and testbench panel with the engine-unavailable tooltip; the Waveforms tab and viewer (cursors, readouts, complex display, live traces, decimated rendering); run-set, corner, Monte Carlo and yield views; correction of the stale M8 labels. Verify the shell view-model tests, the conformance checks with screenshots, and "Glitch remains visible".

## 18. SECURITY (reviewer outside both LVS teams; R4)

- [ ] 18.1 Review the setup-file sandbox, deck sanitisation, the core's frame parser and raw import (design D22) with hostile inputs, and add each one as a unit test. Verify that every denial gives its diagnostic, and record the sign-off with its date in the M9 exit table.

## 19. PERF (R5)

- [ ] 19.1 `lvs-bench` (Q-LVS-35): callgrind counts against netgen on the three workloads, recorded in `bench/lvs/` and run in `perf.yml`. Verify the counts on the x86_64 runner and the `hefesto-performance` "LVS and simulation throughput" scenarios.
- [ ] 19.2 `sim-bench` (Q-SIM-28): the overhead ratio on the three decks, Monte Carlo scaling, and the storage cost of the `measured` mode (D-24 default decision). Verify recorded baselines on the M3 Pro and the Linux runner, and report the `measured` storage numbers to the user.

## 20. INTEGRATION (every round; exit in R6)

- [ ] 20.1 `THIRD_PARTY_NOTICES.md`: rows for KLU, numparam, OSDI and the compiled-in OSDI models, OpenVAF noted as build-only, and netgen absent (design D2). Verify `check_third_party_notices.py` and the `hefesto-licensing` scenarios.
- [ ] 20.2 Desktop Beta end-to-end cases BETA-E2E-SKY130, -GF180 and -IHP (Q-SIM-27): testbench simulation, DRC-clean layout, GDS, extraction, an LVS match, and `rc` post-layout simulation within 30 % of the schematic delay. Verify them on Linux x86_64 and macOS arm64 in CI.
- [ ] 20.3 Docs: `docs/diagnostics/codes/{lvs,sim}.md` complete; `docs/building.md` (ngspice, OpenVAF, data sets); `docs/contributing/` (areas, fuzz targets, golden modules); the CHANGELOG; the roadmap M9 status; `docs/design/m1-ui-conformance.md` rows. Verify the docs checks.
- [ ] 20.4 M9 exit check (design D25): the golden suites, coverage of the 17 capabilities plus the M9 part of `schematic-simulation-integration`, three-platform identity and tolerance, both-mode tests, `check_engine_boundary.py`, `check_cleanroom.py`, the security sign-off, the PERF gates, retirement of `CORE-NOTIMPL-009`, and `openspec validate --all --strict`. Verify each item in the M9 exit table with run ids.
- [ ] 20.5 The user's Desktop Beta sign-off on Linux and macOS with real pointer and keyboard input (Q-SIM-27). Verify that the sign-off is recorded with its date in the roadmap.

## Parallel workstreams per round

| Round | In parallel |
|---|---|
| R0 | INTEGRATION (1.1), SPEC (1.2) |
| R1 | CLEANROOM (2), PDK (3.1), GOLDEN (3.2–3.7), NGBUILD (5.1–5.2), PLATFORM (5.3), SCAFFOLD (4) |
| R2 | LVSREAD (6), LVSSETUP (7), LVSALG (8) · SIMD (5.4), SIMENG (5.5) · SIMDECK (13.1–13.2), SIMRES (13.3) |
| R3 | LVSRED/LVSPROP (9), LVSHIER/LVSREP (10) · SIMCMD (14.1) · SIMADV (15.1–15.3), SIMMEAS (15.4) · DESIGN (17.1) |
| R4 | MAPS (11) → LVSCMD (12), SIMDECK (13.4), SIMPROBE (14.2), SCHEMSIM (16) · SHELL (17.2) · SECURITY (18) |
| R5 | GOLDEN SCHEM and Beta cases (16, 20.2), PERF (19) |
| R6 | INTEGRATION exit (20.3–20.5) |
