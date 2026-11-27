# M8 UI conformance

How the M8 desktop items match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the schematic board `docs/design/mockups/Schematic.dc.html` (`openspec/changes/m8-schematic/design.md` D15). M8 enables the Schematic tab of the top bar (disabled since M1, [m1-ui-conformance.md](m1-ui-conformance.md), top bar) and relabels the Waveforms tab "Waveforms: arrive in M9". Same format and rules as [m7-ui-conformance.md](m7-ui-conformance.md).

Every row names one element, how the shell implements it, and any gap with its reason. Where Raven cannot match the board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally). An element missing from the board is drawn by DESIGN first (M2–M7 rule) and the row says so.

Owner: SHELL (tasks 13.1, 13.2). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **model**: the headless view model and its command lines are built and unit-tested (`tests/unit/shell/schematic_*_test.cpp`); the Raven panel that shows it is not laid out yet.
- **later**: shown disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core option it issues is not implemented yet (EDIT and CMD land `schematic` options in parallel).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with the board, on macOS and Linux, as in M1–M7.

## How this was checked

Round 4 (branch `m8-round4-shell`, 2026-11-24). The shell drives the schematic editor only through `schematic ...` command lines sent through the window's command sink (`CoreCommandSink`), so every action is echoed, logged and replayable (`schematic-command` "Command logging"); the view models are Raven-free and tested headless on the command text they issue and on the results they parse:

- `schematic_canvas.hpp`: the read-only scene of a `schem::Document` (graphics, wires, each instance's symbol placed with FORMAT's orientation transform, `@key` texts), highlighted wires and instances, the viewport's pan, zoom about a point and fit; symbols through the session `SymbolCache`.
- `schematic_library.hpp`: the library browser (search path, then `<install root>/assets/schematic/xschem_library/devices`), the search filter, `schematic place`; the desktop install root (M8-D10).
- `schematic_panels.hpp`: properties (`getprop` / `setprop`), the hierarchy breadcrumb (`hierarchy path`, `descend`, `ascend`), net highlighting (`hilight ...`), messages, the netlist action with the M4/M5 Interrupt card.
- `schematic_bindings.hpp`: the xschem-style default bindings in the macro table `schematic`, rebindable through the key-binding sheet.

The Schematic tab itself is live: clicking it shows `schematic_view.cpp`, which draws the session's active document (the last opened) read-only and fitted, with the breadcrumb above it; a click on an instance issues `schematic select instance <name>`. No capture was taken in this round: the screenshots come from the CI desktop legs (macOS and Linux), so no row is **done** yet.

Round 5 (branch `m8-round5-shell`, 2026-11-24). The view is laid out in Raven as the board's three columns: the symbol library browser on the left (240 px, `surface1`), the breadcrumb, canvas, netlist bar and messages panel in the centre, the properties panel on the right (260 px, `surface1`). New view models, tested headless:

- `schematic_symbols.hpp`: symbols are resolved off the UI thread (round-4 open item). `SchematicSymbolResolver` hands a snapshot of the document's references (its folder and embedded blocks, the search path, xschem-compat) to one worker thread, which resolves them through a `SymbolCache` and a `LocalFileSystem` of its own (the session's cache, used by netlisting and the commands, is never touched from two threads). The canvas draws each instance whose symbol is not resolved yet with the placeholder in the tertiary text color, with "Resolving N symbols…" in the corner; the worker posts the result to the UI thread (`ui::post_task`), which merges it and repaints. A result is dropped when the document shown changed meanwhile (another document, its file or on-disk content, the search path, xschem-compat, `schematic reload_symbols`, a save), and a reference the document no longer uses is not merged. Closing the window cancels between references; a generator already running finishes first (at most 10 s, `kGeneratorTimeLimit`). Tests: `tests/unit/shell/schematic_symbols_test.cpp` (also run under ThreadSanitizer).
- `schematic_input.hpp`: the canvas input (a Raven viewport under the painted canvas takes the events, as the layout canvas does): the wheel and a pinch zoom about the pointer, a trackpad scroll, an Option-drag or a middle-drag pans (view only, no command), a left click selects the instance under the pointer, and keys run the `schematic` bindings once a click gave the canvas the key focus. `w`, `m` and `c` put `schematic wire `, `schematic move `, `schematic copy ` in the command bar (their coordinates are typed), `e` descends into the instance last clicked, `q` shows its properties, `Insert` focuses the symbol search, Ctrl-C interrupts. Tests: `tests/unit/shell/schematic_input_test.cpp`.

