# Tasks

How this plan runs:
- **Section 1 comes first.** The user approved every decision this plan cites on 2026-11-22, each with its recommended option (`docs/decisions/m9-decision-table.md`), so the tasks below stand as written.
- Gates and rounds follow design D1:
  - G0: section 1 is done.
  - G1: the LVS clean room, section 2.
  - G2-LVS and G2-SIM: the LVS capture and the SIM/SIMADV capture, section 3. Each stream waits only for its own capture.
  - G3: M7 exit plus `render_with_map`.
  - G4: M8 exit plus `NetlistResult::names`.
  
  Each section names its round, its gate and the tasks it waits for (`after …`).
- **LVS clean room (Q-LVS-30).** Each LVS section names its team. LVS implementation tasks never read netgen source, never follow `Evidence:` lines or `(netgen ref: ...)` traces, and raise spec gaps with the LVS spec team. A session that read netgen source never takes LVSCMD (12) or SCHEMSIM 16.3. SCHEMSIM follows the M8 xschem clean room.
- Each task ends with how it is verified.
- Workstreams edit only what design D3 assigns them. Shared-file needs go to INTEGRATION as open items.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Requirements that cite a `Resolution: compat` entry get one test per `netgen-compat` mode.
- The last task of each capability workstream runs `scripts/check_requirement_coverage.py --capability <cap>`. The milestone target is every requirement of the 17 capabilities after the task 1.2 sync (184 today plus the requirements this change adds), plus the M9 part of `schematic-simulation-integration`. The iPadOS-only requirements of `simulation-engine-embedding` are listed as deferred to M14.
- Every function stays at cognitive complexity 15 or below (25–35 for the readers), measured with the cognitive-complexity skill. Irreducible functions are flagged in the pull request.
- Unimplemented paths return `CORE-NOTIMPL-009` until their workstream lands. No such path remains at exit.

## 1. Decisions (R0; gate G0)

- [x] 1.1 (INTEGRATION) Present `docs/decisions/m9-decision-table.md` to the user, and record the answers:
  - `Status:`/`Resolution:` lines on the 34 blocking entries, on Q-SIM-1, on the rest of Q-SIMADV-5 and on the 18 entries raised by this plan;
  - for the four verify rows (Q-SIM-8, Q-SIMADV-3, -4, -9), conditional `Resolution:` lines that name the option for each outcome of the reference run (design D1), so that the rows are resolved at G0 and only their `Verified` lines wait for task 3.3;
  - D-22 to D-28 in `docs/decisions/audit/DECISIONS.md`;
  - the design's Open Questions 1–4.
  
  Verify with `check_milestone_readiness.py --milestone M9` ("ready", with no row left pending for a reference run) and `openspec validate --all --strict`. **As landed (2026-11-22, branch `m9-round0-decide`):** all approved with the recommended options; 52 entries got `Status: approved 2026-11-22` and a `Resolution:` line (20 LVS, 24 SIM, 7 SIMADV, Q-SCHEM-39), Q-SIM-1's draft line was replaced and Q-SIMADV-5's open part resolved (D-23); the four verify rows have conditional lines and no `Verified` line; D-22 to D-28 have `Status:` lines; OQ1–4 (a) are in `design.md`. Readiness: `M9: ready`; `openspec validate --all --strict`: 179 passed.
- [x] 1.2 (SPEC) Apply the approved spec changes. Under Open Question 1 (a), sync this change's `specs/` deltas into `openspec/specs/`, then set `skip_specs: true` in `openspec/changes/m9-lvs-simulation/.openspec.yaml`, so that the archive at exit (task 20.6) does not apply the deltas a second time. Add the edits that deltas cannot express:
  - the `lvs-setup-file` Purpose names `ihp-sg13g2_setup.tcl` (Q-LVS-31);
  - the roadmap's M9 de-risking line and D-28 gain a sentence naming the desktop linking question, decided in R0 and reviewed before the Desktop Beta, with D-28's iPadOS deadline unchanged (Q-SIM-26);
  - roadmap principle 2 names the ngspice pin approved under Q-SIM-7 in place of `7aa47164` (Q-SIM-23).
  
  Revise any delta whose answer differs from the recommendation. Verify that `openspec validate --all --strict` passes, that a dry-run `openspec archive m9-lvs-simulation` reports no spec change, and that the decision table's "Spec changes" section lists each edit. **As landed (2026-11-22):** synced by running the archive on a scratch copy (+29 added, ~11 modified, 0 removed, 20 capabilities; no new capability folder, so `docs/specs-index.md` is unchanged); deltas revised first for the roster wording (Q-LVS-30), the ngspice-47 pin (Q-SIM-7), the Q-SIM-1 sentence and a pool scenario (Q-SIMADV-13). `openspec` 1.13 fails `--strict` when `skip_specs: true` sits beside delta files, so the synced `specs/` folder was removed from the change; a dry-run archive of the branch then changes no spec. The `tests/golden/LVS/CASES.md` rename (NG-PDK-03) is left to GOLDEN.
