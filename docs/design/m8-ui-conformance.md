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
| "Netlist" button | not in the top bar yet: the action is the `n` binding and the `schematic netlist` command; the view model is `NetlistAction` | model |
| Simulate, Corner | the M1 "Simulate" stays disabled ("Simulation: arrives in M9"); no corner selector | later (M9) |

## Canvas

| Element | Implemented as | Status / gap |
|---|---|---|
| The active schematic drawn read-only: graphics, wires, placed symbols, texts with `@name`/`@symname`/`@<attribute>` | `build_scene` over the session's active document; `schematic_view.cpp` paints it fitted, wires in the info color, symbols in success, pins in danger, highlights in the accent | partial: the colors are placeholders until DESIGN fixes the schematic palette (the board uses the layer colors of xschem's dark scheme); text substitution is the display subset (attributes only, no `@pinlist`, no `tcleval`) |
| Pan and zoom | `SchematicViewport` (pan by pixels, zoom about the pointer, fit, zoom clamped to 1/64…64) | model: Raven's painted widget has no wheel or drag input here yet, so the view always fits |
| Selection by click | `schematic select instance <name>` for the instance under the pointer | blocked on EDIT's `select` option |
| Net highlighting drawn | wires whose `lab` (written by netlisting, Q-SCHEM-29) is in `schematic hilight list`, instances by name | model: the view does not read `hilight list` yet; colors per highlight (the cycle) are EDIT's |

## Symbol library browser

| Element | Implemented as | Status / gap |
|---|---|---|
| "Symbols · <library>" title, "Search symbols" field, rows | `SymbolLibraryBrowser` (folders = `schematic path list`, then the system library; case-insensitive filter) | model |
| Place a symbol | `schematic place <name> <x> <y>` (Insert binding opens the browser) | model; blocked on EDIT's `place` |

## Properties panel

| Element | Implemented as | Status / gap |
|---|---|---|
| Rows of the selected instance or wire (W, L, …) | `PropertiesPanel`: `schematic getprop instance\|wire ...`, rows from FORMAT's `parse_properties`, decoded values | model; `getprop` is CMD's (implemented) |
| Edit a value, remove an attribute | `schematic setprop instance\|wire <n> <key> [<value>]` | model; blocked on EDIT's `setprop` |

## Hierarchy breadcrumb

| Element | Implemented as | Status / gap |
|---|---|---|
| "top.sch › x1 › xinv[2]" above the canvas | `HierarchyBreadcrumb` from `schematic hierarchy path`; drawn by `schematic_view.cpp` (top only until the path is read) | partial: missing from the board (DESIGN) |
| Descend (`e`), ascend (`Ctrl+e`), click a crumb | `schematic descend <instance>`, one `schematic ascend` per level | model; blocked on EDIT's `descend`/`ascend` |

## Netlist action and messages

| Element | Implemented as | Status / gap |
|---|---|---|
| Netlist (`n`) with progress and Interrupt | `NetlistAction`: `schematic netlist [<file>] -hier\|-top\|-flat [-lvs]`, the Interrupt card "Netlisting inv.sch · 3 s" after `kLongRunning`, "Stopping: no file is written" | model: the core netlist run does not poll the script interrupt yet (open item for NETLIST/CMD), so Interrupt only marks the card |
| Messages | `MessagesPanel`: each command's diagnostics with severity and code, "2 errors · 1 warning", at most 1000 kept | model; missing from the board |

## Key bindings

| Element | Implemented as | Status / gap |
|---|---|---|
| xschem-style defaults: w wire, m move, c copy, R rotate, F flip, e descend, Ctrl+e ascend, q properties, k highlight, u undo, U redo, Insert place, n netlist | `schematic_default_bindings()` in the macro table `schematic`; Delete deletes the selection (Hefesto addition) | model: the schematic view does not take key focus yet |
| Rebinding through the key-binding sheet (M2-Key-Bindings) | the sheet's `macro schematic KEY "TEXT"` lines applied by `SchematicKeyMap::apply`; Reset table restores the defaults | model: the core `macro` command has no `schematic` client, so the lines are applied by the shell (open item for the core's ui clients) |
| Optional tool palette (Q-SCHEM-24) | not built | todo (DESIGN board first) |
