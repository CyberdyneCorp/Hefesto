# Proposal: M8 Schematic capture

> **Status: plan drafted 2026-11-13, after the user approved the M8 decisions the same day.** The 24 open SCHEM entries, D-21 (`xschem-compat`) and judgement items J-1 to J-5 of `docs/decisions/m8-decision-table.md` were approved as drafted (commit "Resolve the 24 open questions blocking M8"), and the specs were aligned with them (commit "Align the M8 specs with the approved decisions"). `scripts/check_milestone_readiness.py --milestone M8` reports ready. Design Open Questions 1–3 were decided the same day, each as its recommended option (a).

## Why

Hefesto has no schematic editor. Decision P-12 requires one that reads and writes xschem `.sch`/`.sym` files so that the sky130, gf180mcu and IHP sg13g2 symbol libraries work unchanged, and that writes SPICE netlists identical to xschem's. Schematics are the start of the Desktop Beta flow (schematic → simulation → layout → DRC → GDS → extraction → LVS → post-layout simulation, M9).

What exists today (main at 30561192):
- **Specs.** Six `schematic-*` capabilities with 101 requirements, 56 of them `exact` against xschem 3.4.8RC (`4142e6d6`). Every SCHEM open question is resolved.
- **Core.** No schematic code. The sandboxed Tcl interpreter (`script/tcl/sandbox.hpp`, X-3), the PDK consent and trust store (`script/pdk/`), the project settings store (`settings/project_settings.hpp`, `compat/compat_settings.hpp` with `magic_compat` and `netgen_compat`) and the undo log (`undo/`) exist and are reused. `tests/unit/core/settings_test.cpp` asserts that `xschem-compat` is an unknown setting; M8 changes that.
- **Desktop.** "Schematic" is a disabled editor-view tab ("Schematic editor: arrives in M8", `view_models.cpp:409`). The "Waveforms" tab says "arrive in M8", although waveforms belong to M9 (`simulation-ui-and-crossprobe`). The reference board is `docs/design/mockups/Schematic.dc.html`.
- **Golden.** `tests/golden/SCHEM/CASES.md` lists 41 cases, none authored or captured. The capture image already builds xschem at the pinned commit (`tools/golden/container/Containerfile`, `WITH_XSCHEM=1`), and `goldenlib/refrunners.py` knows the `xschem` tool. The image has no gawk: Debian's default `awk` is mawk, which gives different netlists (Q-SCHEM-18).
- **PDK data.** `scripts/fetch_pdk.py` installs only `libs.tech/magic` and the `.mag`/GDS libraries. No xschem library of any PDK is fetched.

## What Changes

- **Clean-room setup first (CLEANROOM).** The two-team process of `hefesto-licensing` (J-1) becomes operational before any implementation: the spec team and the implementation team are named; implementation sessions run from worktrees and instructions that contain no xschem source and forbid following `Evidence:` and `(xschem ref: ...)` paths; the six `schematic-*` specs and `SCHEM.md` are reviewed for code paraphrase and rewritten as behaviour where needed; the pull-request template and a repository check enforce Hefesto-authored golden inputs (design D2).
- **PDK xschem data and inventories (PDK).** `fetch_pdk.py` gains the xschem libraries of the three PDKs at the pinned commits. One inventory script (`scripts/schem_inventory.py`) lists, over those Apache-2.0 trees: the referenced `devices/` symbols (Q-SCHEM-10), the Tcl procedures and variables (Q-SCHEM-8), the `xschem` subcommands (Q-SCHEM-10) and the subcircuit symbols with multi-line templates (Q-SCHEM-21). Its output is committed and checked by a test that fails when a PDK bump changes it (design D18).
- **Golden capture (GOLDEN, before implementation).** gawk installed as `awk` in the capture image; the 41 SCHEM cases authored with Hefesto-authored and PDK inputs only; `expected/` captured from xschem in CI. The spec team then fixes the exact texts that depend on captures (Q-SCHEM-14, -21, -22, -27, -28) and checks the electrical-equivalence comparator on Q-SCHEM-15 (design D16).
- **Interface table (SPEC).** The spec team extracts the interface of every inventoried `devices/` symbol (file name, pin names, order and positions, `type`, `format`, `template`, `global`) into `schematic-symbols-and-libraries` "System library", and the Tcl procedure semantics into "tcleval values", from the public manual and computed PDK values. Counsel reviews the table before Desktop 1.0 (J-3), which does not block M8.
- **File format (FORMAT).** `.sch`/`.sym` reader and writer: record grammar, brace strings, numeric fields, version records (Q-SCHEM-1, Q-SCHEM-3 compat), global records, embedded symbols (Q-SCHEM-2), property strings, orientation, canonical write order, round-trip identity, malformed input and safe saving.
- **Symbols and libraries (SYMLIB).** Symbol model, types, pins and pin order (Q-SCHEM-5), templates, instance names, text tokens, references and the search path, `xschemrc` as data in the sandbox (Q-SCHEM-23), `tcleval` with the inventoried procedures (Q-SCHEM-7, -8), the permission model (Q-SCHEM-25), `http` references as missing symbols (Q-SCHEM-9), schematics used as symbols, missing symbols, Tcl-only generators (Q-SCHEM-11), the symbol cache and the PDK attribute conventions.
- **System library (DEVLIB).** The clean-room `devices/` library: only the inventoried symbols, built from the spec's interface table, graphics drawn independently, with a test that checks every symbol against the table (design D8).
- **Netlisting (NETLIST).** In-process SPICE netlisting: connectivity, net naming, buses, globals, format substitution, hierarchical parameters, `expr`, post-processing and line breaking written natively from the spec, file structure, subcircuit definitions, device models, LVS mode, flat mode (behavioural, Q-SCHEM-15) and diagnostics, with every compat branch point (Q-SCHEM-12, -14, -19, -21, -22) in one place each.
- **Compatibility setting (COMPAT).** `xschem-compat` (default ON) and "portable netlist paths" (default off) in the project settings, the `** xschem-compat=off` netlist line, and the settings test updated (design D5).
- **Editing (EDIT) and commands (CMD).** The schematic document model, snap, selection, wires, placement, move/copy/stretch, rotate/flip, labels and ports, buses, properties, graphics, order, clipboard, hierarchy navigation and path, net highlighting and undo; the `schematic` command with every option, headless use and command logging; the `xschem` Tcl command with the inventoried subcommands.
- **Simulation integration, M8 part (SIMINT).** The `schematic` circuit source, testbench code blocks, control-block translation, simulator vector names and launchers, producing a validated deck that M9's embedded ngspice runs. Operating-point annotation with results, embedded graphs, waveform and layout cross-probing, the schematic-driven LVS command and results interchange need M9 and are deferred to it (design D13).
- **Security review (SECURITY).** The schematic script permission model (`hefesto-security` "Schematic script permission model") is reviewed and signed off before M8 exits (Q-SCHEM-25).
- **Desktop (DESIGN, SHELL).** The schematic editor screen per `Schematic.dc.html`: canvas, symbol library browser, properties, hierarchy, netlist action, net highlighting, desktop bindings (xschem-style, rebindable). The floating tool palette's view model is built and tested headless; its iPad presentation and the finger-vs-Pencil usability test are M14 items (Q-SCHEM-24).
- **Exit (PERF, INTEGRATION).** A netlisting and load benchmark, fuzz targets for the readers and evaluators, the requirement coverage check, docs and the M8 exit check.

