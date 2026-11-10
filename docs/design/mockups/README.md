# Hefesto UI mockups (design source)

Sources of the reference design artifact cited by capability `hefesto-design-system`:
https://claude.ai/artifact/9fWfmyGAx1EYtzhmgqyeeK ("Hefesto UI Mockups"). The artifact is the source of
truth; these copies let code review and agents check the implementation against it offline. Re-copy them
when the artifact changes (design changes go to the artifact first, per `hefesto-design-system`).

| Board | File | Screen |
|---|---|---|
| Design system — tokens & components | `Main.dc.html` | forge tokens, type scale, components |
| Desktop — layout editor | `Desktop-Layout.dc.html` | layer palette, hierarchy, canvas, inspector, command bar, status bar |
| Desktop — schematic + waveforms | `Schematic.dc.html` | M8 |
| Desktop — DRC results browser | `DRC-Browser.dc.html` | M3 |
| Desktop — 3D inspection | `View3D.dc.html` | M10 |
| Desktop — AI agent proposal review | `Agent-Review.dc.html` | M11 |
| iPad — layout editing | `iPad.dc.html` | M14 |
| M2 boards | `M2-*.dc.html` | see below |

## M2 boards (interactive editing)

Design D8 of `openspec/changes/m2-interactive-editing`. These boards were drafted here first (the user decided on
2026-11-09 to draft them locally); the user copies them into the reference design artifact before the M2 usability
review (task 2.3), after which the artifact is again the source of truth. `canvas.json` already places them on the
artifact canvas below the M1 boards.

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

The M2 boards use the forge tokens as CSS variables on the root element: set `data-theme="light"` on it for the
light theme. The layout canvas and everything drawn on it keep the dark tokens in both themes (as the M1 shell does).
Numbered purple pins are annotations, not UI; the "Notes for SHELL" under each board explain them. Each board says
which command every control issues.

Each `.dc.html` is self-contained HTML (inline styles; the M2 boards add one shared `<style>` block with the tokens):
colors, sizes and spacing can be read directly. PNG renders are in `docs/images/ui/`: the M2 boards are rendered at
1x in both themes (`<board>.png` dark, `<board>-light.png` light) with WebKit (macOS Quick Look), fonts from
`assets/fonts/`.
