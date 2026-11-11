# Tasks

Each task ends with how it is verified. Section 1 (PDK) lands first; workstreams 4–12 run in parallel after the scaffold and edit only what design.md D2 assigns to them; shared-file needs are reported as open items. SHELL builds a screen only after DESIGN's board for it exists and the user approved it (design D13). Golden case inputs are authored by GOLDEN from the owning workstream's requirements (design D16); `expected/` is captured in CI. Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`; requirements with a magic-compat dependence get one test per mode. The audit decisions D-29, D-31, D-33, D-34 and D-36 are approved (design "Resolved Questions").

## 1. PDK — pin move (Q-SCRIPT-9, first)

- [ ] 1.1 Verify the release data (design D15): that ciel `sky130-5740c4a8…` and `gf180mcu-5740c4a8…` contain the Magic files of open_pdks `c6df28c1` byte for byte (compare `libs.tech/magic` with an open_pdks `c6df28c1` build of the templates or the files fetched read-only for Q-SCRIPT-9), that `ihp-sg13g2-5e6d592e…` contains `libs.tech/magic` of IHP-Open-PDK `5e6d592e`, and the download and kept sizes; verify the facts are recorded in design D15, or stop and raise an Open Question if any Magic file differs
- [ ] 1.2 Move `scripts/fetch_pdk.py` and `tests/unit/common/reference_data.hpp` to the new pins (`sky130`, `gf180mcu`, `ihp` sets, same subsets, new tree digests and cache keys) with updated tests in `scripts/tests/`; verify a fresh install of each set matches its pinned digest and a second run is a verified no-op
- [ ] 1.3 Update the reference-data steps and cache keys of `ci.yml`, `golden-capture.yml` and `perf.yml`; verify a cold-cache run fetches each set once and a warm run fetches nothing
- [ ] 1.4 Re-run every M1–M3 unit test that reads PDK data on the new pins and fix or explain each changed expectation; verify `ctest` passes on every CI leg and each changed expectation is listed in design D15 "Re-run results" with its cause (PDK content) or an approved decision
- [ ] 1.5 With GOLDEN, re-capture and replay every golden case with `source = "pdk"` inputs (DRC-PDK-1..3, DRC-STD-SKY130-HD, DRC-STD-GF180-7T5V0 and the M1/M2 PDK cases) on the new pins; verify they pass without a new deviation, or each new difference has an approved decision
- [ ] 1.6 Update `docs/building.md` "Reference data for tests" (pins, ciel release names and why `5740c4a8` stands for `c6df28c1`, sizes, cache keys); verify the documented commands run as written

## 2. Scaffold

- [x] 2.1 Create the change `m4-scripting` (proposal, design, tasks, `skip_specs: true`) and verify `openspec validate m4-scripting --strict` and `openspec validate --all --strict` pass
- [ ] 2.2 Draft the App Review inquiry (design D20) in `docs/decisions/app-review-inquiry.md` for the user to submit, and record the submission date and answer there when the user provides them; verify the user has the text
- [ ] 2.3 Add `cmake/Tcl.cmake` (Tcl 8.6.16 by URL and SHA-256, static, no Tk, embedded library scripts; design D3) and the `hefesto::tcl` target, with `THIRD_PARTY_NOTICES.md` updated; verify every preset (`*-debug`, `*-release`, `*-asan`, `*-tsan`, `*-bench`, `linux-fuzz`, `ci-*`, `ipados`) configures and builds, and a `[m4-scaffold]` test evaluates `expr {1+1}` in an interpreter
- [ ] 2.4 Add the areas `script` (subdirectories `tcl`, `binding`, `pdk`, each with its `sources.cmake` and `codes.inc`), `pcell` (`framework`, `native`), `tools` and `gui`, their test directories and `docs/diagnostics/codes/{script,pcell,tools,gui}.md` with one section per owning workstream; verify the build passes and the test "each area's code table matches its document and prefix" covers the four areas
- [ ] 2.5 Write the documented public headers of design "Area contracts" with stubs, add `CORE-NOTIMPL-004`, and register `tag`, `gencell` with the `magic::` framework procedures of `pcell-toolkit`, `readspice`, `cif`, `drc_save_report`, `drc_load_report`, `mazeroute` and the GUI manager entry commands with stub handlers; verify the headers compile in `macos-debug` and `macos-bench`, and a `[m4-scaffold]` test lists every new command as registered
- [ ] 2.6 Add the shared interfaces of design D4 (`script::Interpreter`, `ProjectInterpreter` in `EditorState` created and deleted with the project, `ScriptEvents`, `ConsentHost` with a test host, `WidgetInterceptor`/`MenuModel` stubs, `pcell::DialogModel`, `NativeRegistry`, the `gui::` view-model headers, `platform::FilePicker`); verify the CLI, shell and core tests pass unchanged, and `[m4-scaffold]` tests show two open projects get two interpreters
- [ ] 2.7 Record the ownership table and contracts in design.md (D1–D20) and seed `docs/design/m4-ui-conformance.md`; verify `openspec validate --all --strict` passes

## 3. DESIGN — M4 boards

- [ ] 3.1 Draw `M4-Menu-Bar.dc.html` (menu bar with the wrapper menus of `ui-gui-managers`, PDK `Devices` menus, the intercepted `DRC Routing` and `Import SPICE` items, the skipped-statement report), `M4-PCell-Dialog.dc.html` (Place device from the tool rail and menus; dialog new, edit, invalid-value and dependency states; instance name; Create/Apply/Close), `M4-Script-Consent.dc.html` (consent prompt with script origin and PDK version, trusted PDKs in the PDK panel, declined read-only state, sandbox-denial diagnostic, long-running script with interrupt), `M4-Managers.dc.html` (cell, library and technology managers, text helper, layer toolbar and reorder dialog) and `M4-Console.dc.html` (Tcl console); render dark and light PNGs to `docs/images/ui/m4-*.png`, update `docs/design/mockups/{README.md,canvas.json}`; verify every M4 screen state SHELL builds (section 10) maps to a board and the user approves the boards
- [ ] 3.2 Reflect the new screen patterns in `hefesto-design-system` "Screen patterns follow the reference mockups" (the exact bullet text as an open item for INTEGRATION) and verify `openspec validate --all --strict` passes

## 4. TCL — embedded sandboxed Tcl (hefesto-scripting, hefesto-security)

- [ ] 4.1 Implement `create_project_interpreter` (full interpreter, embedded library scripts, no host Tcl files read), `Interpreter::evaluate/source/set_var/get_var/define_command` and the `cmd::Value` ↔ `Tcl_Obj` conversion (lists stay lists); verify unit tests and `[cap:hefesto-scripting][req:Host-independent scripting binding]` tests
- [ ] 4.2 Remove `exec`, `socket`, `load`, `unload`, `exit`, unsafe `interp create`, binary `package require` and `open |…` (design D5), each raising a Tcl error naming the operation and a `SCRIPT-SANDBOX-*` diagnostic with script file and line; verify `[req:Embedded sandboxed Tcl on all platforms]` (scenario "Denied exec") and `[cap:hefesto-security][req:Script sandbox enforcement]` (scenario "Script tries a socket") tests, one per removed command
- [ ] 4.3 Implement the `SandboxFilesystem` (`Tcl_FSRegister`) allowing only granted roots, with denial tests per path class (relative escape, symlink out of a root, `~user`, `//` forms, `glob` patterns, `source`, `open`, `file` subcommands, `cd`), `cd` allowed within granted roots (D-33) and the synthesized `env` array (D-31); verify `[req:Embedded sandboxed Tcl on all platforms]` scenarios "cd outside the granted roots" and "Synthesized environment", the denial tests, and that `catch` absorbs a denial
- [ ] 4.4 Implement one interpreter per open project and its teardown, and the evaluation limit with interrupt; verify `[req:One interpreter per open project]` (scenario "Two projects on different PDKs", with stub helpers) and an interrupt test
- [ ] 4.5 Add the `fuzz_tcl_sandbox` libFuzzer target (with INTEGRATION) with a filesystem audit of every path the VFS sees; verify the seed replay passes and a 60 s run finds no access outside the temporary root
- [ ] 4.6 Keep every function's cognitive complexity at most 15 (flag irreducible ones) and verify with the cognitive-complexity tooling on `script/tcl/`

