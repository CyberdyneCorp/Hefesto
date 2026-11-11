# Hefesto UI mockups (design source)

The reference mockups cited by capability `hefesto-design-system`. Since 2026-11-10 these files are the source of
truth (user decision, M2 task 2.1): design changes are made here first, with renders in `docs/images/ui/`. The M1
boards were first drawn in the design artifact https://claude.ai/artifact/9fWfmyGAx1EYtzhmgqyeeK ("Hefesto UI
Mockups"), which is no longer updated.

| Board | File | Screen |
|---|---|---|
| Design system — tokens & components | `Main.dc.html` | forge tokens, type scale, components |
| Desktop — layout editor | `Desktop-Layout.dc.html` | layer palette, hierarchy, canvas, inspector, command bar, status bar |
| Desktop — schematic + waveforms | `Schematic.dc.html` | M8 |
| Desktop — DRC results browser | `DRC-Browser.dc.html` | M3 (redrawn with the M3 states; see below) |
| Desktop — 3D inspection | `View3D.dc.html` | M10 |
| Desktop — AI agent proposal review | `Agent-Review.dc.html` | M11 |
| iPad — layout editing | `iPad.dc.html` | M14 |
| M2 boards | `M2-*.dc.html` | see below |
| M3 boards | `DRC-Browser.dc.html`, `M3-*.dc.html` | see below |
| M4 boards | `M4-*.dc.html` | see below |

## M2 boards (interactive editing)

Design D8 of `openspec/changes/m2-interactive-editing`. These boards were drafted here (user decision 2026-11-09) and
approved by the user on 2026-11-10 together with the M2 usability review (task 2.3); they were not copied into the
old design artifact. `canvas.json` places them below the M1 boards.

| Board | File | Screen states | Renders (`docs/images/ui/`) |
|---|---|---|---|
| Editing tools | `M2-Tool-States.dc.html` | tool rail states, canvas feedback of box / select / paint / wire / label / measure, cursor glyphs | `m2-tool-states.png`, `-light` |
| Selection and inspector | `M2-Selection.dc.html` | white selection highlight; inspector for paint, label, port, instance, mixed | `m2-selection.png`, `-light` |
| Layer palette | `M2-Layer-Palette.dc.html` | hover toggles, active paint layer with Paint/Erase on box, locked and hidden rows, row menu | `m2-layer-palette.png`, `-light` |
| Command bar | `M2-Command-Bar.dc.html` | Tab completion (layers, commands, cells), history popover, multi-line result with Console expander | `m2-command-bar.png`, `-light` |
| Undo and redo | `M2-Undo-Redo.dc.html` | top-bar buttons and tooltip, enabled/disabled/recording-off states, history popover, status after undo | `m2-undo-redo.png`, `-light` |
| Prompt dialogs | `M2-Dialogs.dc.html` | crash recovery (Hefesto and Magic backups), `flush`, unsaved color map, quit with modified cells, `writeall` | `m2-dialogs.png`, `-light` |
| Color window | `M2-Color-Window.dc.html` | `specialopen color` window: Magic surface, caption, index stepper, Load/Save, value strip | `m2-color-window.png`, `-light` |
| Feedback list | `M2-Feedback.dc.html` | feedback areas on the canvas, Feedback tab with find / clear / save | `m2-feedback.png`, `-light` |
| Wiring tool | `M2-Wiring.dc.html` | wiring tool tracking: preview leg, box, HUD, mode chip, wire inspector | `m2-wiring.png`, `-light` |
| Dockable panels | `M2-Panels.dc.html` | docked, collapsed, floating panel, drag with drop zone, edge strip | `m2-panels.png`, `-light` |
| Key bindings | `M2-Key-Bindings.dc.html` | macro tables by tool, key capture, conflict warning | `m2-key-bindings.png`, `-light` |

## M3 boards (design rule checking)

