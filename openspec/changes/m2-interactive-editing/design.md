# Design: M2 Interactive editing and design system

## Context

See proposal.md for motivation. M1 (openspec/changes/m1-core-viewer) delivered the technology and layout databases, `.mag` I/O, display styles and fonts, the Vulkan renderer, the core view commands, the PDK manager, settings and startup, and a desktop viewer on the forge design system. The conventions of M0 and M1 still hold: SPDX headers, Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`, per-area diagnostic code tables `AREA-TOPIC-NNN` documented in `docs/diagnostics/codes/<area>.md`, cognitive complexity at most 15 per function (flag irreducible ones), no UI/GPU/Raven/process headers in the core, golden expected outputs captured only in CI (`.github/workflows/golden-capture.yml`), Magic 8.3.694 at fc5daa9e as the behavioral reference (`build/ref/magic`, gitignored), clean-room rule for netgen/xschem/KLayout.

What M1 left that M2 builds on (from the code, 2026-11-09):

- **Commands.** Registered by area through `cmd::register_all_commands`. The registry is an exact-name `std::map`: no unique-prefix lookup, no case folding, no window clients. `box` has only the bare report, four coordinates, `values`, `size`, `position`, `width`, `height`; `paint`/`erase` take the box area only; `getcell` takes `name [orientation]`; `setlabel` addresses the labels in the box (no selection); `array` is registered by `db`. Missing: `select`, `what`, `identify`, `delete`, `copy`, `move`, `stretch`, rotations, `orient`, `dump`, `edit`, `down`, `xload`, `flush`, `findlabel`, `goto`, `element`, `undo`, `redo`, `macro`, `imacro`, `tool`, `wire`, `units`, `fill`, `corner`, `contact`, `polygon`, `spliterase`, `drop`, `xor`, `random`, `shell`, `feedback`, `setpoint`, `pushbutton`, `logcommands`, `cursor`, the box/edit stacks and the measurement helpers.
- **Coordinates.** `cmd::parse_coordinate` (`cmd/coordinates.hpp`) parses one term with `l`/`i`/`g`/metric suffixes; expressions are not parsed. `db/command_util` keeps a second integer parser, and Magic's unique-prefix `lookup()` is duplicated in `db/command_util` and `view/command_util`. `units` is a settings-layer key, not editor state.
- **Database.** `db::paint`/`erase` mark the cell modified and return a status; there is no change observer or undo hook. Redisplay is reported by the command layer (`WindowManager::area_changed`). There is no connectivity search, only `Technology::connects_to`.
- **Editor state.** `editor::EditorState` holds technology, grid, cells, edit cell, box and box root, locked types, snap, current use, label defaults, paths, locks, cell session, styles, fonts, windows and settings. There is no selection, undo log, macro table, tool or feedback state.
- **View.** Redisplay tracking (`view::RedisplayTracker`, `Change`) exists; there is no highlight layer, no feedback, no tile-structure overlay. The box is drawn by the shell overlay.
- **Desktop.** Arrows/`+`/`-`/`f` issue `pan`/`zoom`/`view` with origin `key`; wheel, drag and pinch move the camera directly; a left click on the layout does nothing. The tool rail shows Box (active, no state behind it) and disabled Select, Paint, Wire, Label, Measure, Place device. The command bar has Up/Down history and no completion. The inspector fills only BOX and CELL. No crash-recovery dialog (`confirm_recovery` unset → `read`). The problem count never resets. The status bar shows the first line of a multi-line result.

M2 runs as a scaffolding step (interfaces, `EditorState` members, command stubs, build contract), a design step for the new screens, eight parallel workstreams and an integration step. The directory layout, public headers, ownership table and shared-file rules below are the contract that lets the workstreams work without editing each other's files.

## Goals / Non-Goals

**Goals:**
- The M2 exit criteria of docs/roadmap.md: the common exit criteria for the 19 M2 capabilities (unit tests tagged for every exact/behavioral requirement, 163 of them; fuzz targets for new readers; CI green), the CMDEDIT golden cases and the UI command cases pass (identical or a documented deviation in `tests/golden/deviations.toml`), and an internal usability review of the M2 UX design artifact is done.
- A desktop editor in which every M2 interaction (box, select, paint, erase, copy, move, stretch, rotate, label, port, wire, measure, undo/redo, macros, command bar) is a command of the single command API, logged and replayable.
- The M1 items deferred to M2 are closed (D10).
- Interfaces stable enough that M3 (continuous DRC on edits), M4 (Tcl calling every command, `magic::` helpers, PCells placing cells) and M7 (connectivity reused by extraction tools) add behavior without reshaping them.

**Non-Goals:**
- DRC, Tcl, PCells, GUI managers, plowing, routing, the netlist/rsim tool actions, GDS/LEF/DEF/extraction (proposal "Non-goals").
- iPadOS gestures and Pencil (M14); the core keeps the mapping tables the iPad shell will use.
- Rewriting M1 code that already meets its spec. M2 changes M1 code only where an M2 requirement needs it (for example `box`, `paint`, `setlabel` by selection, `getcell` placement syntax).

## Decisions

### D1. Layout of the M2 code

| Area | Public headers | Sources | Unit tests | Namespace |
|---|---|---|---|---|
| keywords, coordinates, units (shared parsing) | `src/core/include/hefesto/cmd/{keywords,coordinates,units}.hpp` | `src/core/src/cmd/{keywords,coordinates,units}.cpp` | `tests/unit/core/` | `hefesto::cmd` |
| undo log | `src/core/include/hefesto/undo/` | `src/core/src/undo/` | `tests/unit/undo/` | `hefesto::undo` |
| selection | `src/core/include/hefesto/select/` | `src/core/src/select/` | `tests/unit/select/` | `hefesto::select` |
| connectivity | `src/core/include/hefesto/connect/` | `src/core/src/connect/` | `tests/unit/connect/` | `hefesto::connect` |
| dispatch, clients, macros, tools, script helpers | `src/core/include/hefesto/ui/` | `src/core/src/ui/` | `tests/unit/ui/` | `hefesto::ui` |
| highlights, feedback, color window client, tile overlay | `src/core/include/hefesto/view/{highlights,feedback,color_window,tile_overlay}.hpp` | `src/core/src/view/` | `tests/unit/view/` | `hefesto::view` |
| wiring | `src/core/include/hefesto/wiring/` | `src/core/src/wiring/` | `tests/unit/wiring/` | `hefesto::wiring` |
| layout database, paint commands | `src/core/include/hefesto/db/` (existing) | `src/core/src/db/` | `tests/unit/db/` | `hefesto::db` |
| hierarchy and cell commands | `src/core/include/hefesto/cell/` (existing) | `src/core/src/cell/` | `tests/unit/cell/` | `hefesto::cell` |
| renderer (selection and highlight batches) | `src/render/vulkan/include/hefesto/render/` | `src/render/vulkan/src/` | `tests/unit/render/` | `hefesto::render` |
| desktop shell | `src/shell/desktop/include/hefesto/shell/` | `src/shell/desktop/src/` | `tests/unit/shell/` | `hefesto::shell` |

All new core code is part of `hefesto_core` (no new library targets). `src/core/CMakeLists.txt` adds `undo select connect ui wiring` to `HEFESTO_CORE_AREAS`; `tests/unit/CMakeLists.txt` adds the same test directories through `hefesto_add_area_tests`. New diagnostic prefixes: `UNDO-`, `SELECT-`, `CONNECT-`, `UI-`, `WIRE-` (area tables and documents as in M1 D3).

### D2. Workstreams and ownership

| Workstream (tasks.md section) | Capabilities | Owns (may edit) |
|---|---|---|
| DESIGN | `hefesto-design-system` (screen patterns), `hefesto-app-ux` (information architecture) | the reference design artifact boards for M2, `docs/design/mockups/`, `docs/images/ui/`, `docs/design/m2-ux-review.md` |
| EDIT | `cmd-coordinates-and-units`, `cmd-box`, `cmd-paint-erase`, `cmd-misc-edit` | `src/core/{include/hefesto,src}/cmd/{keywords,coordinates,units}.*`, `src/core/{include/hefesto,src}/db/` except the files HIER owns, `tests/unit/db/`, `tests/data/db/`, the coordinate tests in `tests/unit/core/`, `docs/diagnostics/codes/db.md` |
| UNDO | `layout-undo` | `src/core/{include/hefesto,src}/undo/`, `tests/unit/undo/`, `docs/diagnostics/codes/undo.md` |
| SELECT | `cmd-selection`, `layout-connectivity` | `src/core/{include/hefesto,src}/{select,connect}/`, `tests/unit/{select,connect}/`, `tests/data/{select,connect}/`, `docs/diagnostics/codes/{select,connect}.md` |
| HIER | `cmd-hierarchy`, `cmd-labels-properties` | `src/core/{include/hefesto,src}/cell/`, `src/core/{include/hefesto,src}/db/{label,labels,property}.*`, `src/core/src/db/{commands_labels,commands_cells}.cpp`, `tests/unit/cell/`, `tests/data/cell/`, label/property tests in `tests/unit/db/` (`setlabel_test.cpp`, new `*_labels_test.cpp`), `docs/diagnostics/codes/cell.md` |
| DISPATCH | `ui-command-dispatch`, `ui-tools-and-input`, `ui-box-stack-and-measure`, `ui-wizard-and-debug` (visibility) | `src/core/{include/hefesto,src}/ui/`, `src/core/{include/hefesto,src}/cmd/{registry,dispatcher,script_parser,script_runner,command_log}.*`, `tests/unit/ui/`, `tests/data/ui/`, `docs/diagnostics/codes/ui.md` |
| VIEW | `ui-feedback-and-highlights`, `ui-color-window`, `ui-wizard-and-debug` (tile overlay), selection drawing of `cmd-selection` | `src/core/{include/hefesto,src}/{view,display}/`, `src/render/vulkan/`, `tests/unit/{view,display,render}/`, `tests/data/{view,display}/`, `docs/diagnostics/codes/{view,display}.md` |
| WIRE | `wiring-tool` | `src/core/{include/hefesto,src}/wiring/`, `tests/unit/wiring/`, `tests/data/wiring/`, `docs/diagnostics/codes/wire.md` |
| SHELL | `hefesto-input-and-touch` (desktop), `hefesto-app-ux`, `hefesto-design-system`, the desktop parts of every other M2 capability | `src/shell/desktop/`, `assets/` (icons, fonts), `tests/unit/shell/`, `docs/design/m2-ui-conformance.md` |
| INTEGRATION | — | everything shared: root and `src/core/CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `tests/unit/CMakeLists.txt`, `tests/fuzz/`, `src/core/{include/hefesto,src}/{cmd (other files),diag,platform,settings,compat,util,editor,app,tech,pdk}/`, `src/cli/`, `.github/`, `tools/golden/`, `tests/golden/` (`CASES.md`, `expected/`, `deviations.toml`), `scripts/`, `README.md`, `docs/roadmap.md`, `docs/building.md`, `docs/contributing/`, `docs/decisions/`, `docs/diagnostics/codes.md`, `THIRD_PARTY_NOTICES.md`, this change's artifacts |

