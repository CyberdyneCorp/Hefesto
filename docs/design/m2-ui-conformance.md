# M2 UI conformance

How the M2 desktop editor matches the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the M2 boards of the reference design artifact, drafted under [mockups/](mockups/) (`openspec/changes/m2-interactive-editing/design.md` D8). Same format and rules as [m1-ui-conformance.md](m1-ui-conformance.md), which stays the record for the M1 screens.

Every row names one board element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally).

Owner: SHELL (seeded by the M2 scaffold, task 1.5; completed in task 10.9). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-002` stub during M2).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1.

## How this was checked

Not yet checked: rows are filled as SHELL builds each screen (tasks 10.2-10.8), after DESIGN adds its board (task 2.1).

## Editing tools on the rail and canvas

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Tool rail: box, select, paint, wire, label, measure live; place device disabled until M4 | | todo | |
| Active tool in ember; cursor glyph per tool (`ui::ToolRegistry`) | | todo | |
| SVG icon set (1.6 px stroke, 24 px grid) under `assets/icons/` | | todo | |
| Box, selection and move/copy previews drawn through highlights | | todo | |

## Selection and inspector

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Selection highlight (white solid outlines over the layout) | | todo | |
| Inspector for paint, label, port, instance and mixed selections | | todo | |

## Layer palette

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Active paint layer (`paint`/`erase` on the box) | | todo | |
| Visibility and lock toggles (locked types) | | todo | |

## Command bar

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| History popover | | todo | |
| Tab completion of commands, cells and layers (`ui::complete`) | | todo | |
| Multi-line results: first line in the status bar, expander to the Console | | todo | |

## Undo, color window, feedback, dialogs, panels

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Undo/redo affordances | | todo | |
| Color window screen | | todo | |
| Feedback list | | todo | |
| Confirmation dialogs through `cmd::PromptHost` (crash recovery, `flush`, unsaved color map, `quit` with modified cells) | | todo | |
| Macro / key-binding reference | | todo | |
| Docked, undocked and collapsed panels | | todo | |

## M1 gaps closed in M2 (design.md D10)

| M1 gap | Implemented as | Status | Gap / why |
|---|---|---|---|
| Crash recovery confirmation dialog | | todo | |
| Problem count never resets | | todo | |
| Status bar shows only the first line of a multi-line result | | todo | |
| Minimap: outline-only and cross styles, labels and cell boxes; incremental rebuild | | todo | |
| Top ruler in JetBrains Mono 10 | | todo | |
| Hierarchy toggle on overlapping siblings | | todo | |

## Performance (hefesto-performance interactive editing targets)

UNDO records the paint/erase/undo benchmark on the 100 × 100 `sky130_fd_sc_hd__dfxtp_1` array here (task 4.6).

## Remaining gaps (open items)

None recorded yet.
