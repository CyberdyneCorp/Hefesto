# Tasks

How this plan runs:
- **Every decision this plan cites was approved on 2026-11-13** (`docs/decisions/m8-decision-table.md`: the 24 open SCHEM entries, D-21 and J-1 to J-5, as recommended). Design Open Questions 1–3 are pending; the tasks below follow their recommended options and name them where an answer would change a task.
- Gates (design D1): sections 1 (CLEANROOM) and 2 (PDK) first; section 3 (GOLDEN) and section 4 (SPEC) next; implementation sections 5–14 start only after G1 (section 1 done) and G2 (task 3.6 done), and DEVLIB and the Tcl part of SYMLIB after G3 (section 4 done). When implementation starts relative to M6/M7 is Open Question 3.
- **Clean room (J-1).** Each section names its team. Implementation-team tasks never read xschem's source, never follow `Evidence:` lines or `(xschem ref: ...)` traces, and raise spec gaps as spec issues for the spec team. Spec-team members do not take implementation tasks.
- Each task ends with how it is verified. Workstreams edit only what design D3 assigns to them; shared-file needs are reported as open items.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Requirements that cite a `Resolution: compat` entry get one test per `xschem-compat` mode.
- The last task of each capability workstream also runs `scripts/check_requirement_coverage.py --capability <cap>` and verifies that every requirement is covered or listed as deferred with a reason (design D13 for the M9 parts).

## 1. CLEANROOM: the two-team process (spec team; first, gate G1)

- [x] 1.0 Decisions recorded: `Status:`/`Resolution:` on every `SCHEM.md` entry, D-21 in X-2 and `audit/DECISIONS.md`, the decision table marked approved, and the spec edits applied, including the two-team requirement of `hefesto-licensing`. Verified by `check_milestone_readiness.py --milestone M8` (ready) and `openspec validate --all --strict`.
- [ ] 1.1 Write `docs/contributing/cleanroom.md`: the two teams, which workstreams belong to each (design D3), the rules of design D2 for implementation sessions (worktree only, no xschem repository or `/opt/src/xschem`, no `Evidence:`/`xschem ref` paths, the public manual allowed), how spec issues are raised, and the roster by role. Link it from `docs/contributing/README.md`. Verify that a reviewer outside both teams confirms each rule of `hefesto-licensing` "Two-team clean-room process for specs written from GPL source" maps to a written rule.
- [ ] 1.2 Review the six `schematic-*` specs and `SCHEM.md` for code paraphrase (design D2): source identifiers that are not format or user-visible names, awk or Tcl program structure, pseudo-code that mirrors C. Rewrite each finding as behaviour, starting with `schematic-netlisting` "Element line post-processing" and "Line breaking". Record the findings and rewrites in `docs/decisions/m8-paraphrase-review.md`. Verify that `openspec validate --all --strict` passes and a second spec-team reviewer signs the record.
- [ ] 1.3 Add `scripts/check_cleanroom.py` with tests in `scripts/tests/`: fail on a committed file carrying xschem's GPL header, on a `tests/golden/SCHEM/` case without `provenance` or with an xschem source, and on an `xschem ref:` trace under `src/`. Run it in `openspec-validate.yml`. Verify with fixture trees for each failure and a clean run on main.
- [ ] 1.4 Add the clean-room checkbox for schematic code to the pull-request template, and the "author not on the spec team" item to the review checklist. Verify that the template renders and `docs/contributing/README.md` points to it.
- [ ] 1.5 Record the counsel review of the process and of the system-library interface table as an M13 exit item (roadmap de-risking schedule "Before Desktop 1.0"). Verify that the roadmap row exists and names J-1 and Q-SCHEM-10.

## 2. PDK: xschem data sets and the inventory (either team; first)

