# Design: M4 Scripting and PDK integration

## Context

See proposal.md for motivation. M3 (openspec/changes/m3-drc) delivered the DRC rule compiler, kernel, engine with a background worker, the `drc` command set, mask hints and the desktop DRC browser. The conventions of M0–M3 still hold: SPDX headers, Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`, per-area diagnostic code tables `AREA-TOPIC-NNN` documented in `docs/diagnostics/codes/<area>.md`, cognitive complexity at most 15 per function (flag irreducible ones), no UI/GPU/Raven/process headers in the core, golden expected outputs captured only in CI (`.github/workflows/golden-capture.yml`), Magic 8.3.694 at fc5daa9e as the behavioral reference (`build/ref/magic`, gitignored), clean-room rule for netgen/xschem/KLayout, `native_only` diagnostics dropped by the `magic` diagnostics format.

What M3 left that M4 builds on (from the code, 2026-11-11):

- **Scripts.** No Tcl is vendored, linked or fetched (`THIRD_PARTY_NOTICES.md` lists it under "to be added"). `cmd::parse_script`/`run_script`/`quote_word` (`cmd/script.hpp`) split `;`, newlines, quotes, braces and `#`, with no `$`/`[ ]` substitution and no Tcl built-ins. `fuzz_command_script` fuzzes that reader.
- **Commands.** `cmd::CommandRegistry` and `cmd::Dispatcher::execute(Invocation) -> CommandResult` with `cmd::Value` (int, double, string, list); origins cli, script, gui, key, touch, plugin, internal. `strip_namespace()` already accepts `magic::`/`::magic::`. Magic-compat quiet ambiguity for `get`/`id` exists (`Dispatcher::magic_ambiguous`, `ui::magic_lookup_ambiguous`); every keyword lookup (`match_name`, `cmd::lookup`, `lookup_entry`) ends a keyword at a space only, which is Q-SCRIPT-14's ON behavior. Registered: `macro`, `source`, `suspendall`/`resumeall`, `pushbox`/`popbox`, `pushstack`/`popstack` (native ports of tools.tcl), `hefesto::strict`, `echo`, `send`, `setting`, and most generator commands (`port`, `cellname list`, `tech lock/unlock/revert`, `random`, `scalegrid`, `snap`, `units`, `polygon`, `wire segment`, `windownames`, `quit -noprompt`, `addpath`, `getcell … child`, `identify`, `goto`, `view`, `property`). Missing: `tag`, `gencell`, `readspice`, `cif`, `ext2spice`; `instance list` and `what -list` return empty structures; `load -silent foo` (option before the name) prints the usage.
- **Results.** `port last` already counts all labels (Q-SCRIPT-6 ON); `label X default` already gives font −1 (Q-SCRIPT-31). The CIF output scale exists only internally (`CoordinateUnits::cif_scale`, single-precision emulation in `metric_to_internal`). `drc listall why` lists texts in first-discovery order (`drc/commands/reporting.cpp` `WhyCollector`), and `tools/golden/compare/drc_console.py` sorts them. `util::MagicHashTable` (`util/magic_hash_table.hpp`) implements `hefesto-output-determinism` "Magic string hash table" and is used by macros, builtins, cells and properties.
- **Paths and settings.** `cell::PathVariables`/`expand_entry` expand `$NAME` from the environment on every use (no cache, no script variables). `compat::CompatSettings`, `settings::ProjectSettings{compat, script_strict}`, setting layers builtin/pdk/user/project/session.
- **PDK.** `pdk::PdkManager` installs, enables and verifies; consent covers installs only (`-accept-license`, `-accept-unverified`); `Provenance::trusted` exists and is always false. Technology auto-discovery is implemented (`app/startup_files.cpp`, `cell::IoSession::discover_technology`) but skips the `<tech>.magicrc` step. Startup files contribute only literal `set NAME value` lines (`app/settings_layers.cpp`). `fetch_pdk.py` pins open_pdks `0fe599b2` and IHP `ddb601a4` (also `tests/unit/common/reference_data.hpp`).
- **Desktop.** No menu bar: a top bar with Open and PDKs buttons, chips, view tabs, disabled later actions, one shared popover. Tool rail "Place device" disabled ("arrives in M4"). Panels: layers/hierarchy, inspector, console, keys, DRC, color, PDK manager. Dialogs: only a question dialog (`PromptDialog`, Raven `Rdialog_run`) and the native file picker (`Rfile_dialog_open`). No form or dialog builder, no cell/library/technology manager. No iPad shell; an `ipados` preset builds the core.
- **Golden.** `tests/golden/SCRIPT/CASES.md` has 89 rows (API 25, PDK 17, PCELL 16, GUI 13, BOX 8, RSP 6, AUX 4) and no authored case. The harness runs Magic's Tcl build (`magic -dnull -noconsole [-T tech] <script>`) and replays through `hefesto-cli --diagnostics=magic`. Waiting for M4: UI-G-009, UI-G-013, UI-G-049 (`deferred = "M4"`), MODEL-G-055, CELL-PR-03, CELL-PW-05, CELL-PR-12 (not authored), DRC-PDK-3 (box/paint stand-in for the IHP `cap_cmomf` generator).
- **CI.** `ci.yml` (lint, reference-data, build matrix of 8 Linux/macOS legs with the `ipados` build, gpu-readback, determinism, asan, tsan, fuzz), `golden-capture.yml`, `perf.yml`, `openspec-validate.yml` (readiness `--through M1`).
- **Diagnostics.** CORE-NOTIMPL-001..003 are retired; the next is CORE-NOTIMPL-004.

M4 runs as an early PDK pin step and the App Review inquiry, a scaffolding step (interfaces, `EditorState` members, the Tcl build, command stubs, the build contract), a design step for the new screens, eight parallel workstreams and an integration step.

## Goals / Non-Goals