The view reads `schematic hilight list`, `schematic hierarchy path` and the properties' `getprop` with origin `internal` (not replayed, no command count) after any command changed the session, and observes the dispatcher for the results of `schematic` commands (the messages panel, the netlist run). The round-4 model read `hilight list` as a plain list of net names, while the command returns `{net <name> <colour>}` and `{instance <name> <colour>}` triples: fixed with a regression test (`schematic_panels_test.cpp`).

Round 6 (branch `m8-round6-shell`, 2026-11-26):

- The top bar has the board's "Netlist" button next to Simulate: it shows the Schematic tab and runs the same netlist action as the netlist bar and the `n` binding (`SchematicView::start_netlist`, the headless `run_netlist` and `netlist_button` of `schematic_panels.hpp`).
- The key bindings live in the editor's macro table `schematic`: the core `macro` command has a `schematic` client (`src/core/src/ui/macro_commands.cpp`), so the key-binding sheet's `macro schematic KEY "TEXT"` lines, a startup file and scripts edit the table the canvas reads (`SchematicKeyMap::attach` adds the xschem-style defaults for keys not yet bound). Before, the core silently ignored those lines (no such client).
- Copy and Paste link the schematic clipboard with the system clipboard (`schematic_clipboard.hpp`): Command-C / Command-V on macOS (Ctrl-C stays the interrupt), Ctrl-C / Ctrl-V elsewhere (Ctrl-C interrupts while a netlist runs, and copies otherwise). Copy runs `schematic clipboard copy` and puts the text on the system clipboard; Paste runs `schematic clipboard set <text>` and `schematic paste <x> <y>` at the pointer, snapped to 10. A system clipboard text above 16 MiB is refused with a message (crafted-input limit).
- The view models build without Raven and Vulkan (`src/shell/desktop/schematic_models`, target `hefesto_shell_schematic_models`, tests `hefesto_shell_schematic_tests` in builds with `HEFESTO_BUILD_DESKTOP=OFF`), so the linux-tsan CI job runs them under ThreadSanitizer.
- CI desktop legs: a "Capture the Schematic tab" step (the product app opens `tests/data/schem/devlib/tb/ihp_style_tb.sch` and an `--input` click on the Schematic tab; artifact `schematic-captures-<leg>`) and a `cmake --install` check of the install tree.

Round 6 capture review (2026-11-26). The first captures (integration branch `m8-round6`, CI run 45912671571, artifacts `schematic-captures-linux-x64-gcc-latest`, `-linux-x64-clang-latest`, `-macos-arm64-latest`) showed the Layout view, not the Schematic tab, although the step passed:

- the top bar's "Netlist" button was counted as always shown, which pushed the view tabs (the last group to give way, `top_bar_hidden_groups`) out of a 1280 px bar: no tab could be clicked. Fixed: "Netlist" gives way with the deferred actions (Simulate, Ask agent), before the view tabs, so the bar at 1280 px is the one of the M10 3D capture (tabs at x 252–548);
- the step clicked window point 455,24 (the 3D/Waveforms boundary), not the Schematic segment (x 320–405): it now clicks 362,24;
- nothing checked that the tab was shown. After an `--input` script the app now prints `hefesto: schematic tab <breadcrumb>, <n> instances, <k> pending` when the Schematic tab is shown (`SchematicView::status_line`), and the Linux step fails unless it reads `ihp_style_tb.sch` with at least one instance and no pending symbol;
- the macOS runner's screen is 1024 px wide (the capture is 1024 × 642), too narrow for the view tabs at any position: the macOS capture cannot show the Schematic tab until a second way to reach it exists (a Window-menu item or a key; open item), and that leg stays non-gating.