- [ ] 2.1 Settle design Open Question 1: list `libs.tech/xschem` (and `libs.ref/*/sym/xschem`) in the ciel releases of M5 for sky130, gf180mcu and IHP, compute tree digests and compare them with the pinned commits of `SCHEM.md` (`xschem_sky130` @ `2bfd7d65`, `gf180mcu_fd_pr` @ `aacc04cc`, IHP @ `5e6d592e`). Record sizes, digests and the decision in design D17. Verify that the recorded facts reproduce from the documented commands.
- [ ] 2.2 Add `sky130-xschem`, `gf180mcu-xschem` and `ihp-xschem` to `scripts/fetch_pdk.py` (pins, subsets, tree digests, cache keys, `--print-digest`), their paths to `tests/unit/common/reference_data.hpp` (clean skip when missing), and `docs/building.md`. Verify that a fresh install matches the digests, a second run is a verified no-op, and the `scripts/tests/` tests pass.
- [ ] 2.3 Write `scripts/schem_inventory.py` (design D18): referenced `devices/` symbols, Tcl commands and `$::` variables in `tcleval(` bodies and `xschemrc` files, `xschem` subcommands, subcircuit symbols with multi-line templates. It reads only the three data sets. Commit `tests/data/schem/inventory.json` and tests on fixture trees. Verify that two runs give identical output and that the IHP and sky130 lists include `code_shown.sym`, `ev7` and `SKYWATER_MODELS`.
- [ ] 2.4 Add the CI step that re-runs the inventory on the cached data sets and fails on a difference from the committed file. Verify by a deliberate fixture change that fails and a clean run that passes.
- [ ] 2.5 Report the multi-line-template result (Q-SCHEM-21) to the spec team: if no PDK subcircuit symbol has one, both modes are identical on PDK data, and GOLDEN adds a Hefesto-authored case (task 3.3). Verify that the result is recorded in `SCHEM.md` under Q-SCHEM-21 as a `Verified` line.

## 3. GOLDEN: SCHEM cases and capture (implementation team for inputs; gate G2)

- [ ] 3.1 Install `gawk` in `tools/golden/container/Containerfile` and make it `/usr/bin/awk`, with a build-time check that `awk --version` reports GNU Awk; update `pins.toml` and `tools/golden/tests/test_pins.py` if the APT snapshot changes. Verify a CI image build and a capture-job log line showing GNU Awk.
- [ ] 3.2 Mount the xschem data sets read-only in the capture job and set `xschem_execute_scripts` to `yes` for SCHEM captures (an rc fragment in the harness, not in the PDK files). Verify with a dispatch run of one SCHEM case per PDK.
- [ ] 3.3 Author the 41 cases of `tests/golden/SCHEM/CASES.md` as `case.toml` plus inputs, each with `provenance` (design D16). Every input that is not a PDK file (the hand-written and Hefesto-authored inputs of `CASES.md`, including SCHEM-RT-05, NL-01, NL-19 and the Q-SCHEM-21 case of task 2.5) is written from the specs only. Verify that `check_cleanroom.py` passes and every case has a requirement tag.
- [ ] 3.4 Add `tools/golden/compare/spice_canonical.py` (design D10) with tests on hand-written equivalent and non-equivalent netlists, and the path and version normalisers for SCHEM cases. Verify the tests, including a node-renaming and a case-only difference that pass and a swapped-pin difference that fails.
- [ ] 3.5 Capture `expected/` for every SCHEM case in `golden-capture.yml`. Verify the run id, that every case has stdout, stderr and exit code, and that SCHEM-RT-02 lists the PDK files whose second save differs.
- [ ] 3.6 (spec team) Record the capture-fixed texts: Q-SCHEM-14 (SCHEM-NL-16), Q-SCHEM-21 (task 2.5 case), Q-SCHEM-22 (SCHEM-NL-01 and a user-code line), Q-SCHEM-27 (SCHEM-NL-14), Q-SCHEM-28 (SCHEM-NL-15), in the specs and as `Verified` lines in `SCHEM.md`; check SCHEM-NL-21 against the D10 comparator; correct any spec rule a capture contradicts and record it. Verify that `openspec validate --all --strict` passes and that no entry's `Resolution:` changed (otherwise raise it to the user).