Rules (unchanged from M1 D2):
- A workstream edits only what it owns. A needed change to a shared or another workstream's file is an `open_item` (what, why, the exact edit if small); the owner or INTEGRATION applies it.
- New golden case directories `tests/golden/<AREA>/cases/<ID>/` (`case.toml`, `input/`, no `expected/`) are authored by the workstream owning the capability, through the authoring modules (D9); INTEGRATION captures `expected/` in CI and edits `CASES.md` and `deviations.toml`.
- Per-area `codes.cpp`, `sources.cmake` and `tests/unit/<area>/CMakeLists.txt` belong to the area.
- Only the integration step commits and pushes, to `origin m2-interactive-editing` (or the branch its prompt names).

Dependencies between workstreams: the scaffold (section 1) lands first and publishes every interface below with stubs. DESIGN boards for a screen land before SHELL builds that screen. SELECT, HIER and WIRE use EDIT's coordinate parser and paint paths and UNDO's recorder through the scaffold's interfaces, so they can start at once against stubs; their golden cases that combine commands from several workstreams are replayed by INTEGRATION once all have landed.

### D3. Build and stub contract

- **Sources and tests.** As M1 D3: each new area has `sources.cmake` and a globbed test directory.
- **Stubs.** Every M2 command name is registered at scaffold time with a stub handler that fails with a new code `CORE-NOTIMPL-002` ("not implemented yet: <command>"; `CORE-NOTIMPL-001` is retired and never reused), in the area that will own it, so `help`, dispatch, golden scripts and the shell's completion see the final command set from day one. Commands move to their owner: `array` from `db` to `select`; `getcell`/`flatten` stay in `cell`. INTEGRATION retires `CORE-NOTIMPL-002` and deletes the stub helper at M2 exit.
- **Scaffold tests** carry `[m2-scaffold]` and no `[cap:][req:]` tags; owners replace them.