## Board review (DESIGN + SHELL, task 13.1)

Reviewed on 2026-11-24 against the M8 scope (D13, D15; `schematic-editing` "Desktop and touch interaction"; Q-SCHEM-24). The board predates the M8/M9 split: it draws one testbench session with simulation results. What it shows, and what M8 needs from it:

| Board part | M8 scope | Review finding |
|---|---|---|
| Header "tb_inverter.sch · sky130A · xschem-compatible · testbench" | M8 | Keep. The subtitle's "xschem-compatible" follows the `xschem-compat` setting ("xschem-compat off" when OFF), as the layout header shows magic-compat. |
| Tabs Layout · Schematic · 3D | M8 | Keep; the board has no Waveforms tab while the M1 top bar has one: the shell keeps it, disabled, "Waveforms: arrive in M9". |
| Corner selector (tt · 27 °C · 1.80 V / ss …) | M9 (`sim` corners) | Show disabled with "Corners arrive in M9". DESIGN item. |
| "Netlist" button | M8 | Keep: runs `schematic netlist` (the netlist action). |
| "▶ Simulate" | M9 (`schematic simulate` answers CORE-NOTIMPL until M9, D13) | Show disabled with "Simulation: arrives in M9" (the M1 "Simulate" action already says so). |
| "Symbols · sky130_fd_pr" with "Search symbols" and symbol rows | M8 | Keep: the library browser. |
| Canvas with instances, wires, labels, the `code_shown` block | M8 | Keep. |
| Waveforms pane (v(a), v(y), tpd, cursor readout) | M9 | Show disabled with "Waveforms arrive in M9", or drop from the M8 capture. DESIGN item. |
| Right panel "Net Y · 2 pins · probed in waveforms · linked to layout net Y" | M8 for the net and its pin count (net highlighting); M9 for "probed in waveforms" and "linked to layout net" (cross-probing, D13) | Keep the net header; the two M9 phrases become disabled chips naming M9. DESIGN item. |
| XM2 · W / XM2 · L fields | M8 | Keep: the properties panel (`setprop`). |
| "Operating point (back-annotated)" | M9 (D13: data model only in M8) | Show disabled with "Operating point: arrives in M9". DESIGN item. |
| "Run Monte Carlo (200 samples)…" | not in M8 or M9 plans | Show disabled with its milestone once planned (M9 decides). DESIGN item. |
| Hierarchy breadcrumb | M8 (`schematic-editing` "Hierarchy navigation", "Hierarchy path") | Missing from the board. DESIGN draws it above the canvas (the shell already shows "top.sch › x1 › xinv[2]" there). |
| Messages panel | M8 (netlist and command diagnostics) | Missing from the board. The shell reuses the M4 console's Diagnostics tab layout (bottom dock) until DESIGN draws it. |
| Netlist progress and Interrupt | M8 (M4/M5 patterns) | Missing from the board; reuses the M4-Script-Consent Interrupt card (tile 7) and the console's running header, as M5–M7 did. No new board needed. |
| Optional tool palette (Q-SCHEM-24) | M8 desktop: optional toolbar; iPad presentation M14 | Missing from the board. DESIGN draws the toolbar variant; until then it is a view-model item only. |

Board note: the board changes above are DESIGN items (open item for DESIGN: redraw `Schematic.dc.html` with the M9 parts disabled and the breadcrumb, messages and palette added, render it to `docs/images/ui/`); this table is the review record.

## Top bar

