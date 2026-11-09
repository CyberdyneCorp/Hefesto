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

Each `.dc.html` is self-contained HTML with inline styles: colors, sizes and spacing can be read directly.
PNG renders are in `docs/images/ui/`.