### D4. Session state and shared interfaces

`editor::EditorState` gains, each member's type owned by its area:
- `units` (`cmd::UnitsState`: input unit, print flag; replaces the settings-layer key) — EDIT;
- `undo` (`undo::UndoLog`) — UNDO;
- `selection` (`select::Selection`) — SELECT;
- `macros` (`ui::MacroTables`), `tools` (`ui::ToolRegistry`), `box_stack`, `edit_stack`, `suspend_depth`, `command_point` (setpoint) — DISPATCH;
- `feedback` (`view::FeedbackList`), `highlights` (`view::HighlightLayer`), `color_map` editing state — VIEW;
- `wire` (`wiring::WireState`: current type, width, last leg, direction) — WIRE;
- `edit_transform` and edit root (root↔edit frames of `cmd-coordinates-and-units` "Root and edit coordinate frames", `cmd-hierarchy` "Edit command") — HIER, read by everyone through `editor::to_edit`/`to_root`.

Shared interfaces created by the scaffold (INTEGRATION owns the files; the named area fills the implementation):
- `db::ChangeRecorder` (`db/change_recorder.hpp`): an optional observer the database calls on each primitive change — tile type change over a rectangle on a plane (including split tiles), split-tile fracture/join, label put/erase (full contents), use place/delete (full state), use id clear/set, cell modified flag, property set/remove. `CellLibrary` holds one pointer; null means no recording (zero cost). EDIT and HIER call it from the db primitives; UNDO implements it. Editor-level events (box, selection, edit cell, wiring, color map) are pushed by their areas through `undo::UndoLog::record(std::unique_ptr<undo::Event>)`.
- `cmd::PromptHost` (`cmd/prompt.hpp`): `ask(question, answers, default)` for Magic's `TxDialog` prompts — crash recovery, `flush` without `-noprompt`, `quit`/`load` with a modified color map, `quit` with modified cells. Headless hosts return the default; the desktop shows a Raven message box (closes the M1 "crash recovery confirmation dialog" gap).
- `cmd::PointerState` (`cmd/pointer.hpp`): the window, the pointer point in root coordinates and the pixel area under it, set by the shell before dispatching a button or key macro and by `setpoint`; used by `box … cursor`, point selection, `paint`/`erase` by example, `wire`, `cursor`, `select … at`.
- `editor::post_command` (the post-command update of `ui-command-dispatch` "Command counter and post-command update"): increments the command counter (not for `*bypass`/`Origin::internal`), closes the undo step, runs client update handlers, redraws highlights (D6), then window redisplay.