| Element | Implemented as | Status / gap |
|---|---|---|
| Schematic tab enabled (no longer "arrives in M8") | `deferred_view_tabs()` tooltip "Schematic editor: xschem-compatible schematics (M8)"; the segmented control's enabled mask includes it; a click shows the schematic view instead of the canvas (`MainWindow::show_schematic_tab`) | partial: needs the CI capture |
| Waveforms tab disabled, "Waveforms: arrive in M9" | `deferred_view_tabs()`; the tab list's accessible name and tooltip name M9 | partial: needs the CI capture |
| "Netlist" button | the top bar's "Netlist" (secondary, before Simulate, as the board) shows the Schematic tab and runs `NetlistAction` through `run_netlist`, as the netlist bar under the canvas ("Netlist", primary) and the `n` binding do | partial: needs the CI capture (the capture shows the Schematic tab, not a netlist run) |
| Simulate, Corner | the M1 "Simulate" stays disabled ("Simulation: arrives in M9"); the netlist bar shows "Simulate · arrives in M9" in tertiary text; no corner selector | later (M9) |

## Canvas

| Element | Implemented as | Status / gap |
|---|---|---|
| The active schematic drawn read-only: graphics, wires, placed symbols, texts with `@name`/`@symname`/`@<attribute>` | `build_scene` over the session's active document; `schematic_view.cpp` paints it, wires in the info color, symbols in success, pins in danger | partial: the colors are placeholders until DESIGN fixes the schematic palette (the board uses the layer colors of xschem's dark scheme); text substitution is the display subset (attributes only, no `@pinlist`, no `tcleval`); needs the CI capture |
| Symbols resolved off the UI thread | `SchematicSymbolResolver` (worker thread, own cache); pending instances drawn with the placeholder in tertiary text and "Resolving N symbols…" until the worker's result is posted back | partial: needs the CI capture; no board element (the placeholder look is a DESIGN item with the schematic palette) |
| Pan and zoom | `SchematicCanvasInput` over `SchematicViewport`: wheel and pinch zoom about the pointer, trackpad scroll, Option-drag and middle-drag pan; the view fits the document until the user pans or zooms, and again for another document | partial: needs the CI capture |
| Selection by click | `schematic select instance <name>` for the instance under the pointer; the properties panel then shows that instance | partial: needs the CI capture (EDIT's `select` landed in round 4) |
| Net highlighting drawn | the view reads `schematic hilight list` after each command; wires whose `lab` (written by netlisting, Q-SCHEM-29) is a highlighted net and highlighted instances are drawn in their highlight colour (the 8-colour cycle mapped to forge tokens), highlighted wires twice as thick (state is not color only) | partial: a wire shows its net's highlight only once a netlist run wrote its `lab`; needs the CI capture |

## Symbol library browser

| Element | Implemented as | Status / gap |
|---|---|---|
| "Symbols · <library>" title, "Search symbols" field, rows | the left panel: section title, the search field, a scrolled list of `.sym` rows with their folder; `SymbolLibraryBrowser` (folders = the session's search path, the document folder for `.`, then the system library; case-insensitive filter) | partial: needs the CI capture; no symbol preview yet (the board shows names only) |
| Place a symbol | a click on a row issues `schematic place <name> <x> <y>` at the view's centre snapped to 10; `Insert` focuses the search field | partial: no drag-to-place yet; needs the CI capture |

## Properties panel

| Element | Implemented as | Status / gap |
|---|---|---|
| Rows of the selected instance or wire (W, L, …) | the right panel "Properties · XM2": key and value rows of the instance last clicked (`q` too), read with `schematic getprop instance <name>` (origin internal) after each command | partial: needs the CI capture; wires are not picked yet (instances only) |
| Edit a value, remove an attribute | a click on a row puts `schematic setprop instance <n> <key> <value>` in the command bar to edit | partial: no in-place field yet |
| "Operating point (back-annotated)" | a tertiary "Operating point · arrives in M9" line at the panel's foot | later (M9) |

## Hierarchy breadcrumb

| Element | Implemented as | Status / gap |
|---|---|---|
| "top.sch › x1 › xinv[2]" above the canvas | `HierarchyBreadcrumb` from `schematic hierarchy path` (read after each command); parents in `accent_soft`, the current level in primary text | partial: missing from the board (DESIGN) |
| Descend (`e`), ascend (`Ctrl+e`), click a crumb | `schematic descend <instance last clicked>`, `schematic ascend`; a click on a crumb issues one `schematic ascend` per level below it | partial: needs the CI capture |

## Netlist action and messages

| Element | Implemented as | Status / gap |
|---|---|---|
| Netlist (`n`) with progress and Interrupt | the netlist bar: "Netlist" runs `NetlistAction` (`schematic netlist -hier`, the text returned); after `kLongRunning` the bar shows the Interrupt card "Netlisting inv.sch · 3 s" with "Interrupt ⌃C" (the frames the script progress pumps repaint it); Interrupt or Ctrl-C on the canvas sends the script interrupt once (`Interpreter::request_interrupt`), the button then reads "Stopping…" and the detail "Stopping: no file is written"; the result line "● Netlist ready (12 lines)" / "■ Netlist failed: …" | partial: the netlist run polls the interrupt in the CMD workstream's round-5 change; needs the CI capture |
| Messages | the bottom panel (120 px, `surface0`): "MESSAGES · 2 errors · 1 warning", the newest diagnostics of every `schematic` command and of symbol resolution, each with its shape (■ ▲ ●), code and text; a click clears it | partial: missing from the board (DESIGN); needs the CI capture |

## Key bindings

| Element | Implemented as | Status / gap |
|---|---|---|
| xschem-style defaults: w wire, m move, c copy, R rotate, F flip, e descend, Ctrl+e ascend, q properties, k highlight, u undo, U redo, Insert place, n netlist | `schematic_default_bindings()` in the macro table `schematic`, run by `SchematicCanvasInput` once a click gave the canvas the key focus; Delete deletes the selection (Hefesto addition) | partial: F6 and the tab switch focus the schematic canvas (round-5 integration); needs the CI capture |
| Rebinding through the key-binding sheet (M2-Key-Bindings) | the sheet's `macro schematic KEY "TEXT"` lines run through the core `macro` command, whose `schematic` client edits the editor's table the canvas reads (`SchematicKeyMap::attach`); `SchematicKeyMap::reset_commands` gives the lines that restore the defaults | partial: the sheet's Reset table button uses `reset_table_commands` (the core's startup bindings), which has no schematic defaults: for the `schematic` table it should issue `SchematicKeyMap::reset_commands` (open item for CARRY, `key_bindings_view.cpp`) |
| Copy and Paste with the system clipboard | Command-C / Command-V (macOS) or Ctrl-C / Ctrl-V on the canvas: `schematic clipboard copy` and its text to the system clipboard; the system clipboard to `schematic clipboard set <text>` and `schematic paste <x> <y>` at the pointer (`schematic_clipboard.hpp`, Raven's clipboard) | partial: no board element (an edit action); off macOS Ctrl-C copies unless a netlist runs (proposed Q-SCHEM-66) |
| Optional tool palette (Q-SCHEM-24) | not built | later: deferred until DESIGN redraws `Schematic.dc.html` with the palette (round 6 keeps it out) |

## Packaging

| Element | Implemented as | Status / gap |
|---|---|---|
| Asset folder of the desktop app | `load_assets` finds the fonts and icons with `desktop_asset_dir` from the folder the operating system reports for the executable (`platform_posix::executable_dir()`), never argv[0]; the `--asset-dir` override and the source-tree fallback stay | partial: regression test in `schematic_library_test.cpp`; the CI desktop captures load the bundled fonts through it |
| Install tree | `src/shell/desktop/CMakeLists.txt` installs `hefesto` in `<prefix>/bin`, the fonts and icons in `<prefix>/share/hefesto/assets` and the schematic system library in `<prefix>/assets/schematic` (the install root of `<prefix>/bin`, M8-D10) | partial: the CI desktop legs run `cmake --install` of the real desktop build and check `bin/hefesto`, `bin/hefesto-cli`, the fonts and icons and `assets/schematic` ("Check the desktop install tree", round 6) |