## 5. BINDING — scripting-api

- [ ] 5.1 Register every command in `magic::` with the global import, collision renames, unknown handler and quiet dispatch; status and strict mode; window-directed commands; verify `[cap:scripting-api]` tests for "Command registration in the magic namespace", "Global import of magic commands", "Name collisions with Tcl built-ins", "Unknown-command fallback for renamed built-ins", "Command status and error signalling", "Window-directed commands", "Post-command bookkeeping"
- [ ] 5.2 Implement `tag` and the single-pass, list-quoted substitution (Q-SCRIPT-3); verify `[req:Command tag callbacks]`, `[req:Tag callback execution and substitution]` tests including "Result with spaces stays one word" and "Literal percent in a tag script"
- [ ] 5.3 Implement output routing through `puts` with literal `$` (Q-SCRIPT-4), prompts and dialogs, the terminal input queue, directory-change rebasing within granted roots (D-33), and the generic helpers (`echo`, `irsim`/`xcircuit`/`netgen` per D-34, `drcstate`, `auto_noexec`); verify `[req:Output, flushing and escaping]`, `[req:Prompts and dialogs through the script layer]`, `[req:Terminal input queue without console]`, `[req:Directory change rebases cell paths]`, `[req:Generic Tcl helpers defined at startup]` tests
- [ ] 5.4 Implement the engine-read variables with traces into the D-14 setting and the path-variable cache per magic-compat with the warn-once set (Q-SCRIPT-5); verify `[req:Script variables read by the engine]` tests in both modes
- [ ] 5.5 Implement the result formats and their mode switches: `port last` OFF (Q-SCRIPT-6), the `cif` command's `scale` and style queries (Q-SCRIPT-7 both modes), `drc listall why` in Magic hash order with the comparator sort removed (Q-SCRIPT-8, with GOLDEN), number formatting; verify `[req:Numeric value formatting in results]`, `[req:Result formats of commands used by scripts]` tests (both modes) and `[cap:drc-commands][req:drc why]` tests updated for the order
- [ ] 5.6 Implement the keyword terminator rule per magic-compat (Q-SCRIPT-14) in every lookup; verify `[req:Option keyword matching]` tests including the two tab-terminated alias scenarios
- [ ] 5.7 Implement script-defined procedures in `magic::` and the cross-PDK redefinition warning hook (with PDKINT's origin tracker); verify `[req:Script-defined procedures in the engine namespace]` tests including "Cross-PDK helper redefinition"
- [ ] 5.8 Deliver `magic::drcstate busy|idle` from `drc::DrcEvents` in every interpreter; verify a test with the M3 worker
- [ ] 5.9 Keep every function's cognitive complexity at most 15 (flag irreducible ones) and verify with the cognitive-complexity tooling on `script/binding/`

## 6. PDKINT — scripting-pdk-integration and consent

- [ ] 6.1 Implement the script-host startup sequence and the command-line table with Magic's quirks (Q-SCRIPT-37), startup files as Tcl with Magic's error text, `techmanager initall` as a no-op (Q-SCRIPT-33), and remove the literal-`set` startup reader; verify `[cap:scripting-pdk-integration]` tests for "Script-host startup sequence", "Command-line options in the script host", "Command-line file arguments", "Startup files are scripts" and `[cap:app-startup]` tests unchanged
- [ ] 6.2 Implement `ConsentHost`, the per-PDK-version trust store (`Provenance::trusted`), the CLI policy (`--trust-pdk`, batch declines), and the declined path (read-only, `minimum` tech); verify `[cap:hefesto-security][req:Consent before implicit script execution]` tests and a CLI test
- [ ] 6.3 Run the sky130A, gf180mcuD and ihp-sg13g2 rc files and device scripts unchanged at the new pins (the PDK startup and device-script contracts, generator drawing conventions, the generator command surface including `ext2spice scale`, `load -silent <cell>`), and the older rc generation; verify `[req:PDK startup file contract]` (including "Older rc generation"), `[req:PDK device-script contract]`, `[req:Generator drawing conventions]`, `[req:Command surface used by device generators]` tests on the installed PDKs
- [ ] 6.4 Implement the helper-origin tracker and cross-PDK warning (Q-SCRIPT-41) and leave the PDK helper names undefined; verify `[req:PDK helper procedures in the engine namespace]` tests including "Micron conversion" and "SPICE suffix parsing" under each PDK's definitions
- [ ] 6.5 Implement `WidgetInterceptor` and `MenuModel` (Q-SCRIPT-42/34, D-32) with skipped-statement diagnostics; verify `[req:Wrapper widget operations in PDK menu callbacks]` tests (the three scenarios) with the test file-picker host
- [ ] 6.6 Run the `<tech>.magicrc` step of technology auto-discovery behind consent, and the PDK-relative paths with script variables; verify `[req:Technology auto-discovery from layout files]`, `[req:PDK-relative paths in saved files]` tests
- [ ] 6.7 Implement `CuratedPackPolicy` (Q-SCRIPT-46, D-30) as a core policy; verify `[req:PDKs without bundled scripts in curated-pack mode]` and `[cap:hefesto-scripting][req:App Store fallback for iPadOS]` tests with a curated-pack test host
- [ ] 6.8 Build the command-trace hook and compare a trace of each PDK's rc file and every generator with defaults against the inventory table; verify `[req:PDK command and variable inventory]` (scenario "Inventory coverage check") and that every missing command found is implemented or raised as an open item
- [ ] 6.9 Keep every function's cognitive complexity at most 15 (flag irreducible ones) and verify with the cognitive-complexity tooling on `script/pdk/`

## 7. PCELL — gencell framework and native generators

- [ ] 7.1 Implement the library/device procedure contract, the parameter dictionary model, normalization (Q-SCRIPT-21) and naming (Q-SCRIPT-22) in C++; verify `[cap:pcell-toolkit]` tests for "Library and device procedure contract", "Parameter dictionary model", "Parameter value normalization for naming", "Deterministic generated-cell naming" (including "Infinite value" and a non-ASCII table)
- [ ] 7.2 Implement generated-cell properties, `gencell` dispatch, create (Q-SCRIPT-24), change (Q-SCRIPT-23, 29), non-placing generation and legacy random names, and the `instance list`/`what -list` results they use; verify the `[req:…]` tests of those requirements including "Scripted change of a fixed device" and "Headless change keeps the instance name"
- [ ] 7.3 Implement the dialog builder API into `DialogModel`, dependencies and validation (Q-SCRIPT-25), selection-driven refresh and the ^P binding, toolkit menu hooks; verify `[req:Parameter dialog structure]`, `[req:Dialog builder API]`, `[req:Dialog dependencies and validation]`, `[req:Selection-driven dialog refresh and key binding]`, `[req:Toolkit menu hooks]` tests and golden UI-G-049 (with GOLDEN)
- [ ] 7.4 Implement SPICE netlist seeding (`netlist_to_layout`, subcircuit seeding); verify `[req:SPICE netlist to layout seeding]`, `[req:Seeding a subcircuit cell]` tests
- [ ] 7.5 Implement the bit-identical PDK helpers per PDK (Q-SCRIPT-40, sg13g2 variants) and the native MOSFET generators for sky130, gf180mcu and sg13g2 (design D10), registered per exact PDK version; verify per-device tests against Tcl output in the same build
- [ ] 7.6 Implement the native fixed-device generators for sky130 and gf180mcu, and for sg13g2 after SCRIPT-PDK-14 confirms Q-SCRIPT-45 (edit-cell and view reset, `label … +layer` failure per Q-SCRIPT-39); verify the same per-device tests
- [ ] 7.7 Add the `pcell-equivalence` CI job (with INTEGRATION) running the parameter sweeps of `tests/data/pcell/sweeps/` natively and through Tcl and comparing names and saved `.mag` files byte for byte except the `timestamp` line (D-29); verify `[cap:hefesto-scripting][req:Native PCell generators validated against Tcl]` and `[cap:pcell-toolkit][req:Native generator equivalence]` tests and a green job
- [ ] 7.8 Keep every function's cognitive complexity at most 15 (generators may be flagged, at most 25) and verify with the cognitive-complexity tooling on `pcell/`

## 8. TOOLS — readspice and auxiliary tools

- [ ] 8.1 Implement `readspice` (file candidates, preprocessing with both continuation modes, subcircuit recognition, port renumbering with the fixed message, PININFO) (Q-SCRIPT-19, 32); verify `[cap:readspice]` tests for all five requirements, both modes for "Line preprocessing", and golden SCRIPT-RSP-01..06 (with GOLDEN)
- [ ] 8.2 Add the `fuzz_readspice` target (with INTEGRATION); verify the seed replay and a 60 s run without a crash
- [ ] 8.3 Implement the auxiliary tools (socket server absent, BSI helpers and the rev-0 toolkit not loaded, reference-library stripper, console integration, launcher options, tech builder) and the `mazeroute` stub naming M10 (Q-SCRIPT-30); verify `[cap:scripting-auxiliary-tools]` tests for the M4 requirements and that the `mazeroute` requirement is listed for M10 in the coverage report
- [ ] 8.4 Keep every function's cognitive complexity at most 15 and verify with the cognitive-complexity tooling on `tools/`

## 9. GUIMGR — ui-gui-managers view models

- [ ] 9.1 Implement the wrapper model: construction, hook points (Q-SCRIPT-13), menus (Q-SCRIPT-36), tag callbacks and captions, option variables, close and key overrides, per-window command entry; verify `[cap:ui-gui-managers]` tests for those requirements including "Per-window callback"
- [ ] 9.2 Implement the technology manager, the layer toolbar and presets file read as data (Q-SCRIPT-11, 28) and the `fuzz_toolbar_presets` target (with INTEGRATION); verify `[req:Technology manager]`, `[req:Layer toolbar]` (scenario "Toolbar file is not executed"), `[req:Toolbar order presets file]` tests and the fuzz seed replay
- [ ] 9.3 Implement the cell and library managers (Q-SCRIPT-27) and their tags; verify `[req:Cell manager]`, `[req:Library manager]` tests
- [ ] 9.4 Implement the DRC manager model, `drc_save_report` and `drc_load_report` (Q-SCRIPT-26); verify `[req:DRC manager and DRC report file]` tests including "Report with micron units"
- [ ] 9.5 Implement the text helper (Q-SCRIPT-31), file prompts, the crash-backup timer and tool modes; verify `[req:Text helper]`, `[req:File prompts]`, `[req:Crash backup timer]`, `[req:Tool modes]` tests
- [ ] 9.6 Keep every function's cognitive complexity at most 15 and verify with the cognitive-complexity tooling on `gui/`

## 10. SHELL — desktop M4 screens

- [ ] 10.1 Build the menu bar from `WrapperMenuModel` and `MenuModel` (PDK menus, intercepted items, skipped-statement report) per `M4-Menu-Bar`; verify view-model tests and that each item issues its logged command
- [ ] 10.2 Build Place device and the PCell dialog rendering `DialogModel` per `M4-PCell-Dialog` (enable the tool rail item); verify `[cap:hefesto-app-ux][req:Functional parity with Magic's GUI managers]` (scenario "PCell dialog") tests and that Create issues `magic::gencell … k v …`
- [ ] 10.3 Build the consent dialog (`ConsentHost`), the trusted-PDK list in the PDK panel, the declined and sandbox-denial states per `M4-Script-Consent`; verify view-model tests for each state
- [ ] 10.4 Build the cell, library and technology managers, the text helper, the layer toolbar and reorder dialog per `M4-Managers`, with virtual Raven Tables; verify the GUI-manager parity tests and keyboard and screen-reader support
- [ ] 10.5 Turn the console panel into the Tcl console with interrupt per `M4-Console`, and host `platform::FilePicker`; verify view-model tests
- [ ] 10.6 Complete `docs/design/m4-ui-conformance.md` (every M4 board element → implemented as → gap/why) and report Raven gaps and spec/board differences as open items

## 11. CARRY — items carried from M2

- [ ] 11.1 Run the Raven confirmation checks (frame rate on the reference M1 Mac and on Linux with a discrete GPU, live mouse/keyboard/trackpad input on Linux X11 and Wayland, Orca) and file the Raven issues upstream (function-key text, key repeat, custom-widget accessibility press and state, anchored popover); verify the results and issue links are recorded in `docs/decisions/raven-go-no-go.md`
- [ ] 11.2 Implement floating panels and drag-to-dock (headers, drop-zone highlight, collapsed edge strips) and the per-user saved panel layout; verify view-model tests per `M2-Panels` and a restore-at-launch test
- [ ] 11.3 Implement the modal key-binding sheet (capture, conflict warning with Replace, Reset table, Load file) per `M2-Key-Bindings`; verify view-model tests
- [ ] 11.4 Implement the label text entry as a canvas popover (or record the Raven gap), `unmeasure` on Escape after a measure, and the Paint tool disabled until a paint layer is chosen; verify view-model tests
- [ ] 11.5 Name undo steps after their user command and show the undo history on press-and-hold; verify `[cap:layout-undo]` and `[cap:hefesto-app-ux][req:Undo history]` tests
- [ ] 11.6 Implement the per-area minimap update and the pixel-width box and selection outline in `render::highlight_batches`; verify render tests and the frame-time benchmark not regressing
- [ ] 11.7 Implement the wiring tracking HUD, the "Wiring · tracking" chip and the Wire inspector per `M2-Wiring`, and capture the crash-recovery dialog; verify view-model tests and the capture
- [ ] 11.8 Implement VoiceOver press on custom-drawn controls if Raven exposes an accessible press action, otherwise record the gap; close the M2 items in `docs/design/m2-ui-conformance.md`; verify the conformance document lists every carried item as done or as a recorded Raven gap

## 12. GOLDEN — SCRIPT and related golden cases

- [ ] 12.1 Build Magic fc5daa9e with Tcl 8.6 and Tk in the capture image and run GUI cases under Xvfb; verify a capture of SCRIPT-API-01 and SCRIPT-GUI-01 succeeds in CI
- [ ] 12.2 Add `tools/golden/authoring/{script_cases,m4_cases}.py` and author the 89 SCRIPT cases plus SCRIPT-PDK-18 (older rc smoke; new `CASES.md` row; design D15) with `--trust-pdk` and `random seed 1` for PDK cases; verify `capture.py --check` accepts every case directory and `m4_cases.py --list` lists 90 SCRIPT cases
- [ ] 12.3 Author MODEL-G-055, CELL-PR-03, CELL-PW-05, CELL-PR-12 (per D-33), remove `deferred = "M4"` from UI-G-009, UI-G-013, UI-G-049, and switch DRC-PDK-3 to the IHP `cap_cmomf` generator; verify `capture.py --check` and that `m4_cases.py --not-authored` lists nothing for M4
- [ ] 12.4 Remove the `drc listall why` sort from `drc_console.py` (Q-SCRIPT-8) with comparator tests, and add the GUI-case replay through view models and `hefesto-cli` (commands and files compared); verify `tools/golden/tests/` pass
- [ ] 12.5 Capture the new and changed cases, record the deviations of design D16 (Q-SCRIPT-3, 4, 32) only with their approved decisions; verify `ctest -L golden` passes on every CI leg
- [ ] 12.6 Add the Desktop Alpha cases `DESKTOP-ALPHA-SKY130`, `-GF180MCU`, `-SG13G2` (design D18) and their `tests/e2e/` scripts; verify they pass on Linux x86_64, Linux aarch64 and macOS arm64

## 13. INTEGRATION

- [ ] 13.1 Apply the shared-file open items of workstreams 1–12 (CMake, `EditorState` members, CLI options such as `--trust-pdk`, presets, the `pcell-equivalence` job, notices) and verify every macOS preset configures, builds and tests with `-DHEFESTO_WARNINGS_AS_ERRORS=ON`
- [ ] 13.2 Move every caller of `cmd::run_script` to the interpreter, remove the placeholder reader and `fuzz_command_script`, and wire `fuzz_tcl_sandbox`, `fuzz_readspice` and `fuzz_toolbar_presets` into the fuzz job; verify the seed replays pass on every leg and the 60 s fuzz job runs each target without a crash
- [ ] 13.3 Run the security review of design D19 and write `docs/contributing/security.md` (sandbox, consent, toolbar data, interception); verify every denial test passes and the review findings are fixed or recorded
- [ ] 13.4 Implement the D-36 `QuitPolicy` (desktop exit unchanged; iPad policy aborting to the project browser with a diagnostic) and verify `[cap:app-startup][req:quit command and exit status]` tests including "PDK guard quit on iPadOS" with a test host; verify that every spec edit made for D-29, D-31, D-33, D-34 and D-36 has its tagged test in the owning workstream (4.3, 5.3, 7.7) and `openspec validate --all --strict` passes
- [ ] 13.5 Verify no `CORE-NOTIMPL-004` use or stub remains except `mazeroute` (M10, its own code), retire the code in the table, and verify the diagnostics tests pass
- [ ] 13.6 Verify the M4 exit criteria end to end: unit, golden (including the Desktop Alpha cases) and fuzz tests green in CI on every leg, the cross-platform determinism job covering the SCRIPT golden outputs, `check_requirement_coverage.py --milestone M4` covering the 57 exact/behavioral requirements (listing `mazeroute` for M10), `check_milestone_readiness.py --milestone M5` reporting the next milestone's state, `openspec validate --all --strict`, and the manual Desktop Alpha checklist signed off by the user on Linux and macOS
- [ ] 13.7 Update README.md, docs/roadmap.md (M4 status, Desktop Alpha), docs/building.md (Tcl, PDK pins, trust options), docs/contributing/ (the `script`, `pcell`, `tools`, `gui` areas, codes, fuzz targets, golden modules, the equivalence job) and the diagnostic code documents; verify the documented commands run as written