**Goals:**
- The M4 exit criteria of docs/roadmap.md: the common exit criteria for the 8 M4 capabilities (unit tests tagged for every exact/behavioral requirement, 57 in M4 plus `mazeroute`'s in M10; fuzz targets for new readers; CI green on every leg; byte-identical output on Linux x86_64, Linux aarch64 and macOS arm64), the SCRIPT golden cases pass (D16), and the Desktop Alpha: on Linux and macOS, open a real sky130, gf180mcu and sg13g2 design with the PDK's startup script after consent, edit it, DRC-check it, and place PCells through the generator dialogs (D18).
- PDK scripts run unchanged; native generators give the same cell names and `.mag` content as the Tcl generators (D10, D-29).
- Nothing in an opened file or folder runs code without consent (X-3, `hefesto-security`, Q-SCRIPT-11).
- The items carried from M2 are closed (D17).

**Non-Goals:**
- The iPad UI (M14), `mazeroute` (M10), GDS/CIF and extraction behind generator utility scripts (M5/M7), native generators beyond D10's set.
- Rewriting M1–M3 code that meets its spec. M4 changes earlier code only where an M4 requirement or decision needs it: keyword lookup OFF rule, `port last` OFF, path-variable cache, `drc listall why` order, startup files, auto-discovery's rc step, the PDK pins.

## Decisions

### D1. Layout of the M4 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| Tcl embedding and sandbox | `src/core/include/hefesto/script/tcl/` | `src/core/src/script/tcl/` | `tests/unit/script/tcl/` | `hefesto::script` |
| command binding (`magic::`, tags, output, variables) | `src/core/include/hefesto/script/binding/` | `src/core/src/script/binding/` | `tests/unit/script/binding/` | `hefesto::script` |
| PDK startup, consent, menu interception | `src/core/include/hefesto/script/pdk/` | `src/core/src/script/pdk/` | `tests/unit/script/pdk/` | `hefesto::script` |
| shared script types (interpreter handle, events) | `src/core/include/hefesto/script/{interp,events,types}.hpp` | `src/core/src/script/` | `tests/unit/script/` | `hefesto::script` |
| gencell framework | `src/core/include/hefesto/pcell/framework/` | `src/core/src/pcell/framework/` | `tests/unit/pcell/framework/` | `hefesto::pcell` |
| native generators | `src/core/include/hefesto/pcell/native/` | `src/core/src/pcell/native/{sky130,gf180mcu,sg13g2}/` | `tests/unit/pcell/native/` | `hefesto::pcell` |
| readspice and auxiliary tools | `src/core/include/hefesto/tools/` | `src/core/src/tools/` | `tests/unit/tools/` | `hefesto::tools` |
| GUI manager view models | `src/core/include/hefesto/gui/` | `src/core/src/gui/` | `tests/unit/gui/` | `hefesto::gui` |
| desktop screens | `src/shell/desktop/include/hefesto/shell/` | `src/shell/desktop/src/` | `tests/unit/shell/` | `hefesto::shell` |

`src/core/CMakeLists.txt` adds `script pcell tools gui` to `HEFESTO_CORE_AREAS`. `script/` and `pcell/` use per-subdirectory `sources.cmake` and `codes.inc` fragments assembled by INTEGRATION-owned `script/{sources.cmake,codes.cpp}` and `pcell/{sources.cmake,codes.cpp}`, as `drc/` did in M3. Diagnostic prefixes: `SCRIPT-` (topics `SCRIPT-TCL-*`, `SCRIPT-SANDBOX-*` (TCL), `SCRIPT-BIND-*` (BINDING), `SCRIPT-PDK-*`, `SCRIPT-TRUST-*` (PDKINT); one document `docs/diagnostics/codes/script.md` with a section per workstream), `PCELL-` (`pcell.md`), `TOOLS-` (`tools.md`), `GUI-` (`gui.md`).

### D2. Workstreams and ownership

| Workstream (tasks.md section) | Capabilities | Owns (may edit) |
|---|---|---|
| PDK | — (reference data for every M4 capability; Q-SCRIPT-9) | `scripts/fetch_pdk.py`, `scripts/tests/test_fetch_pdk.py`, `tests/unit/common/reference_data.hpp`, the reference-data steps of `.github/workflows/{ci,golden-capture,perf}.yml`, `docs/building.md` "Reference data for tests", and, for the re-run only, the expected values of existing PDK-based unit tests (each change listed with its approved decision) |
| DESIGN | `hefesto-design-system` (screen patterns), `hefesto-app-ux` (M4 screens) | new `docs/design/mockups/M4-*.dc.html`, `docs/images/ui/m4-*`, `docs/design/mockups/{README.md,canvas.json}` |
| TCL | `hefesto-scripting`, `hefesto-security` "Script sandbox enforcement" | `src/core/{include/hefesto,src}/script/tcl/`, `tests/unit/script/tcl/`, `tests/data/script/tcl/`, `cmake/Tcl.cmake`, `script.md` (TCL sections) |
| BINDING | `scripting-api` | `src/core/{include/hefesto,src}/script/binding/`, `tests/unit/script/binding/`, `tests/data/script/binding/`, `script.md` (BINDING section); and for the decided changes to M1–M3 code: the keyword-terminator rule in `cmd/keywords.cpp`, `cmd/registry.cpp` `match_name` and `ui/magic_help.cpp` (Q-SCRIPT-14 OFF), `port last` in `db/commands_labels.cpp` (Q-SCRIPT-6 OFF), the path-variable cache in `cell/search_path.*` (Q-SCRIPT-5), the `drc listall why` order in `drc/commands/{reporting,report_options}.cpp` (Q-SCRIPT-8), the new `cif` command registration file `src/core/src/script/binding/cif_command.cpp` |
| PDKINT | `scripting-pdk-integration`, `hefesto-security` "Consent before implicit script execution" | `src/core/{include/hefesto,src}/script/pdk/`, `tests/unit/script/pdk/`, `tests/data/script/pdk/`, `script.md` (PDKINT sections); the startup-file evaluation in `app/startup.cpp` and `app/settings_layers.cpp`, the rc step of `cell/io_session.cpp` `discover_technology`, the trust record in `pdk/manager.cpp` (`Provenance::trusted`), the `ext2spice scale` setting command, `load -silent` argument order in `cell/` |
| PCELL | `pcell-toolkit`; `hefesto-scripting` "Native PCell generators validated against Tcl" | `src/core/{include/hefesto,src}/pcell/`, `tests/unit/pcell/`, `tests/data/pcell/`, `docs/diagnostics/codes/pcell.md`, the `instance list` and `what -list` result fixes in `db/`/`select/` command files |
| TOOLS | `readspice`, `scripting-auxiliary-tools` | `src/core/{include/hefesto,src}/tools/`, `tests/unit/tools/`, `tests/data/tools/`, `docs/diagnostics/codes/tools.md` |
| GUIMGR | `ui-gui-managers` (core side) | `src/core/{include/hefesto,src}/gui/`, `tests/unit/gui/`, `tests/data/gui/`, `docs/diagnostics/codes/gui.md` |
| SHELL | `hefesto-app-ux` (M4 screens), desktop parts of `ui-gui-managers`, `pcell-toolkit` dialogs, `hefesto-security` consent prompt | `src/shell/desktop/` except the CARRY files, `tests/unit/shell/` (M4 screen tests), `docs/design/m4-ui-conformance.md` |
| CARRY | the M2 items of roadmap "Carried from M2" (`hefesto-app-ux`, `hefesto-input-and-touch`, `layout-undo`, `hefesto-rendering`) | the shell files of panels/dock, key-binding sheet, label tool, measure tool, tool rail paint state, wiring HUD/inspector, minimap (`src/shell/desktop/src/{panels*,dock*,key_bindings*,label_tool*,wiring_hud*,minimap*}` and their view-model parts), `render::highlight_batches`, the per-step command label in `src/core/src/undo/`, their tests, `docs/design/m2-ui-conformance.md` (closing notes), the Raven issue reports |
| GOLDEN | the golden cases of the 8 capabilities, the earlier cases that waited for Tcl, the Desktop Alpha cases | `tests/e2e/`, `tools/golden/` (authoring `script_cases.py`, `m4_cases.py`, additions to `cell_cases.py`, `model_cases.py`, `ui_cases.py`, `drc_cases.py`; comparators, masks, the capture image's Magic Tcl/Tk build), `tests/golden/` (`CASES.md` rows, `cases/`, `expected/`, `deviations.toml`) |
| INTEGRATION | — | everything shared: root and `src/core/CMakeLists.txt`, `src/core/src/{script,pcell}/{sources.cmake,codes.cpp}` and the shared `script/` headers, `CMakePresets.json`, `cmake/` (other files), `tests/unit/CMakeLists.txt`, `tests/fuzz/`, `src/core/{include/hefesto,src}/{cmd (other files),diag,platform,settings,compat,util,editor,app (other files),pdk (other files)}/`, `src/cli/`, `.github/` (other steps), `scripts/` (other files), `README.md`, `docs/roadmap.md`, `docs/building.md` (other sections), `docs/contributing/`, `docs/decisions/`, `docs/diagnostics/codes.md`, `THIRD_PARTY_NOTICES.md`, `openspec/specs/` (open items), this change's artifacts |

Rules (unchanged from M1–M3 D2):
- A workstream edits only what it owns. A needed change to a shared or another workstream's file is an `open_item` (what, why, the exact edit if small); the owner or INTEGRATION applies it.
- New golden case directories are authored by GOLDEN from the requirement and the owning workstream's notes; `expected/` is captured in CI.
- Only the integration step commits and pushes, to `origin m4-scripting` (or the branch its prompt names).

Dependencies: PDK (section 1) lands first, before the scaffold's merge, because every PDK-based test of M4 uses the new pins and the M1–M3 re-run must be clean or decided before new code depends on it. The scaffold (section 2) then publishes the Tcl build, the interfaces of D4 with stubs and the command stubs. TCL, BINDING, PDKINT, PCELL, TOOLS and GUIMGR start at once on the scaffold's interpreter handle: BINDING and PDKINT run their tests on the scaffold interpreter (unsandboxed until TCL lands its filesystem layer, which only adds denials), PCELL tests the framework through the binding stubs and its C++ API, TOOLS and GUIMGR are mostly C++ with Tcl-visible names. DESIGN boards land before SHELL builds a screen (D13). CARRY has no dependency on the other workstreams. GOLDEN authors inputs from day one; replay is meaningful once BINDING and PDKINT land.

### D3. Build and stub contract

- **Tcl build.** `cmake/Tcl.cmake` (TCL, written at scaffold time) fetches the Tcl 8.6.16 source tarball by URL with a pinned SHA-256 through FetchContent and builds `libtcl` statically with Tcl's own `unix/configure` driven by `ExternalProject` (macOS and Linux, all presets including the sanitizer and fuzz presets, and the `ipados` cross build), without Tk, with threads enabled and deprecated commands kept (PDK scripts may use them), the bundled `init.tcl`/`library/` embedded as a zip-free in-memory script set (`Tcl_SetPreInitScript` plus a read-only virtual library root), so no Tcl file is read from the host. Tcl is behind a `hefesto::tcl` target; only `script/tcl/` includes `tcl.h`.
- **Why 8.6, not 9.0.** Magic's reference build and the PDKs are exercised on Tcl 8.6; Q-SCRIPT-22 fixes the naming hash to Tcl 8.6 semantics; Tcl 9 changes `binary`, removes `case` and alters encoding errors in ways the PDK scripts have not been checked against. The binding uses only APIs present in both (`Tcl_CreateObjCommand`, `Tcl_EvalObjEx`, `Tcl_FSRegister`), so a later move to 9.x is a build change plus a golden re-run.
- **Stubs.** The scaffold registers `tag`, `gencell` (and the framework procedures of `pcell-toolkit` under `magic::`), `readspice`, `cif`, `drc_save_report`, `drc_load_report` and the GUI manager entry commands with stub handlers that fail with a new code `CORE-NOTIMPL-004` ("not implemented yet: <command>"), so completion, help, scripts and golden inputs see the final names. INTEGRATION retires `CORE-NOTIMPL-004` at M4 exit.
- **Placeholder reader.** `cmd::run_script` keeps working until BINDING lands `script::evaluate`; then every caller (CLI `-c`, `source`, startup files, macros with Tcl text) moves to the interpreter and the placeholder reader and `fuzz_command_script` are removed at exit (the fuzz target is replaced by `fuzz_tcl_sandbox`).
- **Scaffold tests** carry `[m4-scaffold]` and no `[cap:][req:]` tags; owners replace them.

### D4. Interpreter, session and shared interfaces

`editor::EditorState` gains `script` (`script::ProjectInterpreter`, owned by INTEGRATION as a struct whose members are owned by the named workstream):
- `interp` (the Tcl interpreter handle, created when the project opens, deleted when it closes — one per open project, Q-SCRIPT-41) and `sandbox` (`script::SandboxPolicy`: granted roots, denied commands) — TCL;
- `tags` (command → tag script), `path_cache` (Q-SCRIPT-5 cache with the "differs" warning set), `output` (stdout/stderr routing, console flag) — BINDING;
- `pdk_trust` (per PDK version), `helper_origins` (which PDK defined each `magic::` procedure, for the Q-SCRIPT-41 warning), `menu_model` (the native menu actions produced by intercepted widget calls) — PDKINT;
- `gencell` (`pcell::FrameworkState`: dialog models, registered toolkit menus) — PCELL.

Shared interfaces created by the scaffold (INTEGRATION owns the files; the named workstream fills the implementation):
- `script::Interpreter` (`script/interp.hpp`): `evaluate(std::string_view script, Origin) -> CommandResult`, `source(path)`, `set_var`/`get_var`, `define_command(name, handler)`, `info_script()` — TCL. Command results cross as `cmd::Value` ↔ `Tcl_Obj` (lists stay lists).
- `script::bind_registry(Interpreter&, cmd::CommandRegistry&)` (`script/binding/bind.hpp`): registers every command in `magic::` with Magic's rules — BINDING.
- `script::ScriptEvents` (`script/events.hpp`): observer list for denied operations (file, line, operation), skipped widget statements, helper redefinitions, consent requests and menu-model changes; consumed by the shell, the CLI and the console.
- `script::ConsentHost` (`script/pdk/consent.hpp`): `ask(PdkIdentity, script_path) -> Decision` — implemented by the desktop dialog (SHELL), by a CLI policy (`--trust-pdk`, default decline in batch), and by tests.
- `script::WidgetInterceptor` (`script/pdk/widgets.hpp`): the Q-SCRIPT-42 mapping as Tcl commands for `${f}.titlebar…` paths and `tk_getOpenFile`, emitting `MenuModel` edits and file-picker requests through `platform::FilePicker` — PDKINT.
- `pcell::DialogModel` (`pcell/framework/dialog.hpp`): the platform-neutral model the `magic::add_entry/add_checkbox/add_selectlist/add_message/add_dependency` API builds (parameters, widgets, values, dependencies, buttons); SHELL renders it, iPad renders it in M14.
- `pcell::NativeGenerator` registry (`pcell/native/registry.hpp`): `find(pdk_id, pdk_version, device) -> const NativeGenerator*` — PCELL.
- `gui::` view models (`gui/managers.hpp`): `CellManagerModel`, `LibraryManagerModel`, `TechManagerModel`, `DrcManagerModel`, `TextHelperModel`, `ToolbarModel`, `WrapperMenuModel` — GUIMGR.
- `platform::FilePicker` (`platform/file_picker.hpp`): open/save picker requests answered by the shell (and a scripted test host).

### D5. Embedded Tcl and the sandbox (TCL; X-3, `hefesto-security`)

- **One full interpreter per project, then removed capabilities.** A Tcl safe interpreter removes `source`, `file`, `glob`, `open`, `info script` and more that PDK scripts need; re-adding them as aliases changes error texts and `info script` semantics. Instead the project interpreter is a normal interpreter from which TCL deletes `exec`, `socket`, `load`, `unload`, `exit` (replaced by the engine's `quit` path), `interp create` without `-safe`, and `package require` of anything but Tcl's own pure-Tcl packages; `open` rejects command pipelines (`|…`); `cd` is allowed only within the granted roots (D-33), and `env` is a synthesized array holding `PDK_ROOT`, `PDK` and `CAD_ROOT` from the PDK store and settings (the Q-CELL-17 path-variable table), whose writes stay local to the interpreter and never reach the process environment (D-31).
- **Filesystem layer.** A `Tcl_Filesystem` registered with `Tcl_FSRegister` sits in front of the native one: every path operation (`source`, `open`, `file *`, `glob`, `cd`, `load`) is resolved to a normalized absolute path and allowed only under a granted root of `hefesto-files-and-pdks` (the project folder, the PDK store, the session temporary area, explicitly opened documents). A denied access raises a Tcl error naming the operation and posts a `SCRIPT-SANDBOX-*` diagnostic with the script file and line (from `info frame`), which `catch` can absorb as Magic's PDK code expects (older rc `query_*` procedures).
- **Enforced in the core.** The sandbox is set up by `script::create_project_interpreter`, which the shells cannot bypass; there is no API to obtain an unsandboxed interpreter outside unit tests (a test-only build flag).
- **Resources.** A Tcl resource limit (`interp limit` command count and time) guards evaluation from the GUI so a runaway script can be interrupted (Cancel in the console and consent-run progress), mapped to the existing interrupt path.

### D6. Command binding (BINDING; `scripting-api`)

- **Registration.** Every registry command becomes `::magic::<name>` (an object command calling `Dispatcher::execute` with origin `script`), then is imported globally with Magic's collision rules (`tcl_<name>` renames, the unknown handler that tries the `tcl_` name, `magic::` precedence) as `scripting-api` specifies; window-directed commands are registered per window path from the shell's window list.
- **Quiet dispatch and status.** Dispatch failures through the namespace are quiet with an empty result; command failures return ok (D-4 default) unless `script-strict`/`hefesto::strict` is on.
- **Tags.** `tag` stores scripts per command; after an ok command the tag runs with D6's single-pass, list-quoted substitution (Q-SCRIPT-3), restoring the result unless `%R`, and the command counter.
- **Output.** Engine text goes through the interpreter's `puts -nonewline stdout|stderr` with `"`, `[`, `]`, `\` and `$` always escaped (Q-SCRIPT-4); flushing calls `::tcl_flush`.
- **Variables.** `VDD`, `GND`, `SUB`, `globals(…)` trace into the project setting of D-14; `CAD_ROOT`, `PDK_*` and `$NAME` path expansion read script variables first, then the environment as `scripting-api` orders them, with Q-SCRIPT-5's cache (ON) or live value (OFF) and the warn-once set in both modes.
- **Mode switches on M1–M3 code.** Q-SCRIPT-14 OFF (any whitespace ends a keyword), Q-SCRIPT-6 OFF (ports only), Q-SCRIPT-7 (`cif scale` float-widened ON, exact OFF; a new `cif` command with the `scale`, `list`/`listall istyle|ostyle` queries backed by the M3 cifoutput catalog — the rest of `cif` is M5), Q-SCRIPT-8 (`drc listall why` in a `util::MagicHashTable` of requested size 16 keyed by the substituted text; `drc_console.py` stops sorting it).
- **drcstate.** `drc::DrcEvents` busy/idle call `magic::drcstate busy|idle` in every interpreter, as the M3 non-goal promised.

### D7. PDK startup, scripts and consent (PDKINT; `scripting-pdk-integration`, `hefesto-security`)

- **Startup sequence.** `app/startup` follows "Script-host startup sequence": the generic helpers, the command-line table with Magic's quirks (Q-SCRIPT-37), startup files evaluated as Tcl at global scope with Magic's error text, `techmanager initall` as a no-op (Q-SCRIPT-33). The literal-`set` reader of M1 (`startup_file_settings`) is removed; the setting layers keep receiving `VDD`/`GND`/`SUB`/`globals` through the variable traces of D6.
- **Toolkit procedures.** Magic's Tcl-side framework (`toolkit.tcl`, `tools.tcl`, the parts of `wrapper.tcl` scripts rely on) is provided as native commands under the same `magic::` names (PCELL, GUIMGR, the M2 stack commands); no Magic Tcl file is executed. The PDK helper names of `scripting-api` are never predefined.
- **Consent and trust.** Explicit startup (a project pinned to a PDK, `-rcfile`) and implicit `<tech>.magicrc` from auto-discovery both go through `ConsentHost` unless the PDK version is trusted in the store (`Provenance::trusted`, recorded per PDK and version). Declined: the file opens read-only with the `minimum` tech (`hefesto-security`). CLI batch mode declines unless `--trust-pdk <id>` or the store already trusts it, so golden replay passes `--trust-pdk` for the pinned PDKs.
- **Cross-PDK warning.** `proc` inside a `source` whose file lies under a PDK's `libs.tech/magic` records the PDK as the origin of each `magic::` procedure it defines; a redefinition from another PDK's file posts `SCRIPT-PDK-*` naming both (Q-SCRIPT-41).
- **Widget interception (Q-SCRIPT-42/34, D-32).** `WidgetInterceptor` defines Tcl commands named by the window frame paths the binding announces (`.layoutN.titlebar…`), so `cget -height` errors (selecting `.pane.top`), `…drc.toolmenu add command` and `…file.toolmenu insert 4 …` edit the `MenuModel`, `tk_getOpenFile` asks `platform::FilePicker`; any other widget statement is skipped with a diagnostic and evaluation continues (the wrapper's `catch` no longer hides the rest of the menu setup). `closewrapper` renaming works because `closewrapper` is a real command.
- **`quit` from a startup script (D-36).** `quit` goes through a core `QuitPolicy`: the desktop policy exits as before; the iPad policy stops evaluating the script, discards the project session (after the exit checks unless `-noprompt`) and returns to the project browser with a diagnostic naming the script and line. Both are unit-tested with test hosts; the iPad shell installs its policy in M14.
- **Curated-pack refusal (Q-SCRIPT-46, D-30).** A core policy `script::CuratedPackPolicy` (unit-tested now, used by the iPad shell in M14) refuses a PDK without bundled scripts; there is no declarative profile.
- **Older rc generation.** Runs unchanged; the sandbox denial in `query_*` is absorbed by `catch` (SCRIPT-PDK-18).

### D8. Tcl-visible framework in C++ (PCELL, TOOLS, GUIMGR)

Magic implements gencell, readspice, the managers and `drc_save_report` in Tcl/Tk. Hefesto implements them in C++ behind the same procedure names and argument forms, because the shells are native (no Tk), the X-3 native fast path needs the framework in C++ anyway (naming, hash, properties), and the fixed behaviors (Q-SCRIPT-19, 21, 23, 25, 26, 27, 28, 29, 32) are then plain code. The ported procedures call back into the interpreter for PDK code (`<lib>::<type>_defaults|_convert|_dialog|_check|_draw`, dialog callbacks). Each port cites its `tcltk/*.tcl` source lines like the M1–M3 ports of C files.

### D9. gencell framework (PCELL; `pcell-toolkit`)

- **Dispatch and algorithms.** `magic::gencell` dispatch table, create (with Q-SCRIPT-24's reuse), change (Q-SCRIPT-23's fixed-device counts, Q-SCRIPT-29's headless name), non-placing generation, legacy random names, `add_dependency` without names (Q-SCRIPT-25).
- **Naming.** Normalization (Q-SCRIPT-21: Inf/NaN verbatim) and MurmurHash3 over characters' low bytes (Q-SCRIPT-22), computed in C++ from the dictionary's string forms; a table test pins names from SCRIPT-PCELL captures.
- **Dialogs.** The builder API fills `pcell::DialogModel`; dependencies and validation run `update_dialog` semantics on model edits; selection-driven refresh and the ^P gencell binding (UI-G-049) are commands. Creating from the dialog issues `magic::gencell … k v …` so every action is logged and replayable.
- **SPICE seeding.** `magic::netlist_to_layout` and subcircuit seeding.

### D10. Native generators (PCELL; `hefesto-scripting`)

- **Scope in M4.** The MOSFET families (every device that delegates to the PDK's `mos_draw`: sky130 `sky130_fd_pr__{n,p}fet_*`, gf180mcu `{n,p}fet_*`, sg13g2 `sg13_{lv,hv}_{n,p}mos`) and the fixed (`nocell`) devices of the three PDKs (12 + 11 + 8). sg13g2 fixed devices wait for SCRIPT-PDK-14 to confirm Q-SCRIPT-45's derivation. Every other device uses its Tcl generator (allowed by "Native PCell generators validated against Tcl": native only when it exists for the installed PDK version).
- **Versioned.** A native generator is registered for an exact PDK identity (family, variant, ciel version hash) and is used only for that version; another version falls back to Tcl with an informational diagnostic.
- **Bit-identical arithmetic.** The PDK helpers (`spice2float` with its defects (Q-SCRIPT-40), `u2i`, `i2u`, `3digitpastdecimal`, sg13g2's `%.3f`/`round`) are reproduced in C++ with Tcl's double formatting and `expr` rounding; the `label … +layer` failure (Q-SCRIPT-39) is reproduced.
- **Validation.** A CI job (`pcell-equivalence`, Linux x86_64) generates each native device over a parameter sweep (defaults, each parameter at its PDK bounds and one interior value, `nf`/`m` combinations; about 2,000 cells) natively and through Tcl in the same build and compares names and saved `.mag` files under D-29: byte-identical except the `timestamp` line. The sweep definitions are data in `tests/data/pcell/sweeps/`.

### D11. readspice and auxiliary tools (TOOLS)

- `readspice`: file candidates, preprocessing with Q-SCRIPT-32's two continuation modes, subcircuit recognition, port renumbering with the fixed message, PININFO with `switch` semantics (Q-SCRIPT-19). Its `port last` follows Q-SCRIPT-6.
- Auxiliary: the socket server is absent (sandbox, `hefesto-security`); `bsitools`, the legacy rev-0 toolkit and the technology-builder prototype are not loaded by default and are provided as documented informative behavior; the reference-library stripper, console integration and launcher behaviors are mapped to `hefesto-cli` options. `mazeroute` is registered with a `TOOLS-*` diagnostic saying it arrives with `router-maze-iroute` (M10), and its requirement's test moves to M10 (Q-SCRIPT-30).

### D12. GUI managers (GUIMGR; `ui-gui-managers`)

View models and commands, no widgets: wrapper construction and hook points (`Opts(callback)`, `Winopts(<f>, callback)` then the no-space key, Q-SCRIPT-13), wrapper menus (Q-SCRIPT-36) and option variables, technology manager, layer toolbar and presets parsed as data (Q-SCRIPT-11, 28; the parser is a new untrusted reader with a fuzz target), cell manager, library manager (Q-SCRIPT-27), DRC manager and `drc_save_report`/`drc_load_report` (Q-SCRIPT-26), text helper (Q-SCRIPT-31), file prompts, crash-backup timer, tool modes. The `3-D view wrapper` requirement stays informative until M10.

### D13. Desktop screens (DESIGN, SHELL)

- **Boards first** (`hefesto-design-system` "Screen patterns follow the reference mockups", scenario "New screen"): DESIGN adds `M4-Menu-Bar.dc.html` (the desktop menu bar File/Edit/Cell/Window/Layers/Drc/Options with PDK `Devices` menus, the intercepted `DRC Routing` and `Import SPICE` items, skipped-statement report), `M4-PCell-Dialog.dc.html` (Place device from the tool rail and menus, the parameter dialog in its new/edit/invalid/headless-error states, dependencies, Apply/Create/Close, instance name), `M4-Script-Consent.dc.html` (consent prompt showing the script origin and PDK version, trusted-PDK list in the PDK panel, declined read-only state, sandbox-denial and long-running script states), `M4-Managers.dc.html` (cell, library and technology managers, text helper, layer toolbar and reorder dialog), and `M4-Console.dc.html` (the console panel as a Tcl console: multi-line input, stdout/stderr, interrupt). Later-milestone elements are disabled with their milestone. The user approves the boards before SHELL builds the screens.
- **Commands only.** Every screen action is a logged command (`magic::gencell …`, `load`, `getcell`, `drc …`, `setlabel …`), as in M2/M3.
- **Menu bar.** The native menu bar is built from `gui::WrapperMenuModel` plus `script::MenuModel` edits; Raven's menu support decides between a native macOS menu bar and an in-window bar on Linux (gap reported if Raven lacks one).
- **Conformance record.** `docs/design/m4-ui-conformance.md`, same format as M3's.

### D14. Ownership of the M1–M3 changes M4 makes

| Change | Owner | Decision |
|---|---|---|
| keyword terminator OFF rule | BINDING | Q-SCRIPT-14 |
| `port last` OFF | BINDING | Q-SCRIPT-6 |
| path-variable cache and warning | BINDING | Q-SCRIPT-5 |
| `drc listall why` hash order; comparator sort removed | BINDING; GOLDEN (comparator) | Q-SCRIPT-8 |
| startup files through Tcl; literal-`set` reader removed | PDKINT | `scripting-pdk-integration` "Startup files are scripts" |
| auto-discovery rc step with consent | PDKINT | Q-SCRIPT-10, `hefesto-security` |
| `instance list`, `what -list` results | PCELL | `scripting-api` "Result formats" |
| `load -silent <cell>` argument order | PDKINT | generator command surface |
| PDK pins and re-run | PDK | Q-SCRIPT-9 |

### D15. PDK pin move (PDK; Q-SCRIPT-9)

- **Releases.** ciel publishes no release for open_pdks `c6df28c1`. The next release, `sky130-5740c4a81c42f9fe4af5ef2ee258a99151053d28` and `gf180mcu-5740c4a8…` (2026-11-10), is three commits after `c6df28c1` and changes only `VERSION` and the `reference` blocks of `sky130.json`/`gf180mcu.json` (it records `c6df28c1` as its open_pdks reference), so its Magic files are those of the spec pin. The reference blocks also move `sky130_fd_pr` (403964dc → 29b55153) and four gf180mcu sub-libraries; `sky130_fd_sc_hd` and `gf180mcu_fd_sc_mcu7t5v0` are unchanged. IHP: `ihp-sg13g2-5e6d592e4002946a4616f798c357f0f3c06cf3b6` exists (2026-09-01; `common.tar.zst` 434 MB). PDK 1.1 verifies these facts at implementation and records them here; if the Magic files differ from `c6df28c1` in any byte, it stops and raises an Open Question.
- **Sets.** Same three data sets and subsets as M3 (`sky130A/libs.tech/magic` + `sky130_fd_sc_hd/mag`; `gf180mcuD/libs.tech/magic` + `gf180mcu_fd_sc_mcu7t5v0/mag`; `ihp-sg13g2/libs.tech/magic`), new pins, new tree digests, new cache keys.
- **Re-run.** All unit tests that read PDK data (`reference_data.hpp` users) and all golden cases with `source = "pdk"` inputs (DRC-PDK-1..3, the standard-cell corpus cases DRC-STD-SKY130-HD and DRC-STD-GF180-7T5V0, the M1/M2 PDK cases) are re-captured and replayed on the new pins. Every changed expectation is listed in design D15 "Re-run results" with its cause; a change that is not explained by the PDK content needs an approved decision before it lands.
- **Older rc smoke.** The older rc files are tiny; GOLDEN keeps copies of the `0fe599b2` sky130A `.magicrc` and `ddb601a4` `ihp-sg13g2.magicrc` (Apache-2.0, with notices) as case inputs of SCRIPT-PDK-18, run against the new-pin tech files.

### D16. Golden cases

- **Authoring.** `tools/golden/authoring/script_cases.py` (the 89 SCRIPT cases and SCRIPT-PDK-18) and `m4_cases.py` (driver as `m3_cases.py`). The capture image builds Magic fc5daa9e with Tcl 8.6 and Tk and runs GUI cases (SCRIPT-GUI-*) under Xvfb; Hefesto replays GUI cases through the GUIMGR view models and `hefesto-cli` (no display), comparing the issued commands and files, not pixels.
- **Other areas.** MODEL-G-055, CELL-PR-03, CELL-PW-05 and CELL-PR-12 (its `cd` into a project subfolder, D-33) authored; UI-G-009, UI-G-013 and UI-G-049 lose `deferred = "M4"`; DRC-PDK-3 uses the IHP `cap_cmomf` generator through Tcl, its row reworded back.
- **Deviations.** Q-SCRIPT-3 (`%%`/`%d` tag: Magic hangs; captured with a timeout), Q-SCRIPT-4 (SCRIPT-API-13 and any `$name` message), Q-SCRIPT-32 (readspice message), each with its approved decision. Cases run in the default magic-compat ON mode; OFF modes are unit-tested.
- **PDK cases.** Run with `--trust-pdk` for the pinned PDKs and `random seed 1` after startup (Q-SCRIPT-44).

### D17. Items carried from M2 (CARRY)

| Item (roadmap "Carried from M2") | M4 home |
|---|---|
| Raven confirmation checks: frame-rate run on the reference M1 Mac and Linux with a discrete GPU (M0 task 3.6), live mouse/keyboard/trackpad input on Linux X11 and Wayland, Orca, filing the Raven issues upstream | CARRY (manual runs recorded in `docs/decisions/raven-go-no-go.md`) |
| Floating panels and drag-to-dock, panel headers, drop-zone highlight, collapsed edge strips | CARRY |
| Panel layout saved per user and restored at launch | CARRY |
| Modal key-binding sheet: capture, conflict warning with Replace, Reset table, Load file | CARRY |
| Label text entry as a canvas popover (needs Raven's anchored popover); Escape after a measure issues `unmeasure`; Paint tool disabled until a paint layer is chosen | CARRY |
| Undo steps named after their user command; undo history on press-and-hold of Undo | CARRY (`undo/` per-step label) |
| Per-area minimap update | CARRY |
| Pixel-width box and selection outline in `render::highlight_batches` | CARRY |
| Wiring tracking HUD, "Wiring · tracking" chip, Wire inspector | CARRY |
| Capture of the crash-recovery dialog | CARRY |
| VoiceOver press on custom-drawn controls, once Raven exposes an accessible press | CARRY (blocked on Raven; recorded as a gap if still missing at exit) |

These use the approved M2 boards; no new board is needed.

### D18. Desktop Alpha exit check

- **Scripted end to end** (`tests/e2e/desktop_alpha_{sky130,gf180mcu,sg13g2}.tcl`, run by `hefesto-cli` in CI on Linux x86_64, Linux aarch64 and macOS arm64): start with the PDK's rc file (trusted), open a design from the PDK's standard-cell library, edit (paint, label, move), place one generated device of each native family and one Tcl-only device through `magic::gencell`, run `drc check; drc catchup`, save, and compare the saved files and `drc listall why` with the same script run in Magic (a golden case each, `DESKTOP-ALPHA-*`).
- **Manual on the desktop.** A checklist in `docs/design/m4-ui-conformance.md` "Desktop Alpha" run on Linux and macOS through the GUI: consent prompt, PDK menus, Place device dialog, DRC browser on the result, save and reopen. The user signs off.

### D19. Security review

- The sandbox, consent and toolbar-data decisions are security changes (CLAUDE.md: OpenSpec for security). INTEGRATION runs a review at exit: denied-operation tests for every removed command and filesystem path class (relative escapes, symlinks out of a granted root, `~user`, UNC/`//` forms, `glob` patterns, `open |cmd`, `interp create`, `package require` of binary packages, `load`), the fuzz target `fuzz_tcl_sandbox` (random scripts must never touch a path outside the temporary root, checked by an `LD_PRELOAD`-free filesystem audit through the VFS log), and a written summary in `docs/contributing/security.md`.

### D20. App Review inquiry (early de-risking)

The roadmap asks to submit the App Review inquiry about running sandboxed Tcl on iPadOS at the start of M4. INTEGRATION drafts the inquiry (what runs, the sandbox, consent, no downloaded native code, curated-pack fallback) in `docs/decisions/app-review-inquiry.md` in section 1; the user submits it through App Store Connect (an Apple developer account action the agents cannot take) and records the answer there. M4 does not depend on the answer: Q-SCRIPT-46 already fixed the curated-pack behavior, and the answer decides only M14's mode (Open Question 2).

## Area contracts

The headers are the contract; each is documented. Key types and functions per area (the scaffold writes them with stubs):

### TCL — `hefesto/script/tcl/`
- `create_project_interpreter(EditorState&, const SandboxPolicy&) -> std::unique_ptr<Interpreter>`; `SandboxPolicy` (granted roots, denied commands, limits); `SandboxFilesystem`; `DeniedOperation` events; `to_tcl(const cmd::Value&)`, `from_tcl(Tcl_Obj*)`.

### BINDING — `hefesto/script/binding/`
- `bind_registry(...)`; `TagTable` and `substitute_tag(script, CommandContext) -> std::string`; `escape_output(text)`; `VariableTraces` (VDD/GND/SUB/globals, PDK_*, CAD_ROOT); `PathVariableCache`; window-path command registration; `register_cif_command`.

### PDKINT — `hefesto/script/pdk/`
- `run_startup_files(EditorState&, const StartupPlan&)`; `CommandLineTable` (Magic's patterns); `ConsentHost`, `TrustStore` (per PDK version); `run_tech_magicrc(EditorState&, const PdkIdentity&, path)`; `HelperOriginTracker`; `WidgetInterceptor`, `MenuModel`; `CuratedPackPolicy`.

### PCELL — `hefesto/pcell/`
- `normalize_value`, `gencell_hash`, `generated_cell_name`; `gencell(...)` dispatch, `create`, `change`, `makecell`; `DialogModel` and the builder commands; `ToolkitMenus`; `netlist_to_layout`; `NativeGenerator` (`defaults`, `check`, `draw`), `NativeRegistry`; `PdkHelpers` (bit-identical `spice2float`, `u2i`, `i2u`, `3digitpastdecimal`, per PDK).

### TOOLS — `hefesto/tools/`
- `readspice(EditorState&, path, options) -> int`; `SpiceLineReader` (continuation modes); auxiliary tool commands.

### GUIMGR — `hefesto/gui/`
- the view models of D4; `parse_toolbar_presets(text) -> PresetFile` (data only); `write_current_toolbar(...)`; `drc_save_report`, `drc_load_report`.

### SHELL — `hefesto/shell/`
- `MenuBar`, `PCellDialog` (renders `DialogModel`), `ConsentDialog` (implements `ConsentHost`), `CellManagerPanel`, `LibraryManagerPanel`, `TechManagerPanel`, `TextHelper`, `LayerToolbar`/`ReorderDialog`, `ScriptConsole`, the `FilePicker` host.

## Risks / Trade-offs

- **R1. Sandbox escape.** A full interpreter with commands removed is a larger surface than a safe interpreter. → Removal plus a filesystem layer in front of every path operation (D5), denial tests per command and path class, a fuzz target with a filesystem audit, and the D19 review; `interp create` is safe-only.
- **R2. PDK scripts relying on Tk or on removed commands.** Unknown widget calls or `exec` in a PDK's menu code would lose items. → Interception reports every skipped statement (Q-SCRIPT-42), SCRIPT-PDK-09's command trace checks the inventory, and SCRIPT-PDK-07/08 run every menu entry; a newly found call is mapped through an approved decision.
- **R3. Native generator drift.** Floating-point formatting or `expr` rounding differences make native names or geometry differ from Tcl. → Reproduce Tcl's `%g`/`%.3f` and `round` exactly (D10), the sweep job on every PR touching `pcell/`, native generators registered per exact PDK version.
- **R4. Pin move fallout.** The new pins change `sky130_fd_pr` and gf180 sub-libraries and may change M1–M3 expectations (PDK-based DRC cases, the standard-cell corpus). → Done first (section 1), each change explained or decided; the Magic files are checked to equal `c6df28c1` (D15).
- **R5. Tcl build portability.** Building Tcl from source for 8 CI legs, sanitizers, fuzzing and `ipados` can fail in ways unrelated to Hefesto. → One CMake module, a pinned tarball and hash, cached by preset; the `ipados` build is compile-only in M4.
- **R6. App Review outcome.** A rejection forces curated packs on iPadOS. → Already decided (Q-SCRIPT-46); M4 code is the same either way (D20).
- **R7. Scope.** 8 capabilities, 57 testable requirements, 90 SCRIPT golden cases, native generators for three PDKs, five new screens and twelve carried shell items. → Workstreams independent after the scaffold; native generators limited to MOSFETs and fixed devices; GUI cases compared by commands and files, not pixels.
- **R8. Byte-identical PCell output (D-29).** Comparing whole `.mag` files (not geometry) also exposes differences in element order, property formatting and label attributes between the native and Tcl paths. → Both paths save through the same `.mag` writer; only the `timestamp` line is masked; the sweep runs on every PR touching `pcell/`.

## Migration Plan

User data does not change. Projects with a PDK pin now run the PDK's startup script after consent instead of applying its literal `set` lines; a project that was opened before M4 keeps working when the user declines (read-only with the `minimum` tech, per `hefesto-security`) — this is a visible behavior change documented in the release notes and `docs/building.md`. Saved `.mag` files are unchanged; generated cells keep their names. `drc listall why` changes order (Magic's) and `port last`/path expansion change only with `magic-compat` OFF. The CI reference-data cache keys change once (new pins). `CORE-NOTIMPL-004` exists only during M4; the placeholder script reader and `fuzz_command_script` are removed at exit.

## Resolved Questions

The plan's open questions were answered by the user on 2026-11-11.

1. **Audit decisions** (docs/decisions/audit/DECISIONS.md), all approved as recommended, with the spec edits made in the commit "Apply the user's answers to the M4 plan questions":
   - **D-29**: native and Tcl generator output are byte-identical saved `.mag` files except the `timestamp` line (`hefesto-scripting`, `pcell-toolkit`); D10's equivalence job compares that way.
   - **D-31**: the sandbox's `env` array is synthesized with `PDK_ROOT`, `PDK` and `CAD_ROOT` from the PDK store and settings, writes local to the interpreter, on every platform (`hefesto-scripting`); D5.
   - **D-33**: `cd` is allowed within the granted roots and fails otherwise (`hefesto-scripting`); the `scripting-api` scenario "Relative cell path survives cd" uses a project subfolder; CELL-PR-12 is authored that way.
   - **D-34**: `netgen` prints a diagnostic naming the built-in `lvs`, `irsim` follows `irsim-interface` (desktop only), `xcircuit` reports "not available" (`scripting-api` "Generic Tcl helpers defined at startup"); BINDING 5.3.
   - **D-36**: on iPadOS `quit`, including a PDK rc guard's `quit -noprompt`, aborts loading, returns to the project browser and shows a diagnostic (`app-startup` "quit command and exit status"). D7: M4 implements it as a core quit policy with a desktop policy (exit, unchanged) and an iPad policy (abort to the project browser) unit-tested through a test host; the iPad shell uses it in M14.
2. **Native generator scope**: confirmed as in D10, the MOSFET families plus the fixed devices; every other device runs its Tcl generator.
3. **Pin release substitute**: confirmed as in D15. The spec pin stays open_pdks `c6df28c1`; ciel `5740c4a8…` is its documented stand-in in `fetch_pdk.py` and `docs/building.md`.

Not blocking (no decision needed now):

- **App Review inquiry.** Not submitted yet. INTEGRATION drafts it (D20, task 2.2); the user submits it through App Store Connect and records the answer. M4 does not depend on it; it decides only M14's mode.
- **Deferred questions.** Q-SCRIPT-30 (`mazeroute`) is implemented with `router-maze-iroute` in M10; Q-SCRIPT-44's seed-pinning setting is decided with Q-MASK-211 in M5. Q-SCRIPT-43 (sg13g2 `$SUB` and `$[…]` in extraction) is handed to the EXT area for M7.