Design D11 of `openspec/changes/m3-drc`. Drafted here in the M2 board format (shared token `<style>` block, numbered
annotation pins, "Notes for SHELL"); waiting for the user's review (task 2.1). `DRC-Browser.dc.html` replaces the M1
drawing of the same board: its later-milestone parts (measured versus required, nets, suggested fix, Fix with agent,
sign-off) are kept as disabled rows naming M7, M11 or M13. `canvas.json` places both boards below the M2 boards. The
M1 render `drc-browser.png` shows the old drawing.

| Board | File | Screen states | Renders (`docs/images/ui/`) |
|---|---|---|---|
| DRC results browser | `DRC-Browser.dc.html` | docked DRC panel populated (errors grouped by why-text with counts, rectangles, ignored rules, style selector, Re-check), `#k of n` stepping with Prev/Next, error tiles and box on the canvas, inspector for the error area with ignore; empty (not checked, clean), running and stale panel states; style menu | `m3-drc-browser.png`, `-light` |
| DRC status | `M3-DRC-Status.dc.html` | top-bar chip and status-bar segment in the five states (not run, checking, clean, N errors, off), chip tooltip and DRC menu, `drc why` popover on the canvas, job-completion notification | `m3-drc-status.png`, `-light` |

## M4 boards (scripting and PDK integration)

Design D13 of `openspec/changes/m4-scripting`. Drafted here in the M2/M3 board format (shared token `<style>` block,
numbered annotation pins, "Notes for SHELL"). The user approved them on 2026-11-11 (task 3.1); SHELL builds
each screen from its board. `canvas.json` places them below the M3 boards. Diagnostic codes shown as
`SCRIPT-PDK-…` and `SCRIPT-SANDBOX-…` stand for codes that PDKINT and TCL have not assigned yet. Generated-cell
suffixes are illustrative.

| Board | File | Screen states | Renders (`docs/images/ui/`) |
|---|---|---|---|
| Menu bar | `M4-Menu-Bar.dc.html` | in-window menu bar (Linux) and native menu bar (macOS) with File/Edit/Cell/Window/Layers/Drc/Options; the sky130 Devices 1 menu open, with the hovered item's command in the status bar; the contents of every menu, with accelerators; the PDK-inserted `Import SPICE` and `DRC Routing` items; later-milestone items disabled; the skipped-statement report in Diagnostics | `m4-menu-bar.png`, `-light` |
| Place device and PCell dialog | `M4-PCell-Dialog.dc.html` | Place device popover on the tool rail; parameter dialog for a new device (native generator), editing an instance, an invalid value corrected by `_check`, a dependency recomputed (Tcl generator); the error when no dialog opens | `m4-pcell-dialog.png`, `-light` |
| Script consent | `M4-Script-Consent.dc.html` | consent prompt with the script origin and PDK version (store release and auto-discovered variant); trusted PDK versions in the PDK panel; declined read-only state with `minimum`; sandbox-denial diagnostics; a long-running script with Interrupt, and the interrupted result | `m4-script-consent.png`, `-light` |
| Managers | `M4-Managers.dc.html` | cell manager (Cells tab), library manager (Libraries tab), technology manager, text helper, layer toolbar order, reorder dialog | `m4-managers.png`, `-light` |
| Tcl console | `M4-Console.dc.html` | Console tab as a Tcl console: transcript with input, continuation lines, stdout, results, stderr and errors; multi-line input with an incomplete command; running with Interrupt; interrupted; window command entry | `m4-console.png`, `-light` |

The M2, M3 and M4 boards use the forge tokens as CSS variables on the root element: set `data-theme="light"` on it for the
light theme. The layout canvas and everything drawn on it keep the dark tokens in both themes (as the M1 shell does).
Numbered purple pins are annotations, not UI; the "Notes for SHELL" under each board explain them. Each board says
which command every control issues.

Each `.dc.html` is self-contained HTML (inline styles; the M2 boards add one shared `<style>` block with the tokens):
colors, sizes and spacing can be read directly. PNG renders are in `docs/images/ui/`: the M2, M3 and M4 boards are rendered at
1x in both themes (`<board>.png` dark, `<board>-light.png` light) with WebKit (macOS Quick Look for M2; a 1440 px
WKWebView snapshot for M3 and M4, the light one with `data-theme="light"` on the root), fonts from `assets/fonts/`.