## 4. SPEC: interface table and Tcl semantics (spec team; gate G3)

- [ ] 4.1 For every symbol in the inventory's `devices/` list, extract its interface (file name, pin names, order, positions, directions, `type`, `format`, `lvs_format`, `template`, `global` and other netlisting properties, bounding box) into `schematic-symbols-and-libraries` "System library" (design D8). Graphics are not described. Verify that every inventoried symbol has a row and `openspec validate --all --strict` passes.
- [ ] 4.2 Generate `tests/data/schem/devices_interface.json` from that table with a script, and add a test that the JSON and the spec agree. Verify the test.
- [ ] 4.3 Specify the semantics of every inventoried Tcl procedure and variable (`ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::*`, the `xschem` subset) in "tcleval values" from the public xschem manual and expected values computed for each PDK use, with a table of inputs and outputs per procedure (Q-SCHEM-8). Verify that each PDK use in the inventory has an expected value.

## 5. SCAFFOLD and COMPAT (implementation team; after G1)

- [ ] 5.1 Add the area `schem` (subdirectories `file`, `sym`, `lib`, `tcl`, `net`, `edit`, `cmd`, `sim`) with `sources.cmake`, `codes.inc`, test directories and `docs/diagnostics/codes/schem.md` with one section per workstream (design D3). Verify that the build passes and the code-table test covers `schem`.
- [ ] 5.2 Write the public headers of design "Area contracts" with stubs, and `schem::Session` in `EditorState` (design D4). Verify that the headers compile in `macos-debug` and `macos-bench` and `scripts/check_core_includes.py` passes.
- [ ] 5.3 Add `xschem_compat` to `compat::CompatSettings`, the settings `xschem-compat` and `portable-netlist-paths` to `settings::ProjectSettings` (names, JSON, fixed order) and `xschem_compat_netlist_line` (design D5). Change the unknown-name test in `tests/unit/core/settings_test.cpp` to another name. Verify the `[cap:hefesto-magic-compat-mode]` and `[cap:hefesto-settings]` tests, including "xschem-compat off leaves layout and LVS output unchanged" (an `.ext` written before and after switching is byte-identical) and "magic-compat off leaves schematic netlists unchanged" (after task 8.6).

## 6. FORMAT: schematic-file-format (implementation team; after G2)

- [ ] 6.1 Lexer, record grammar, brace strings, numeric fields (source text kept, printing by `to_chars`), layers, graphic, text, wire and instance records, property strings and boolean values. Verify the `[cap:schematic-file-format]` tests for those requirements and SCHEM-RT-03, RT-06 and PR-01.
- [ ] 6.2 Version record and header text with the Q-SCHEM-3 branch point, the writer version string (Q-SCHEM-1), global property records. Verify one test per mode for "Leading instance record" and SCHEM-RT-04, RT-07, RT-08.
- [ ] 6.3 Embedded symbols written as xschem does in both modes (Q-SCHEM-2), coordinate system, orientation transform, canonical write order. Verify SCHEM-RT-05 and the orientation tables.
- [ ] 6.4 Round-trip guarantee, malformed input and safe saving; the fixpoint property test; `fuzz_sch_reader` and `fuzz_schem_props` (design D19). Verify SCHEM-RT-01 and RT-02 on every PDK file, the property test, and a 10-minute fuzz run per target with no finding.
- [ ] 6.5 Requirement coverage for `schematic-file-format`. Verify `check_requirement_coverage.py --capability schematic-file-format`.

## 7. SYMLIB and DEVLIB: schematic-symbols-and-libraries (implementation team; after G2, Tcl and DEVLIB after G3)