### Non-goals

- Verilog, VHDL, Spectre and tEDAx netlisters (Q-SCHEM-4). Their records and attributes are kept through round trips; Verilog is revisited with P-17.
- Running simulations, waveforms, operating-point results, LVS and layout cross-probing (M9). M8 delivers decks and the data structures they need.
- Fetching `http` symbol references (Q-SCHEM-9) and external-program symbol generators (Q-SCHEM-11).
- The iPadOS schematic editor and its gestures (M14). M8 keeps the editing core and view models platform-free.
- Reproducing xschem's flat netlist text (Q-SCHEM-15).
- A GPL symbol pack or any xschem file in the repository or packages.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None in this change. M8 implements the existing requirements of the six capabilities listed in `docs/roadmap.md` ("M8"). The approved decisions were applied to the living specs before this change was created, so it sets `skip_specs: true`, following the M0–M5 and M10 precedent (m0-foundations design D10). Two kinds of spec edits remain inside M8, both by the spec team: the interface table and Tcl procedure semantics (SPEC), and the exact texts fixed by the captures (GOLDEN). They are made in the living specs on this change's branch.

M8 also implements the `xschem-compat` parts of `hefesto-magic-compat-mode`, the "Two-team clean-room process" of `hefesto-licensing` (process and checks, not code) and "Schematic script permission model" of `hefesto-security`.

## Impact

- **New code (core):** `src/core/{include/hefesto,src}/schem/{file,sym,lib,tcl,net,edit,cmd,sim}/`, `tests/unit/schem/`, `tests/fuzz/{fuzz_sch_reader,fuzz_schem_props,fuzz_schem_expr}.cpp`.
- **New code (desktop):** `src/shell/desktop/src/schematic_*.cpp`.
- **Data:** the system symbol library under `assets/schematic/devices/` (Apache-2.0, Hefesto-drawn); the xschem PDK data sets (fetched, never committed).
- **Shared changes:** `compat::CompatSettings` gains `xschem_compat`; `settings::ProjectSettings` gains `xschem-compat` and `portable-netlist-paths`; the sandbox gains the inventoried xschem Tcl procedures and the `xschem` command; the desktop enables the Schematic tab and relabels Waveforms as M9.
- **Tooling:** `scripts/schem_inventory.py`, `scripts/check_cleanroom.py`, gawk in the capture image, a SPICE canonical comparator in `tools/golden/compare/`, `fetch_pdk.py` xschem data sets.
- **Golden:** the 41 SCHEM cases, three of them with new Hefesto-authored inputs (SCHEM-RT-05, NL-01, NL-19).
- **Docs:** `docs/diagnostics/codes/schem.md`, `docs/design/m8-ui-conformance.md`, `docs/contributing/` (the two-team process, areas, fuzz targets), `docs/building.md` (xschem data sets), roadmap status and CHANGELOG.

## Dependencies

| Needs | From |
|---|---|
| Decisions approved, specs aligned | done 2026-11-13 |
| Clean-room roles and paraphrase review | CLEANROOM (section 1), before any implementation task |
| SCHEM golden cases captured (roadmap principle 2) | GOLDEN (section 3), before implementation |
| Interface table and Tcl semantics in the specs | SPEC (section 4), before DEVLIB and the Tcl part of SYMLIB |
| Sandboxed Tcl, consent, settings store, undo log | M4 (exist) |
| Simulation engine, deck preparation, LVS | M9: the deferred parts of `schematic-simulation-integration` |
| Counsel review of the process and the interface table | before Desktop 1.0 (M13), not blocking M8 |
| Finger-vs-Pencil usability test | before the M14 iPad scope freeze, not blocking M8 |