### D5. Undo

- **Event log, not snapshots.** Magic's model is reproduced: one linear log of reversible events, a step boundary closed by `post_command` only if the step recorded something, history limit `undo-history-limit` (setting, default 1000, values below 1000 rejected, Q-MODEL-21). Snapshots per command were rejected: they cannot give per-step selection/box restoration at Magic's granularity, and copying planes per command does not scale to full-chip edits.
- **Paint events at the tile level.** The recorder receives old type, new type, rectangle and plane for each tile changed by `paint`/`erase` (Magic's DBundo paint records), so undo replays the inverse without image resolution, as specified. Consecutive events of different cells interleave cell-switch events.
- **Suspension.** One nesting counter shared by `undo disable/enable`, `undo 0`/`redo 0` (magic-compat ON, D-11) and internal suspensions (file reads, flatten into a new cell, scratch cells). Replay records nothing and marks cells modified (Q-MODEL-12).
- **magic-compat OFF (D-10).** Property changes and `flatten -doinplace` are recorded (closes the M1 "undo for property changes" item). With it ON they are not.
- **Native events.** The `undo::Event` interface also carries Hefesto's native compound events (agent proposals, `sim` testbench edits); M2 provides the mechanism and its tests with a test event, later milestones add the producers.

### D6. Selection, highlights and connectivity

- **Selection storage.** The selection is an internal `db::CellDef` in root coordinates (Magic's `SelectDef`), never in the cell namespace listing nor saved, plus per-item provenance (source cell and use path) so edit-cell restriction and `what` work. It reuses the paint, label and use code. Its root is the top cell of the window it was made in. Every change records an undo event.
- **Highlights.** `view::HighlightLayer` keeps, per window, the screen areas to erase and the root areas to redraw, and runs the highlight clients in Magic's order (box, selection, feedback, wiring preview, measurement rulers, tile overlay) at the end of every command (`ui-feedback-and-highlights` "Highlight layer and clients"). The scene builder emits a highlight batch; the renderer draws the selection in white solid outlines over the layout (deferred from M1 task 5.5), feedback in its styles, and the box as before. The `*watch` tile-structure overlay forces a full redraw of its window after each change (the M1 task 5.6 "debug tile-overlay full redraw rule").
- **Connectivity.** `connect::search_flat` and `connect::search_hierarchical` derive nets on demand from the current geometry (`layout-connectivity`). The flat search reproduces Magic's cross-plane single-tile step with `magic-compat` ON and does the full search with it OFF (its Q resolution). The hierarchical search treats material under port labels of equal index in one cell definition as connected (closes the M1 "net tracing through equivalent ports" item). `select net`, `select short`, `goto` and net highlighting use them; later DEF and extraction tools reuse them.

### D7. Command dispatch, macros, tools and input

- **Window clients.** `ui::Client` tables (`*window`, `layout`, `color`; `netlist` and `wind3d` registered as optional modules that report unavailability until M12/M10) replace the flat map as the dispatcher's lookup: global table first, then the window's client, Magic's unique-prefix and exact-match rules, `help` per client and `help wizard`. Commands keep being registered by area; each registration names its client. Script, macro and CLI lines resolve exact names in `cmd::CommandRegistry` (Q-UI-41); Magic's unique-prefix, case-insensitive lookup is kept only for lines typed in the desktop command bar (`Origin::gui`, `Dispatcher::set_typed_abbreviations`).
- **One keyword matcher.** `cmd::keywords.hpp` (EDIT) provides Magic's unique-prefix lookup, direction and position names; the duplicates in `db/command_util` and `view/command_util` are removed.
- **Macros.** One macro table per tool for the `layout` client and per client otherwise. Key names follow X11 keysym names with `Meta_`/`Control_`/`Capslock_`/`Shift_` prefixes on every platform: Hefesto embeds a keysym name table (data from X11 `keysymdef.h`, MIT/X11 license, notice by INTEGRATION) and the shell maps Raven key codes to it. The default bindings of `ui-tools-and-input` "Default key bindings" are installed at startup as the system startup file would (no Tcl needed); a user key-binding file (Hefesto `macro` command lines) is a new reader with a libFuzzer target.
- **Tools.** `ui::ToolRegistry` with the `box`, `wiring`, `nettool` and `pick` tools, each its own macro table; `tool` cycles them. The `tool` command is owned by `ui-gui-managers` (M4) but is implemented now because the tools need it; its tests carry that capability's tag. Button events go through the same macro tables (`pushbutton` synthesizes them).
- **Input path.** Every pointer button, key and touch-bar action in the shell becomes a macro lookup and then commands through the Dispatcher with origin `key`/`gui`, after setting `cmd::PointerState`. Continuous camera gestures (pinch, scroll) stay direct and unlogged (M0 deviation D6); box drags, selection drags and move/copy previews issue commands on release and draw previews through highlights.
- **Command bar.** History with GNU Readline expansion rules for `history` (`ui-command-dispatch` "Command history"), and completion of command names, cell names and layer names from a core API (`ui::complete(prefix, context)`), so every front end (and M14) shares it (`hefesto-input-and-touch` "Command entry on every platform").
- **Script helpers as native commands.** `pushbox`/`popbox`/`peekbox`, `pushstack`/`popstack`/`clearstack`, `suspendall`/`resumeall`, `ruler`, `measure`/`unmeasure`, `genkey` and macro auto-increment are native core commands with Magic's names, arguments, results and errors (their SCRIPT resolutions: rulers and `measure` in correct units). In M4 the Tcl binding exposes them as `magic::` names without reimplementing them.

### D8. Desktop UI and the M2 UX design artifact

- **Design first.** Before SHELL builds a new screen or state, DESIGN adds a board to the reference design artifact (https://claude.ai/artifact/9fWfmyGAx1EYtzhmgqyeeK) and copies it into `docs/design/mockups/` (`hefesto-design-system` "Screen patterns follow the reference mockups"): editing tool states on the rail and canvas (box, select, paint, wire, label, measure; active-tool ember, cursor glyphs), selection highlight and inspector for paint/label/instance/mixed selections, active paint layer and lock toggles in the layer palette, command bar completion and history popover, undo/redo affordances, the color window, confirmation dialogs (crash recovery, flush, unsaved color map), macro/key-binding reference, feedback list, and the docked/undocked panel arrangement. This board set is "the M2 UX design artifact" of the roadmap's exit criterion.
- **Usability review.** An internal review against the boards and the running app with written findings (`docs/design/m2-ux-review.md`); the user approves it, as for M1 task 8.5.
- **Forge completion.** The SVG icon set of `hefesto-design-system` "Icons" (1.6 px stroke on a 24 px grid; box, select, paint, wire, label, measure, place device, 3D, agent, undo, zoom to fit, visibility, lock) as data under `assets/icons/`, drawn by Raven's SVG support (the M1 painted glyphs are replaced; a Raven gap is recorded and proposed upstream); dockable and collapsible panels; component patterns from `Main.dc.html` for the new controls.
- **Conformance record.** `docs/design/m2-ui-conformance.md`, same format as M1's.

### D9. Golden cases

- **Authoring.** New modules in `tools/golden/authoring/`: `cmdedit_cases.py` (the 80 CMDEDIT cases) and `ui_dispatch_cases.py` (UI command cases), registered in a new `m2_cases.py` driver modeled on `m1_cases.py` (`--list`, `--not-authored`). Cases of earlier areas that waited for M2 features are added to their existing modules (`model_cases.py`, `tech_cases.py`, `ui_cases.py`, `misc_cases.py`) and leave their `NOT_AUTHORED` lists. Cases using `tech: fine` get a small committed test technology (`U` = 10 nm, λ = 2) in their inputs.
- **Capture.** Expected outputs are captured only by `golden-capture.yml` (PR touching `case.toml`/`input/`, or `workflow_dispatch` mode `capture`) and committed by INTEGRATION.
- **Scope of "UI command cases".** The UI cases of `ui-command-dispatch`, `ui-tools-and-input`, `ui-feedback-and-highlights`, `ui-color-window` and `ui-wizard-and-debug` that Magic's Tcl build runs headless (`-dnull`), including button cases driven by `setpoint`/`pushbutton`. Not authored, with the reason in `NOT_AUTHORED`: the non-Tcl-build cases (UI-G-006, 007, 008, 020, 021, 022; the Tcl build is normative, D-7, and the Q-UI resolutions made their behavior informative), the X11-interactive and screenshot cases (UI-G-015, 047, 082; covered by unit tests and `pushbutton` cases unless the capture container gains a virtual display, see Open Questions), and UI-G-068 (needs extraction, M7). Also newly authorable: MODEL-G-040, 050–053, 056–059 (undo, select/what, move), TECH-G26 (`select net`), UI-G-033 and UI-G-036 (edit, per-window what/select); UI-G-035 (authored in M1) is captured. Still not authored: MODEL-G-055 (a Tcl proc, M4), MODEL-G-006/045/049/054 (DRC, LEF, extraction, DEF), MISC-FL-05 (GDS, M5; its `NOT_AUTHORED` reason wrongly says M2) and MISC-FL-06 (debug build); the `setlabel` per-label forms are covered by the CMDEDIT-LBL cases.

### D10. Items deferred from M1

| M1 item (source) | M2 home |
|---|---|
| Selection drawing in the renderer (m1 tasks 5.5) | VIEW, D6 |
| Tile-structure debug overlay full-redraw rule (m1 task 5.6) | VIEW, `ui-wizard-and-debug` "Tile structure overlay" |
| `setlabel` per-label forms and MISC-FL golden cases needing selection (m1 task 5.3) | HIER + golden (D9) |
| `specialopen color` (VIEW-CLIENT-001 until ui-color-window; m1 task 5.6) | VIEW; `netlist` keeps VIEW-CLIENT-001 until M12 |
| Undo for property changes with magic-compat OFF (D-10) | UNDO, D5 |
| Net tracing through equivalent ports | SELECT, D6 |
| 14 MODEL cases not authored (undo, select/what; m1 task 3.7) | golden, D9: 9 authored in M2 (G-040, 050–053, 056–059); G-055 (Tcl proc) and the DRC/LEF/extract/DEF ones (G-006, 045, 049, 054) stay later |
| Crash-recovery confirmation dialog (m1 task 7.6, conformance gaps) | SHELL via `cmd::PromptHost`, D4 |
| Problem count never resets (conformance gaps) | SHELL: reset on a clear action and per session document |
| Status bar shows only the first line of a multi-line result | SHELL: first line plus an expander to the Console |
| Minimap: outline-only and cross styles, labels and cell boxes not drawn; whole-scene rebuild on every publish | SHELL |
| Top ruler keeps the 11 px role (mockup: JetBrains Mono 10) | SHELL |
| Hierarchy toggle on overlapping siblings; `expand toggle` on a selection | SHELL + HIER (`expand selection`) |
| Stale `view/commands.hpp` comment about shell `view`/`zoom` | VIEW |
| Live input not exercised (Raven go/no-go criterion 2), VoiceOver/Orca not run (criterion 4) | SHELL + INTEGRATION, risk R1 |

## Area contracts

The headers are the contract; each is documented. Key types and functions per area (scaffold writes them with stubs):

### EDIT — `hefesto/cmd/{keywords,coordinates,units}.hpp`, `hefesto/db/`
- `keywords.hpp`: `lookup(word, table)` (unique prefix, exact wins), `Direction`, `parse_direction`, `parse_position`.
- `coordinates.hpp`: `parse_distance`/`parse_coordinate` with expressions (`+ - * /`, braces, magic-compat ON/OFF operand rules of Q-CMDEDIT-1/2), suffix chain, default unit; `format_value` (reported value formatting); `snap_point` (pointer snapping).
- `units.hpp`: `UnitsState`, `units` command; `snap`, `grid` complete.
- `db/paint_commands`: `paint`, `erase`, `fill`, `corner`, `contact`, `polygon` (shares the MASK nonzero-winding decomposer: written in `db` now, reused by `cif`/`gds` input in M5), `splitpaint`, `spliterase`, `drop`, `LayerList` parser with `* $ errors labels subcell connect`; `box` (every option, cursor forms, `-edit`); `xor`, `random`, `shell` (through the host `ProcessRunner`; iPadOS reports unavailable), `straighten` (argument checks, then "plowing is not available").

### UNDO — `hefesto/undo/`
- `UndoLog` (record, close_step, undo(n), redo(n), print, suspend/resume, limit), `Event` (undo/redo/describe), `DbRecorder : db::ChangeRecorder`, `undo`/`redo` commands.

### SELECT — `hefesto/select/`, `hefesto/connect/`
- `Selection` (root cell, internal def, provenance, floating offset), `select` (qualifiers, options, cycling), `what`, `identify`/`id`, `delete`, `copy`, `move` (incl. `move origin`), `stretch`, `clockwise`/`rotate`, `sideways`, `upsidedown`, `orient`, `array` (Q-CMDEDIT-63: list-of-lists and setters in native mode only).
- `connect`: `search_flat`, `search_hierarchical`, `NetVisitor`.

### HIER — `hefesto/cell/`, `hefesto/db/{label,labels,property}.hpp`
- `xload`, `edit`, `down`, full `getcell`/`get` and `dump` placement syntax, `expand`/`unexpand selection`, `flush`, `cellname`/`instance` options, naming rules; `editor::to_edit`/`to_root`.
- `setlabel` on the selection, `port make` numbering, `findlabel`, `goto`, `element`.

### DISPATCH — `hefesto/ui/`, `hefesto/cmd/{registry,dispatcher}.hpp`
- `Client`, `ClientTable`, prefix lookup, `help`, line splitting and quoting; `MacroTables`, `KeyName` (keysym table), `macro`/`imacro`, default bindings; `ToolRegistry`, `tool`, `cursor`; `logcommands`, `pushbutton`, `setpoint`, `sleep`, `echo`, `send`, `history`; `complete`; box/edit stacks, `suspendall`/`resumeall`, `ruler`, `measure`/`unmeasure`, `genkey`, auto-increment.

### VIEW — `hefesto/view/`, `hefesto/display/`, `hefesto/render/`
- `HighlightLayer`, `FeedbackList`, `feedback` command (save file with `i`-suffixed box coordinates, Q resolution), `ColorWindow` client (`color`, `load`, `save`, `pushbutton`, RGB/HSV, unsaved-map prompt), `TileOverlay` (`*watch`), highlight batches in `display::build_scene`/`render::to_batches`.

### WIRE — `hefesto/wiring/`
- `WiringSection` read from the raw `wiring` technology section (as VIEWER resolved the styles section in M1), `WireState`, `wire` and every subcommand, `wire segment` (file form reader with a libFuzzer target), wiring tool button actions.

### SHELL — `hefesto/shell/`
- Tool state on the rail, pointer and key input through macros, command bar history and completion popover, selection inspector, layer palette paint/visibility/lock, color window screen, `PromptHost` dialogs, icons, dockable panels, the D10 UI items.

### Scaffold result (tasks 1.2-1.5)

What the scaffold landed, so every workstream starts from the same files. Ownership follows D2; files the scaffold added that D2 does not name are assigned here.

- **Areas.** `undo select connect ui wiring` are in `HEFESTO_CORE_AREAS` and `tests/unit/CMakeLists.txt`, each with `sources.cmake`, `codes.cpp` (empty table), `diag_codes.hpp` and a globbed test directory with a `[m2-scaffold]` `scaffold_test.cpp`. Code areas `undo`, `select`, `connect`, `ui` and `wiring` (prefixes `UNDO`, `SELECT`, `CONNECT`, `UI`, `WIRE`); the wiring area's document is `docs/diagnostics/codes/wire.md` (`hefesto-cli --diagnostic-codes=wiring`).
- **Stubs.** `src/core/src/cmd/stub_commands.{hpp,cpp}` (INTEGRATION; deleted at M2 exit): `stubs::register_stubs`, `stubs::not_implemented` (`CORE-NOTIMPL-002`, "not implemented yet: <name>"). Stub registrations per owner: EDIT `cmd/units.cpp` (`units`) and `db/edit_stubs.cpp` (`fill corner contact polygon spliterase drop xor random shell straighten`, the informative `*extract *plow *psearch *showmem *showtech *tilestats *tsearch makesw sgraph`, and the `parse_layer_list`/`decompose_polygon` stubs); HIER `cell/hier_stubs.{hpp,cpp}` (`xload edit down get dump flush findlabel goto element`); SELECT `select/commands.cpp` (`select what identify id delete copy move stretch clockwise rotate sideways upsidedown orient`, plus `array`, moved from `db/commands_cells.cpp` with its M1 behavior); UNDO `undo/commands.cpp` (`undo redo`); DISPATCH `ui/commands.cpp` (`macro imacro tool cursor logcommands pushbutton setpoint sleep send history source *bypass pushbox popbox peekbox pushstack popstack clearstack suspendall resumeall ruler measure unmeasure genkey autoincr`); VIEW `view/m2_stubs.cpp` (`feedback *watch *coord *winddebug *files`); WIRE `wiring/commands.cpp` (`wire`). The `color` client's commands are registered by VIEW once DISPATCH's client tables exist (7.1).
- **Shared interfaces (INTEGRATION files).** `db/change_recorder.hpp` (`ChangeRecorder`, `NullChangeRecorder`, `TileChange`, `SplitChange`; `CellLibrary::set_recorder`/`recorder`, `CellDef::recorder`); the paint primitives report Manhattan tile changes (`db/paint.cpp` `record_tiles`), the other calls are listed in the header with their owner. `cmd/prompt.{hpp,cpp}` (`PromptHost`, `HeadlessPromptHost`, `cmd::ask`), `cmd/pointer.hpp` (`PointerState`), `editor/post_command.{hpp,cpp}` (counter, undo step, highlights, window update; client update handlers and the dispatcher hook are DISPATCH's, 7.2), `editor/edit_frame.{hpp,cpp}` (`to_edit`/`to_root`).
- **EditorState members.** As D4, with these names: `units`, `undo`, `selection`, `macros`, `tools`, `box_stack`, `edit_stack`, `suspend_depth`, `command_point` (`std::optional<cmd::PointerState>`), `command_count` (DISPATCH), `feedback`, `highlights`, `color_map` (`view::ColorMapState`), `tile_overlays` (VIEW), `wire`, `edit_transform` and `edit_root` (HIER), `prompt` (`cmd::PromptHost*`, INTEGRATION).
- **After the first round (integration, 2026-11-09).** Every stub was replaced: the stub files `db/edit_stubs.cpp`, `cell/hier_stubs.{hpp,cpp}`, `view/m2_stubs.cpp` and the helper `cmd/stub_commands.{hpp,cpp}` are deleted and `CORE-NOTIMPL-002` is retired (task 11.5). `EditorState` owns an `undo::DbRecorder` installed on `cells` (the dispatcher points it at the project's compat settings) and the ui session state (`ui`); `db::ChangeRecorder::cell_removed` and `db::ScopedRecorderDetach` keep cell rename/delete and `.mag` reads out of the history. `cmd::CommandResult::logged_as` carries a command's logged form (Magic's TxRebuildCommand) to the command log and observers. `editor::coordinate_units` (`editor/coordinate_units.hpp`) is the one builder of the session's coordinate units. The `help` command is the ui area's (the M0 built-in is removed).
- **After the second round (integration, 2026-11-10).** New shared interfaces: `db::Plane::record_splits` (split-tile fracture/join events for the recorder), `select::EditCopies` (`getcell`/`dump` selections, undoable and redrawn), `select::Selection::area_types` (the types of the last `select area`, Magic's `cd_types`), `cell::save_def_as`, `cell::load_def`, `cell::dereference_cell`, `view::WindowManager::translate` (`move origin`), `view::color_window_mapping`, `cmd::Dispatcher::target_client` (color-window `setpoint`), `editor::EditorState::style_dirs` (`tech load` reloads display styles) and `ui::snap_to_tool_grid` (pointer snapping shared by the core and the shell's `InputRouter`). The shell's canvas input goes through `shell::InputRouter` (`hefesto/shell/input_router.hpp`). Startup ends with an undo flush (Magic's `UndoFlush` after the command-line files).
- **Area headers (owned by the area, may be reshaped by it).** EDIT `cmd/{keywords,units}.hpp`, `cmd/coordinates.hpp` (`format_value`, `snap_point`), `db/{layer_list,polygon}.hpp`; UNDO `undo/{undo_log,db_recorder,commands}.hpp`; SELECT `select/{selection,commands}.hpp`, `connect/search.hpp`; DISPATCH `ui/{client,macros,tools,stacks,complete,commands}.hpp`; VIEW `view/{highlights,feedback,color_window,tile_overlay}.hpp`; WIRE `wiring/{wiring,commands}.hpp`.

## Risks / Trade-offs

- **R1. Raven go/no-go still provisional (M0 task 6.3).** M2 is the milestone that depends most on Raven: live mouse/keyboard input, SVG icons, dockable panels, message boxes, accessibility. Criteria 1 (reference M1 Mac and Linux GPU frame rate), 2 (live input) and 4 (screen readers) are open. → Mitigation: SHELL schedules the go/no-go checks as early M2 tasks (live input in a real window on macOS and Linux, VoiceOver and Orca on the palette and command bar), and INTEGRATION finalizes `docs/decisions/raven-go-no-go.md` and P-11 before the shell's large UI tasks start. All M2 behavior lives in the core and is reachable through commands, so a no-go replaces only the shell (the fallback order in the go/no-go record), and the golden and unit exit criteria do not depend on Raven. Raven gaps are proposed upstream, not patched locally.
- **R2. Undo recording cost on large edits.** Tile-level events for full-chip paint/erase can be large. → Recording is a null pointer when off; events are compact PODs in chunked storage; a benchmark (`hefesto-performance` interactive editing targets) runs paint/undo on the 100 × 100 `dfxtp_1` array; the history limit is in steps, not bytes, as specified.
- **R3. Interfaces touched by several workstreams** (`db` primitives with the recorder, `EditorState`, the dispatcher). → The scaffold publishes the recorder, prompt, pointer and post-command interfaces with stubs before the parallel phase; ownership of each file is single (D2); reshaping goes through open items naming callers.
- **R4. Golden expectations need CI capture**, and many CMDEDIT cases combine commands from several workstreams. → Workstreams develop against unit tests from the spec scenarios and Magic's source; INTEGRATION captures early (inputs authored per workstream as soon as commands exist) and replays at the end.
- **R5. Interactive behavior cannot be captured from Magic headless.** → Button behavior is captured through `setpoint`/`pushbutton` cases; the rest is specified behavior tested by unit tests on the core mapping and by shell tests with injected Raven events.
- **R6. Scope.** 19 capabilities, 186 requirements, ~120 golden cases. → Workstreams are independent after the scaffold; the iPad, DRC, Tcl and plowing parts are explicit non-goals; the exit check is the coverage script plus golden replay.
- **R7. X11 keysym table licensing.** → Data under the MIT/X11 license with a notice; no Xlib dependency.

## Migration Plan

No user data changes. The `units` settings-layer key becomes editor state (`cmd::UnitsState`); a project file that stored it keeps loading (the key is read once at startup and then ignored). `VIEW-CLIENT-001` stays for `netlist` and is no longer raised for `color`. `CORE-NOTIMPL-002` exists only during M2 and is retired at exit (kept in the code table as retired, never reused).

## Open Questions

None. The user decided on 2026-11-09:
- No virtual X display in the golden capture container: UI-G-015, 047 and 082 stay not authored, covered by unit tests and `pushbutton`-driven cases.
- The M2 boards are drafted locally under `docs/design/mockups/`; the user copies them into the claude.ai design artifact before the usability review (task 2.3, signed off by the user).
- The `tool` command ("Tool modes" in `ui-gui-managers`) is pulled forward into M2 scope, like the M1 design-system pull-forward; the rest of `ui-gui-managers` stays in M4.