- [ ] 7.1 Symbol property, types, pins, pin order with the stable sort and duplicate warning (Q-SCHEM-5), templates, instance names, floater texts, text tokens. Verify the requirement tests, "Duplicate sim_pinnumber", SCHEM-PR-02, PR-03 and PR-04.
- [ ] 7.2 Symbol references (with the `http` placeholder and warning, Q-SCHEM-9), library search path, schematics used as symbols, missing symbols, symbol cache and reload. Verify the tests, "Web reference not fetched" and SCHEM-PR-05.
- [ ] 7.3 `xschemrc` as data in the sandbox with synthetic `env()` (Q-SCHEM-23); `xschem_execute_scripts` ignored and logged (Q-SCHEM-25). Verify "IHP library path" and "xschem_execute_scripts ignored" for all three PDK `xschemrc` files.
- [ ] 7.4 (after G3) `tcleval` and `@tcleval` with the inventoried procedures and variables implemented from task 4.3's semantics; the permission model (sandbox always, launchers confirmed once per document); symbol `drc` procedures; Tcl-only generators. Verify "sky130 corner include", "IHP ev7 expression", the task 4.3 tables, "Launcher confirmation", and the inventory test that fails when a procedure is missing.
- [ ] 7.5 (DEVLIB, after G3) Draw every inventoried `devices/` symbol under `assets/schematic/devices/` from the interface table, with independent graphics and Apache-2.0 headers (design D8, Open Question 2). Verify the interface test against `devices_interface.json`, that the library holds exactly the inventoried symbols, and "IHP testbench with devices prefix".
- [ ] 7.6 PDK attribute conventions over the three PDKs. Verify SCHEM-NL-07..09 element lines once NETLIST lands, and the requirement coverage for `schematic-symbols-and-libraries`.

## 8. NETLIST: schematic-netlisting (implementation team; after G2)

- [ ] 8.1 Connectivity and naming: connection geometry, net naming order with every short reported (Q-SCHEM-16), unnamed nets, pass-through and shorted symbols, bus taps, bus and vector notation, global nets, computed `lab` values (Q-SCHEM-29). Verify the tests, "Top-level short always reported", `fuzz_schem_bus`, SCHEM-NL-11, NL-19 and NL-22.
- [ ] 8.2 Format substitution with the token-prefix warning (Q-SCHEM-26), hierarchical parameters, `expr` with the Q-SCHEM-12 branch point and warning, `fuzz_schem_expr`. Verify "Token ends at whitespace only", both "Minus inside a number" scenarios and SCHEM-NL-13.
- [ ] 8.3 Element line post-processing with the Q-SCHEM-22 branch point and gawk semantics (Q-SCHEM-18), and line breaking (Q-SCHEM-20), written from the spec as reviewed in task 1.2. Verify "Trailing m=1 in user code" in both modes, "Long device line" and SCHEM-NL-01, NL-12.
- [ ] 8.4 File structure (with the `** xschem-compat=off` line and portable paths, Q-SCHEM-13), subcircuit body, top-level port order (stable, Q-SCHEM-5), subcircuit definitions with the Q-SCHEM-14, -19 and -21 branch points and warnings, schematic resolution (Q-SCHEM-27, -28), device models. Verify the scenarios of each, one test per mode for the three compat rules, and SCHEM-NL-03..06, NL-14..18, NL-20.
- [ ] 8.5 LVS netlist mode, flat mode compared by the canonical comparator (Q-SCHEM-15), errors and warnings, in-process netlisting. Verify "LVS inverter", "Flat netlist compared behaviourally", SCHEM-NL-10 and NL-21.
- [ ] 8.6 The cross-platform digest test of design D20 over every PDK example schematic; requirement coverage for `schematic-netlisting`. Verify equal digests on the three CI legs and `check_requirement_coverage.py --capability schematic-netlisting`.

## 9. EDIT: schematic-editing (implementation team; after FORMAT)