- [x] 1.3 (INTEGRATION) Q-SCHEM-39: if it is approved (a), set `[SCHEM-LVS-01] deferred = "M9"` in `tests/golden/deviations.toml` with D13's reason, and update `tests/golden/SCHEM/CASES.md`, m8 design D21 and the roadmap M8 exit line. Verify that the golden harness reports SCHEM-LVS-01 as deferred to M9 and that the M8 exit list no longer waits for it. **As landed (2026-11-22):** the SCHEM deviation block is generated, so the entry was added to `LATER` in `tools/golden/authoring/schem_cases.py` and the block regenerated; `goldenlib.deviations.load` gives `deferred='M9'`; `test_schem_golden.py` (now also asserting SCHEM-LVS-01) and `test_deviations.py` pass; CASES.md row, m8 D21 and the roadmap M8 exit line updated.
- [x] 1.4 (SPEC, simulation spec side) Re-check every `(ngspice ref: ...)` trace of the nine `simulation-*` specs and `SIM.md`/`SIMADV.md` against the approved pin. ngspice-47 (`a80f6e3e`) has no `cmake/` tree, so traces such as `cmake/NgspiceOptions.cmake:21-74` and `:86` are rewritten to the pinned tree's build files or to the Hefesto wrapper, and moved line numbers are corrected. Verify with a list of every trace checked in `docs/decisions/m9-decision-table.md` and `openspec validate --all --strict`. **As landed (2026-11-22):** 112 traces checked (66 spec, 46 in SIM.md/SIMADV.md) through SourceForge raw views, nothing cloned: 51 same lines, 35 moved, 6 rewritten from CMake to `configure.ac`/`src/Makefile.am`/the wrapper, 20 tied to findings F1–F5 (ngspice-47 lacks the post-47 draw-once `.param` semantics and the `setseed` reseed of the `trnoise` pool, which `simulation-monte-carlo` and `simulation-engine-embedding` assume; raised to the user).

## 2. CLEANROOM: netgen two-team process (LVS spec team; R1; gate G1)