- [ ] 9.1 Document model, snap and grid, selection, wires, placing instances, move/copy/stretch, rotate and flip, labels and ports, bus editing. Verify the requirement tests.
- [ ] 9.2 Property editing, graphics and text, object order, clipboard, hierarchy navigation and path, net highlighting, per-document undo and redo, read-only libraries. Verify the tests, "Undo after ascending" and "Edit PDK symbol".
- [ ] 9.3 The floating tool palette view model and the touch gesture mapping as headless view models (Q-SCHEM-24; the iPad presentation is M14). Verify that each palette tool and gesture issues the same command as its desktop binding ("Rotate on iPad", "Wire from the palette on iPad" at the command level) and the requirement coverage for `schematic-editing` (iPad presentation deferred to M14).

## 10. CMD: schematic-command (implementation team; after FORMAT)

- [ ] 10.1 `schematic` option matching, the subcommand table, return values and errors in scripts, headless use, command logging. Verify the requirement tests, the replay test and SCHEM-CMD-01.
- [ ] 10.2 The `xschem` Tcl command with the inventoried subcommands and the unsupported-subcommand error; the inventory test (Q-SCHEM-10). Verify "Instance property from Tcl", "Unsupported subcommand" and the requirement coverage for `schematic-command`.

## 11. SIMINT: schematic-simulation-integration, M8 part (implementation team; after NETLIST)

- [ ] 11.1 Schematic circuit source (deck text and the hand-off to `simulation-netlist-preparation` validation), testbench code blocks, control block translation, simulator vector names, launchers (sandboxed, confirmed once; `sim` calls answer CORE-NOTIMPL until M9). Verify the requirement tests and SCHEM-SIM-01's deck and SCHEM-SIM-03's vector names (the simulation results are M9).
- [ ] 11.2 List operating-point annotation, embedded graphs, waveform and layout cross-probing, the schematic-driven LVS command and results interchange as deferred to M9 with design D13's reason, and add them to the M9 roadmap entry. Verify `check_requirement_coverage.py --capability schematic-simulation-integration` and the roadmap text.

## 12. SECURITY: script permission review (reviewer outside both teams)

- [ ] 12.1 Review the schematic script paths against `hefesto-security` "Script sandbox enforcement" and "Schematic script permission model" (design D14): the PDK testbenches of the three PDKs and hostile schematics (exec, socket, file access outside granted folders, `load`, nested interpreters, launcher replay). Add the hostile cases as unit tests. Verify that every denial gives the sandbox diagnostic and the value `?`, and record the sign-off with its date in the M8 exit table.

## 13. DESIGN and SHELL: the desktop schematic editor (implementation team; after EDIT)

- [ ] 13.1 Review `docs/design/mockups/Schematic.dc.html` against the M8 scope and seed `docs/design/m8-ui-conformance.md` (canvas, library browser, properties, hierarchy, netlist action, messages; M9 parts shown disabled with their milestone). Verify the board review note.
- [ ] 13.2 Enable the Schematic tab; implement the canvas, library browser, properties panel, hierarchy breadcrumb, netlist action, net highlighting and xschem-style rebindable bindings; relabel the Waveforms tab "arrive in M9". Verify the shell view-model tests, the conformance checks with screenshots, and the key-binding sheet test.

## 14. PERF and INTEGRATION

- [ ] 14.1 `schem-bench`: load, save and netlist time for the largest PDK example schematics and a generated 10k-instance hierarchy, recorded in `bench/schem/` and run in `perf.yml`. Verify recorded baselines on the M3 Pro and the Linux runner.
- [ ] 14.2 Docs: `docs/diagnostics/codes/schem.md` complete, `docs/contributing/` (areas, fuzz targets, the SCHEM golden module), `docs/building.md`, CHANGELOG and the roadmap M8 status. Verify the docs checks.
- [ ] 14.3 M8 exit check (design D21): golden suite, coverage of the six capabilities, three-platform identity, both-mode tests, the security sign-off, `check_cleanroom.py`, `openspec validate --all --strict`. Verify each item in the M8 exit table with run ids.