- [ ] 2.1 Extend `docs/contributing/cleanroom.md` to netgen, as the MODIFIED `hefesto-licensing` requirement "Two-team clean-room process for specs written from GPL source" now states:
  - the LVS spec team and the LVS implementation team (roles, not names, for agents);
  - which workstreams belong to each (design D3), and the roster rule (no netgen reader on LVSCMD or SCHEMSIM 16.3);
  - the session rules of design D2 (no netgen repository, no `/opt/src/netgen`, no `Evidence:` or `netgen ref` paths; the public manual and black-box runs allowed).
  
  Verify that a reviewer outside both teams confirms each rule against `hefesto-licensing`.

  As landed (PR #94 via m9-round0, LVS spec team, 2026-11-24): `docs/contributing/cleanroom.md` section "netgen clean room for LVS (M9)": LVS spec and implementation teams by role, the D3 workstream table, the roster rule (no netgen reader on LVSCMD, SCHEMSIM 16.3 or task 3.5), the D2 session rules, an implementation instruction block, the LVS checks, the LVS roster and the hefesto-licensing mapping table. **Open:** the sign-off of a reviewer outside both LVS teams on the mapping table (raised to the user).
- [ ] 2.2 Paraphrase review of the eight `lvs-*` specs and `docs/decisions/open-questions/LVS.md`, starting with `lvs-comparison-algorithm` and `lvs-device-reduction`. Rewrite each finding as behaviour, and record the findings in `docs/decisions/m9-paraphrase-review.md`. Verify `openspec validate --all --strict` and a second spec-team signature.
- [x] 2.3 Extend `scripts/check_cleanroom.py` and its tests:
  - netgen's GPL header in any tracked file;
  - `netgen ref:` traces under `src/` and `tests/`;
  - `provenance` on every `tests/golden/LVS/` case.
  
  Verify with fixture trees for each failure and a clean run on the branch.

  As landed (PR #94 via m9-round0): `check_cleanroom.py` checks a per-tool table (xschem, netgen): netgen's GPL header (first and copyright lines of 1.5.327 `base/netgen.c`), `netgen ref:` under `src/` and `tests/`, and `provenance` (`hefesto` or `pdk:<set>:<path>`, never netgen's distribution or `/opt/src/netgen`) on `tests/golden/LVS/**/case.toml`; a tree without `tests/golden/LVS` passes. 15 fixture tests; clean on the branch.
- [x] 2.4 Add the LVS item to the pull-request template's clean-room section ("I have not read netgen's source"). Add LVS to the Desktop 1.0 counsel row of the roadmap's de-risking schedule. Verify that the template renders and the roadmap row names Q-LVS-30. **Roadmap half done (2026-11-22, R0):** the Desktop 1.0 counsel row names LVS (Q-LVS-30); the template item remains.

  Template half as landed (PR #94 via m9-round0): the LVS author item "I have not read netgen's source" and two LVS reviewer items in `.github/pull_request_template.md`. INTEGRATION added `.github/ISSUE_TEMPLATE/lvs-spec-issue.md` (label `lvs-spec`) and named netgen/LVS in the roadmap M13 counsel-review exit item.

## 3. PDK and GOLDEN: data sets, capture image, comparators, captures (R1; gates G2-LVS and G2-SIM)

The G2-SIM critical path is 5.1 → 3.2 → 3.3 and 3.6 → 3.7 (SIM, SIMADV) → 3.8. The G2-LVS path is 2.1–2.2 → 3.5 → 3.7 (LVS) → 3.8.

- [x] 3.1 (PDK) Add the data sets `gf180mcu-netgen`, `ihp-netgen`, `sky130-ngspice` (`libs.tech/combined` and `libs.tech/ngspice`), `gf180mcu-ngspice` and `ihp-ngspice` (with the Verilog-A sources) to `scripts/fetch_pdk.py`, each with pins, subsets, tree digests and cache keys. Add their paths to `tests/unit/common/reference_data.hpp` with clean skips, and document them in `docs/building.md`. Verify that a fresh install matches the digests, that a second run is a no-op, and that the `scripts/tests/` tests pass.

  As landed (PR #94 via m9-round0): five sets under `.cache/pdks/{netgen,ngspice}/`, digests gf180mcu-netgen `91b3dffa`, ihp-netgen `7f6cf90b`, sky130-ngspice `a4e109d0` (also keeps `libs.ref/sky130_fd_pr/spice`, which both model trees include), gf180mcu-ngspice `31ead471`, ihp-ngspice `89ad3139`; a fresh install matched every digest and a second run was a no-op; `reference_data.hpp` `pinned_tool_file()`; `docs/building.md` section "netgen and ngspice data sets (M9)". Not yet wired into `golden-capture.yml` (see 3.7).
- [x] 3.2 (GOLDEN; after 5.1) Capture image (Q-SIM-23 (a)):
  - build ngspice from the engine's pin (Q-SIM-7) with the patch series of task 5.1 and the spec feature table, as both the executable and `--with-ngshared`;
  - add a pinned OpenVAF;
  - update `tools/golden/pins.toml` and `tools/golden/tests/test_pins.py`.
  
  Verify the CI image build and a log line reporting `<release>-hefesto<n>`.

  As landed (branch `m9-round0-ngbuild`): `tools/golden/container/build_ngspice.sh` builds `third_party/ngspice` (copied into the build context as `container/ngspice/` by `third_party/ngspice/tools/sync_image_copy.py`; `test_pins.py` checks the copy) as `/opt/ref/bin/ngspice` and `/opt/ref/lib/libngspice-hefesto.so`, then installs OpenVAF-reloaded 20260616 (`/opt/ref/bin/openvaf-r`, SHA-256 checked). The stock pre-master-48 build moved to `/opt/ngspice-stock` (`[tools.ngspice_stock]`). golden-capture run 45411226446 logged `hefesto-golden: ngspice 47-hefesto1` and `OpenVAF-Reloaded 20260616-0-g44cfee3b`.
- [ ] 3.3 (GOLDEN, with SPEC; after 3.1, 3.2, and after the F1 decision) Reference runs on the image, each recorded as a `Verified` line under the conditional `Resolution:` line approved at G0:
  - D-22, `.param` precedence: one deck per PDK, with the result recorded in `SIMADV.md` under Q-SIMADV-3, -4 and -9 (after the F1 decision of `docs/decisions/m9-decision-table.md`: a later F1 patch changes `.param` evaluation and would invalidate these runs);
  - D-26, sky130 `ng_nomodcheck`, recorded in `SIM.md` under Q-SIM-8;
  - D-27, gf180mcu `hs` against `hsa`.
  
  Verify that each entry has a `Verified` line with the run id that names the branch of the rule it selects. If a result falls outside every branch of the approved rule, raise it to the user before G2-SIM.
- [x] 3.4 (GOLDEN) Comparators `tools/golden/compare/lvs_canonical.py`, `raw_tolerance.py` and `meas_table.py` (design D19), registered with the case tool, with tests on hand-written equal and unequal inputs:
  - a reordered hash section passes and a changed device count fails;
  - a `Date:`-only difference passes and a 1 % voltage error fails;
  - a column within its tolerance passes.
  
  Verify the tests.

  As landed (PR #94 via m9-round0): `lvs-canonical`, `raw-tolerance` (pairwise) and `meas-table[:<col>=<rel>/<abs>,...]` (pairwise, parametrized) registered in `compare/__init__.py`; 29 tests in `tools/golden/tests/test_m9_compare.py`. The comp.out run-boundary rules of `lvs_canonical.py` are tunable and are checked against the first LVS capture in task 3.8.
- [ ] 3.5 (GOLDEN, LVS implementation team for inputs; after 2.1–2.2) Author the 77 LVS cases of `tests/golden/LVS/CASES.md` as `case.toml` plus inputs, each with `provenance`. Hand-written netlists are written from the specs only, by sessions that the 2.1 roster places on the implementation team. NG-PDK uses the three setup files (Q-LVS-31). Verify `check_cleanroom.py` and that every case has a requirement tag.
- [ ] 3.6 (GOLDEN; after 3.2) Author the 53 SIM cases and the 40 M9 SIMADV cases. The DUT netlists come from Magic in the image; the decks are Hefesto-written.
  - Mark the iPadOS cases and legs of design D19 `deferred = "M14"` in `tests/golden/deviations.toml` with the reason "needs the iPadOS in-process engine (M14)": SIM-ENG-01 (iPad leg), SIM-ENG-07, -09, -10, -12, SIM-ENG-13 (iPad leg), SIM-RES-07, SIMADV-SW-07, SIMADV-MC-04 (iPad leg), SIMADV-MC-13.
  - Re-author SIM-ENG-06 to expect the next job in a replaced worker (D-25), SIM-ENG-04 to use a pool of one worker, and SIM-ENG-11 to set a result budget that lets the pool formula reach 4 workers on its host; update `tests/golden/SIM/CASES.md` to match.
  - Run-set cases that compare 1 and 4 workers use `min(4, physical_cores − 1)` workers, and at least 2, on runners with fewer cores.
  
  Verify that every case has a requirement tag and declared tolerances, and that the harness lists exactly the D19 cases and legs as deferred to M14.
- [ ] 3.7 (GOLDEN) Capture `expected/` in three `golden-capture.yml` runs: LVS (after 3.5), SIM and SIMADV (after 3.3 and 3.6). Verify each with a second run on a fresh image build. Verify the run ids, and that every case has stdout, stderr and an exit code plus its declared outputs.

  Note (R0 integration): the five data sets of task 3.1 are not yet wired into `golden-capture.yml`. Per set add an `actions/cache` step with `${{ steps.keys.outputs.<set>_path }}` / `<set>_key` (outputs `gf180mcu_netgen`, `ihp_netgen`, `sky130_ngspice`, `gf180mcu_ngspice`, `ihp_ngspice`) and add the set names to the `fetch_pdk.py` call of the LVS/SIM/SIMADV capture jobs; do this with the first capture run.
- [ ] 3.8 (SPEC) Record the texts that depend on captures: the `comp.out` texts the LVS specs leave to capture (Q-LVS-2 set sections, Q-LVS-13 naming), the vector names of SIM-NET-03 and SIM-UI-04 (Q-SIM-6), and progress on the third run (SIM-RUN-03, Q-SIM-14). Verify `openspec validate --all --strict`, and that no `Resolution:` line changed (otherwise raise it to the user).
- [ ] 3.9 (M8 spec team, xschem clean room spec side; after 3.1, 3.2) Re-capture the schematic simulation cases with results loaded, in the capture image with xschem, the patched ngspice and the `*-ngspice` data sets:
  - SCHEM-SIM-01 (IHP `dc_lv_nmos.sch`: raw vectors and op values, beside the deck M8 kept) and SCHEM-SIM-02 (sky130 `test_inv.sch` with `annotate_op` after a real op run);
  - new SCHEM-SIM cases for the texts that `schematic-symbols-and-libraries` defers to "M9 (SCHEM-SIM-02)" but SCHEM-SIM-02 does not exercise: the gf180mcu `gf180mcu_fd_pr/*fet*.sym` gm/id texts; `annotate_fet_params.sym` and `annotate_bip_params.sym` on gf180mcu, IHP and sky130; the `xschem raw <what>` queries, `xschem raw_clear` and `xschem raw_read` as `schematic-command` lists them; the sky130 `test_customplot.sch` text that combines `tabulate`, `to_eng` and `xschem raw`;
  - the new cases are added to `tests/golden/SCHEM/CASES.md` and `tools/golden/authoring/schem_cases.py` with Hefesto-authored inputs or PDK files only, and get `deferred = "M9"` until task 16.2.
  
  Verify the capture run ids, that `deviations.toml` no longer says "with no simulation loaded" for SCHEM-SIM-02, and `check_cleanroom.py`.

## 4. SCAFFOLD (R1; after G0)

- [x] 4.1 Add the areas `lvs` (subdirectories `net read setup cmp reduce prop hier report cmd`) and `sim` (`engine deck result probe cmd runset meas`), each with `sources.cmake`, `codes.inc` and test directories. Add `docs/diagnostics/codes/{lvs,sim}.md` and `CORE-NOTIMPL-009` (constant `diag::codes::kM9NotImplemented`). Verify that the build passes in `macos-debug` and that the code-table test covers both areas.

  As landed (PR #98 via m9-round0): areas `lvs` and `sim` with per-subdirectory `sources.cmake`/`codes.inc`, test directories `tests/unit/{lvs,sim}/<sub>/`, `docs/diagnostics/codes/{lvs,sim}.md`, `CORE-NOTIMPL-009` (`kM9NotImplemented`); the diag area test covers both areas.
- [x] 4.2 Write the public headers of design "Area contracts" with stubs that return `CORE-NOTIMPL-009`, except `platform/worker_host.hpp` (PLATFORM, task 5.3) and `sim/engine/frames.hpp` (SIMD, task 5.4). Add `lvs::Session` and `sim::Session` to `EditorState`. Record the as-landed contracts in the design. Verify that the headers compile and that `scripts/check_core_includes.py` passes.

  As landed (PR #98 via m9-round0): headers and stubs per design "Area contracts > As landed"; `lvs::Session` and `sim::Session` in `EditorState`; one `[cap:][req:]` test per stub; `NetNameMap` data structure landed (Q-SIM-24 `layout_name` index and its two lookups; the tests build entries by hand); the naming that fills `layout_name` (`sim::node_name`, the irsim rule with no length limit) and the connectivity fallback (`sim::resolve_probe`) are `CORE-NOTIMPL-009` stubs until task 14.2; `sample_seed` fails with `CORE-NOTIMPL-009` (R0 fix pass; it answered 1); pool formula (Q-SIMADV-13) stubbed in `sim/engine/pool.hpp`; `lvs` registered in `cmd/all_commands.cpp` (answers CORE-NOTIMPL-009); `check_core_includes.py` passes.
- [x] 4.3 Add `scripts/check_engine_boundary.py` (design D2): no `ngSpice_` symbol in any binary other than `hefesto-ngspice-adapter` and `libngspice-hefesto`; no release binary other than `hefesto-simd` references the adapter plugin; `sharedspice.h` included only under `src/simd/adapter/`. Run it in CI. Verify with a fixture that violates each rule, and a clean run.

  As landed (PR #98 via m9-round0): `scripts/check_engine_boundary.py` (symbol, plugin and header rules; `--developer-build` turns the plugin rule off for Q-SIM-22) with fixture tests in `scripts/tests/test_check_engine_boundary.py`; every `ci.yml` build leg runs it after Build.

## 5. NGBUILD, SIMD, PLATFORM: engine build, worker, worker service (R1–R2)

- [x] 5.1 (NGBUILD; R1, first on the G2-SIM critical path) Add `third_party/ngspice/` with:
  - the pin of Q-SIM-7 (ngspice-47, which has no upstream CMake build);
  - the patch series (static OSDI registry, static code-model registration, portable seeding, deterministic initialisation);
  - a Hefesto-written CMake wrapper with the feature table, `-ffp-contract=off` and OpenMP off;
  - the shared library `libngspice-hefesto` (Q-SIM-20 (a));
  - `ngspice-manifest.json`, and a source archive that holds every built source file, including the whole OSDI tree.
  
  Verify the build on the three CI legs, that the manifest lists the patches and licences (KLU with AMD, COLAMD, BTF and SuiteSparse_config; numparam; OSDI; sparse; the XSPICE code models), and the `simulation-engine-embedding` scenarios "No GPL code linked" and "Patch list in the manifest".

  As landed (branch `m9-round0-ngbuild`): `pin.json` (ngspice-47 tarball, SHA-256 `894e6496…`, version `47-hefesto1`); four patches (0001 OSDI registry, 0002 code models, 0003 portable seeding, 0004 deterministic initialisation, which also fixes a stock crash when `ngSpice_nospinit()`/`ngSpice_nospiceinit()` run before `ngSpice_Init()`); `cmake/sources.cmake` generated from ngspice's own build by `tools/gen_sources.py`; option `HEFESTO_BUILD_NGSPICE` (default OFF); `.github/workflows/ngspice.yml` (Linux x86_64, Linux arm64, macOS arm64) runs the patch regression tests, the manifest check, the export check and the source archive. cppduals (MPL-2.0, used by HICUM) is in the manifest and the archive besides OSDI. The IHP OSDI modules are task 5.2.
- [ ] 5.2 (NGBUILD) Compile the IHP OSDI modules (PSP103, `cap_cmomi`, `cap_cmomf`) from the `ihp-ngspice` set with the pinned OpenVAF, and register them statically. Verify "Version mismatch avoided" and an sg13g2 `op` deck.

  Deferred to the next round: the `ihp-ngspice` set landed in the same round (3.1), so the modules were not compiled. Ready: patch 0001, `hefesto_static.c` and `cmake/Registries.cmake` (empty OSDI table). Open: OpenVAF-reloaded publishes linux_x64 and win_x64 only, so macOS and arm64 need OpenVAF from source or cross-compiled objects (raised to the user).
- [ ] 5.3 (PLATFORM) Add `platform::WorkerHost` and `WorkerChannel` (design D9) with hosts in `src/cli/` and the desktop shell (`posix_spawn` and pipes); `WorkerSpec` carries the job directory and the read-only roots for the worker's confinement. The core default reports it unsupported. Verify unit tests with a test worker that echoes frames, exits, crashes and hangs, and the `hefesto-architecture` scenarios of "Long-lived worker processes".
- [ ] 5.4 (SIMD) Add `src/simd/`:
  - the `hefesto-ngspice-adapter` plugin (the only includer of `sharedspice.h`; it copies every string and array before returning, and exports Hefesto's engine interface as a C ABI);
  - the frame codec (`hefesto/sim/engine/frames.hpp`) and the IPC server;
  - the heartbeat thread, which never calls the engine;
  - the worker confinement of design D9 (macOS sandbox profile and the disable-library-validation entitlement for `hefesto-simd` only; Linux no-new-privileges, seccomp and Landlock), applied before the plugin is loaded;
  - the reuse sequence between jobs (`remcirc`, `destroy all`, the initialisation commands, `setseed`).
  
  Verify the adapter tests; "Engine adapter boundary", "Callback contract" and "Deterministic engine initialisation" through the worker; "Large deck load" (heartbeats during a 25 s load); "Deck tries to reach the network" on Linux and its macOS equivalent; and a 50-job sequence in one worker whose results equal the same jobs each run in a fresh worker.
- [ ] 5.5 (SIMENG) Add `sim/engine/`: the `Engine` interface, `WorkerEngine` and `JobManager`, with the fault policy of D-25 (replace the worker on any fault, reuse it otherwise), the pool formula of "Multiple simulations and corner parallelism" and the limits of Q-SIM-25. The in-process engine is a developer-build host that loads the adapter plugin (Q-SIM-22). Verify:
  - "ngspice segmentation fault", "Controlled exit replaces the worker" and "Unrecoverable error";
  - a missed heartbeat and an oversize frame;
  - "Single-core host" and "Setting cannot raise the pool";
  - `fuzz_sim_frames` for 10 minutes with no finding;
  - the requirement coverage of `simulation-engine-embedding`, with the iPadOS-only requirements and the scenario "Reset on iPadOS" listed as deferred to M14.

## 6. LVSREAD: lvs-netlist-input (LVS implementation team; R2; after G1, G2-LVS)

- [ ] 6.1 Netlist database, device classes, format selection, name matching with the unsigned-byte fold (Q-LVS-3), and SI values. Verify the requirement tests and NG-NAME-01..03.
- [ ] 6.2 SPICE and CDL: lexical structure, subcircuit definitions, primitive cards, calls and placeholders, globals, includes and parameters (Q-LVS-14), library and black-box reads, and in-memory sources and limits (Q-LVS-36). Verify the tests, NG-READ cases and `fuzz_lvs_spice`.
- [ ] 6.3 Structural Verilog, `.sim`, and legacy `.ext` per Q-LVS-1 (no GUI entry). Verify the tests, the remaining NG-READ cases, `fuzz_lvs_verilog` and the coverage of `lvs-netlist-input`.

## 7. LVSSETUP: lvs-setup-file (LVS implementation team; R2; after G1, G2-LVS)

- [ ] 7.1 Setup language and runtime in the X-3 sandbox, evaluation, cell reference syntax, `permute`, and every `property` sub-command (Q-LVS-6). Verify the tests, one test per mode for each compat entry, and NG-SET cases.
- [ ] 7.2 `equate classes` (Q-LVS-29 fix), `ignore`, `model`, `global`/`convert`, `flatten`, and the query and output commands. Verify the tests, NG-PDK-01..03 with the three PDK setup files, `fuzz_lvs_setup`, and the coverage of `lvs-setup-file`.

## 8. LVSALG: lvs-comparison-algorithm (LVS implementation team; R2; after G1, G2-LVS)

- [ ] 8.1 Comparison graph, class identity and pin labels, initial partition, legality, and iterative refinement with the seeded generator and the collision check (Q-LVS-4). Verify the tests and NG-ALG cases.
- [ ] 8.2 Match verification, symmetry resolution, `verify`, forced equivalences, deterministic arbitrary pairing, long runs and cancellation, and reset and stepping (Q-LVS-21). Verify the tests, the single-threaded determinism test (the same pair 100 times, identical reports), and the coverage of `lvs-comparison-algorithm`.

## 9. LVSRED and LVSPROP: lvs-device-reduction, lvs-property-matching (LVS implementation team; R3; after LVS R2)

- [ ] 9.1 Combination flags, schedule, parallel and series reduction (Q-LVS-26), permutable pins, zero-valued removal, merged counts, and preservation of pin order and names. The O(n log n) bound is checked by a 1M-finger test. Verify the tests, NG-RED cases and the coverage of `lvs-device-reduction`.
- [ ] 9.2 Properties of interest, value types, tolerance, missing properties, timing of checks, pin-associated properties, series/parallel reconciliation, property errors (Q-LVS-10), and strict and relaxed topology. Verify the tests, NG-PROP cases and the coverage of `lvs-property-matching`.

## 10. LVSHIER and LVSREP: lvs-hierarchy, lvs-reports (LVS implementation team; R3; after LVS R2)

- [ ] 10.1 Compare queue, cell-pair preparation, pre-match flattening (Q-LVS-11), flattening semantics, duplicate and shorted pins, the per-pair flow, pin matching (Q-LVS-17), black boxes, the final result, and the iterative depth limit. Verify the tests, NG-HIER and NG-PIN cases, and the coverage of `lvs-hierarchy`.
- [ ] 10.2 Log and console routing, the two-column layout, the summary, net, device, pin and property reports, the list result, JSON (Q-LVS-13, Q-LVS-19), classification, the canonical comparison (Q-LVS-2), and layout references in the structured result. Verify the tests, NG-REP cases through `lvs_canonical.py`, "Unmatched net points into the layout" (outputs byte-identical with and without references), and the coverage of `lvs-reports`.

## 11. MAPS: name maps from M7 and M8 (R4a; after M7 and M8 exits)

- [ ] 11.1 (M7 owner) `netlist/name_map.hpp` (with `layout_name` and its index, design D4) and `netlist::spice::render_with_map` (Q-LVS-33 (a)). `render` stays unchanged. Also the open item of design D16: `ensure_extracted` treats a `.ext` whose `tech` or `style` line differs from the current ones as stale. Verify that the text equals `render`'s on every NETLIST golden case, that every net and device of the NETLIST-PDK cases has a map entry with a layout point inside its node, and an `ensure_extracted` test that changes the extract style and expects every cell re-extracted.
- [ ] 11.2 (M8 owner, M8 clean room) `schem::NetlistResult::names`. Verify that the netlist text is unchanged on every SCHEM-NL case, and that every net and instance maps to a schematic object.

## 12. LVSCMD: lvs-command (LVS implementation team, no netgen reader; R4a; after G3, G4, 11)

- [ ] 12.1 `lvs` syntax, the `netgen` namespace (D-34), and setup and input resolution (Q-LVS-31, with the `ihp-sg13g2` scenario). Verify the tests and NG-CMD-01.
- [ ] 12.2 The layout source through `ensure_extracted` and `render_with_map` (Q-LVS-32), the `.sch` source through `schem::netlist`, the execution model on the background worker with the database read-only, and headless LVS. Verify "One-step LVS on iPad" at the core level on Linux, "Rerun after editing one cell", "Extract style changed", and `hefesto-cli` LVS of sky130 `inv_1`.
- [ ] 12.3 The result model, the browser view model, cross-probing (report → layout, report → schematic) and the status chip with its six states (`LVS not run`, `LVS match`, `LVS mismatch (<n>)`, `LVS error`, `LVS cancelled`, `LVS stale`). Verify the view-model tests and the coverage of `lvs-command`.

## 13. SIMDECK and SIMRES: preparation, analyses and results (R2 for raw and SPICE sources, R4a for layout and schematic)

- [ ] 13.1 (R2; after G2-SIM) The `spice` and `raw` sources, testbench composition, stimuli, deck validation and sanitisation (Q-SIM-9), deck limits (Q-SIM-25), and deck delivery. Verify the tests and SIM-NET cases on SPICE sources.
- [ ] 13.2 (R2; after G2-SIM) Model descriptors in the PDK store for the three PDKs (Q-SIM-8, D-26, D-27, as their `Verified` lines select), model library resolution (Q-SIM-13), PDK initialisation commands, and corners in the descriptor. Verify the descriptor tests against the pinned data sets and SIM-NET PDK cases.
- [ ] 13.3 (R2, SIMRES; after G2-SIM) Analyses, job lifecycle, progress (Q-SIM-14), pause, resume and cancel, final vectors, saved vectors and the memory budget (Q-SIM-16), storage, raw format (Q-SIM-17), CSV, raw import with its cap, and waveform measurements. Verify the tests, SIM-RES and SIM-RUN cases, `fuzz_sim_raw`, and the coverage of `simulation-analyses-and-results`.
- [ ] 13.4 (R4a; after G3, G4, 11) The layout DUT through `ensure_extracted` and `render_with_map`, parasitic modes `none`, `c` and `rc` (`extresist::run`), the net name map with `layoutName` computed by SIMPROBE's naming function at each entry's point ("Net name map", delta), case collisions (Q-SIM-6), the schematic source through `schem::build_deck`, and `mm_ok=1` for sg13g2 (D-23). Verify the tests, "Renamed node keeps its layout name", the remaining SIM-NET cases, and the coverage of `simulation-netlist-preparation`.

## 14. SIMPROBE and SIMCMD (R3 for the command, R4b for probes)

- [ ] 14.1 (R3, SIMCMD; after simulation R2) Option matching, the subcommand table, numeric syntax, script returns and errors, headless and batch use, the engine subcommand, testbench persistence (with D-10's undo events), and schematic launcher calls. Verify the tests, SIM-CMD cases, "CI corner sweep", and the coverage of `simulation-command`.
- [ ] 14.2 (R4b; after 13.4) Hierarchical node naming brought forward from `irsim-interface` (Q-SIM-24; tests tagged to that requirement; a name longer than 256 bytes), probe resolution through the `layout_name` index and then connectivity (design D4), layout and device-current probes, trace highlighting, operating-point annotation as overlays, and the 3D overlay feed. Verify the tests, "Netlist name differs from the node name", and SIM-UI cases.

## 15. SIMADV and SIMMEAS: sweeps, corners, Monte Carlo, measurements (R3; after simulation R2)

- [ ] 15.1 (SIMADV) Run sets, sweep variables, point generation, point decks with overrides per D-22 (Q-SIMADV-9), DOE, execution on the pool (Q-SIMADV-13) with worker reuse between points (design D9), storage, result views, command options, and the `measured` retention mode (D-24, Q-SIMADV-11). Verify the tests, SIMADV-SW cases, the 1-against-4-worker determinism test (or 1 against `min(4, physical_cores − 1)` on smaller runners), and the coverage of `simulation-sweeps`.
- [ ] 15.2 (SIMADV) Corners: descriptor corners, bundled tables, user corners and sets, the corner dimension, pass and fail, the matrix, and run all and re-run failed. Verify the tests, SIMADV-CO cases, "Large post-layout deck limits the pool" (exactly one worker), and the coverage of `simulation-corners`.
- [ ] 15.3 (SIMADV) Monte Carlo: modes (Q-SIMADV-3, -4, -5, -8), the MC dimension, sample seeds, the portable RNG, reproducibility, parallel and sequential runs, failed samples and early stop, user statistical parameters, and options. Verify the tests, SIMADV-MC cases, a sample's result being identical on worker 1 of 1 and worker 3 of 4 (each worker having run different samples before), and the coverage of `simulation-monte-carlo`.
- [ ] 15.4 (SIMMEAS) Definitions, `.meas` import, expressions, specs, descriptive statistics, yield, Cpk, histograms, correlation, export and options, with fixed-order compensated sums and the debug-build `.meas` cross-check (Q-SIMADV-10). Verify the tests, SIMADV-MEAS cases including MEAS-04, `fuzz_sim_meas`, and the coverage of `simulation-measurements-and-statistics`.

## 16. SCHEMSIM: schematic-simulation-integration, M9 part (M8 implementation team, xschem clean room; R4b; after G4)

- [ ] 16.1 (after 13.4) Run the schematic circuit source, make the launchers complete (remove the not-implemented answer that M8 leaves on the `sim` path), and add `xschem simulate` and the `xschem raw` queries. Verify the requirement tests and SCHEM-SIM-01 as re-captured in task 3.9.
- [ ] 16.2 (after 13.4, 3.9) Operating-point annotation, embedded graphs, waveform cross-probing, and results interchange with xschem (raw read and write). The M8 spec team pins the exact outputs of `to_eng`, `tabulate` and `ngspice::` from the task 3.9 captures in `schematic-symbols-and-libraries`. Verify the tests, SCHEM-SIM-02 and the new SCHEM-SIM cases of task 3.9.
- [ ] 16.3 (after 12; no netgen reader) Schematic-to-layout cross-probing and the schematic-driven LVS command. Verify the tests, SCHEM-LVS-01 (under Q-SCHEM-39 (a)), and `check_requirement_coverage.py --capability schematic-simulation-integration` with nothing deferred.

## 17. DESIGN and SHELL: desktop (R3 boards, R4b screens; design Open Question 3)

- [ ] 17.1 (DESIGN) Draw `docs/design/mockups/LVS-Browser.dc.html` and `Waveforms.dc.html` (design D23), and seed `docs/design/m9-ui-conformance.md`. Verify the board review note.
- [ ] 17.2 (SHELL; after 12.3, 14.2, 17.1) The LVS status chip, the result browser and cross-probe highlights; the Simulate button and testbench panel with the engine-unavailable tooltip; the Waveforms tab and viewer (cursors, readouts, complex display, live traces, decimated rendering); run-set, corner, Monte Carlo and yield views; correction of the stale M8 labels. Verify the shell view-model tests, the conformance checks with screenshots, and "Glitch remains visible".

## 18. SECURITY (reviewer outside both LVS teams; R4a)

- [ ] 18.1 Review the setup-file sandbox, deck sanitisation, the core's frame parser, raw import and the worker confinement (design D22) with hostile inputs, and add each one as a unit or integration test. For the worker: a deck that tries a network socket, a write outside the job directory, a write into the PDK store and a process spawn, on macOS and Linux; and a check that only `hefesto-simd` carries the library-validation entitlement. Verify that every denial gives its diagnostic or OS error, and record the sign-off with its date in the M9 exit table.

## 19. PERF (R5)

- [ ] 19.1 `lvs-bench` (Q-LVS-35): callgrind counts against netgen on the three workloads, each comparison isolated by subtraction (the script with the comparison minus the same script without it; no per-function filter on netgen's internal names), recorded in `bench/lvs/` and run in `perf.yml`. First measure the 1M-device netgen run under callgrind and check that the three workloads fit `perf.yml`'s job time limit; if not, move the 1M-device workload to the nightly benchmark job. Verify the counts on the x86_64 runner and the `hefesto-performance` "LVS and simulation throughput" scenarios.
- [ ] 19.2 `sim-bench` (Q-SIM-28): the overhead ratio on the three decks against ngspice's `rusage` analysis time (the 1 000-point sweep including worker reuse), Monte Carlo scaling for `w` up to `min(4, physical_cores − 1)` of each runner (`w = 4` on the M3 Pro or a larger runner named in `perf.yml`), and the storage cost of the `measured` mode (D-24 default decision). Verify recorded baselines on the M3 Pro and the Linux runner, and report the `measured` storage numbers to the user.

## 20. INTEGRATION (every round; exit in R6)

- [ ] 20.1 Licensing:
  - `THIRD_PARTY_NOTICES.md` rows for KLU and the SuiteSparse parts it brings (AMD, COLAMD, BTF, SuiteSparse_config), numparam, OSDI, the XSPICE code models and the compiled-in OSDI models, OpenVAF noted as build-only, and netgen absent (design D2);
  - `docs/building.md` "Replacing the ngspice library", including ad-hoc signing on macOS;
  - the legal review of the desktop linking (D-28, Q-SIM-20), recorded before the Desktop Beta; if it changes the link type, the affected tasks are revised with the user.
  
  Verify `check_third_party_notices.py`, the `hefesto-licensing` scenarios (including "Rebuilt library on macOS" on a notarized build), and the recorded review.

  Progress (R0 integration): `THIRD_PARTY_NOTICES.md` rows for ngspice 47-hefesto1 and its KLU/AMD/COLAMD/BTF/SuiteSparse_config, numparam, OSDI, cppduals, sparse and XSPICE code-model parts, OpenVAF-reloaded as build-only, and netgen absent; `docs/building.md` "Replacing the ngspice library". Open: the compiled-in IHP OSDI models (task 5.2) and the legal review (D-28).
- [ ] 20.2 Desktop Beta end-to-end cases BETA-E2E-SKY130, -GF180 and -IHP (Q-SIM-27, design D19 "Beta flow inputs"): `beta_inv.sch` drawn in Hefesto and simulated in `beta_tb.sch` with the FO4 load; `beta_inv` layout drawn in Hefesto around the library inverter, clean under Hefesto's DRC with the PDK's Magic tech file, written to GDS; extraction; an LVS match against `beta_inv.sch` (plus the PDK `sg13g2_inv_1.sch` cross-check for IHP); `rc` post-layout simulation in the same testbench with `tpd` within 30 % of the schematic run. Verify them on Linux x86_64 and macOS arm64 in CI.
- [ ] 20.3 Docs: `docs/diagnostics/codes/{lvs,sim}.md` complete; `docs/building.md` (ngspice, OpenVAF, data sets); `docs/contributing/` (areas, fuzz targets, golden modules); the CHANGELOG; the roadmap M9 status; `docs/design/m1-ui-conformance.md` rows. Verify the docs checks.
- [ ] 20.4 M9 exit check (design D25): the golden suites (with only the D19 iPadOS cases deferred to M14), coverage of the 17 capabilities plus the M9 part of `schematic-simulation-integration`, three-platform identity and tolerance, both-mode tests, `check_engine_boundary.py`, `check_cleanroom.py`, the security sign-off, the legal review, the PERF gates, retirement of `CORE-NOTIMPL-009`, and `openspec validate --all --strict`. Verify each item in the M9 exit table with run ids.
- [ ] 20.5 The user's Desktop Beta sign-off on Linux and macOS with real pointer and keyboard input (Q-SIM-27), redrawing `beta_inv.sch` and `beta_inv` by hand. Verify that the sign-off is recorded with its date in the roadmap.
- [ ] 20.6 Archive the change with `openspec archive m9-lvs-simulation`. The deltas were synced at G0 and `skip_specs: true` is set (task 1.2), so the archive moves the change folder only. Verify that `git diff --stat openspec/specs/` is empty after the archive, that no requirement heading appears twice in any living spec, and `openspec validate --all --strict`.

## Parallel workstreams per round

| Round | In parallel | Order inside the round |
|---|---|---|
| R0 | INTEGRATION (1.1, 1.3), SPEC (1.2, 1.4) | 1.1 → 1.2 |
| R1 | CLEANROOM (2), PDK (3.1), GOLDEN (3.2–3.9), NGBUILD (5.1–5.2), PLATFORM (5.3), SCAFFOLD (4) | SIM: 5.1 → 3.2 → 3.3, 3.6 → 3.7 → 3.8 (G2-SIM critical path); 3.9 after 3.2. LVS: 2.1–2.2 → 3.5 → 3.7 → 3.8 |
| R2 | LVSREAD (6), LVSSETUP (7), LVSALG (8) · SIMD (5.4), SIMENG (5.5) · SIMDECK (13.1–13.2), SIMRES (13.3) | LVS after G1 and G2-LVS; simulation after G2-SIM; the two streams do not wait for each other |
| R3 | LVSRED/LVSPROP (9), LVSHIER/LVSREP (10) · SIMCMD (14.1) · SIMADV (15.1–15.3), SIMMEAS (15.4) · DESIGN (17.1) | each stream after its own R2 |
| R4a | MAPS (11) · SECURITY (18) | 11 → LVSCMD (12) and SIMDECK (13.4) |
| R4b | SIMPROBE (14.2), SCHEMSIM (16), SHELL (17.2) | 13.4 → 14.2, 16.1, 16.2; 12 → 16.3; 12.3 and 14.2 → 17.2 |
| R5 | GOLDEN SCHEM and Beta cases (16, 20.2), PERF (19) | after R4b |
| R6 | INTEGRATION exit (20.1, 20.3–20.6) | 20.4 → 20.5 → 20.6 |
