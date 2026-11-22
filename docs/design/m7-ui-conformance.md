# M7 UI conformance

How the M7 desktop items match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the approved boards they reuse (`openspec/changes/m7-extraction/design.md` D19). M7 adds no board, no menu item and no new screen (Open Question 7, decided (a) on 2026-11-21): the top bar's "Extract" button, painted and disabled since M1 ([m1-ui-conformance.md](m1-ui-conformance.md), top bar), runs `extract all` on the root of the active window with the M5 progress panel and cancel, and the console reaches `ext2spice`, `ext2sim`, `extresist` and `antennacheck`. Same format and rules as [m6-ui-conformance.md](m6-ui-conformance.md), which stays the record for the M6 items.

Every row names one element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally). If SHELL finds an element missing from every board, DESIGN draws it first (M2–M6 rule) and the row says so.

Owner: SHELL (seeded by the M7 scaffold, task 3.6; completed in the SHELL tasks of R4). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-008` stub during M7).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1–M6.

## How this was checked

The scaffold (R1) registered the five commands with their option tables (`tests/unit/core/m7_scaffold_test.cpp`); every run path answers `CORE-NOTIMPL-008` until EXTCMD, SPICE, SIM, RESCMD and ANTENNA land it. No shell code changed in R1, so every row is **todo**.

Missing from every board: nothing. The button is the M1 top-bar button, and the progress and cancel reuse the M5 rows (the running header of `M4-Console` and the Interrupt card of `M4-Script-Consent` tile 7).

## Top bar (M1 board, top bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| "Extract" button enabled when the technology has an extract style (`EditorState::extract.current` not null) | the M1 painted button; its enabled state follows the session style | todo (SHELL, R4): the style is null until EXTTECH lands the builder (task 4.6) |
| "Extract" button disabled, with a tooltip, when the technology has no extract style | the M1 disabled state (45 % opacity); the tooltip names the missing style instead of M7 | todo (SHELL, R4) |
| Clicking "Extract" runs `extract all` on the root of the active window, echoed and logged as a typed command | the main window runs the line through the Dispatcher | todo (SHELL, R4); blocked on EXTCMD's run driver (`extract::run`) |
| The button is hidden at narrow widths in the M1 order (`top_bar_hidden_groups`) | unchanged | todo: check in the R4 capture that enabling it does not change the order |

## Progress and cancel (M5 hosts)

| Element | Implemented as | Status / gap |
|---|---|---|
| A long `extract all` shows the console's running header and, after 2 s, the canvas Interrupt card | `extract::run` reports through `mask::Progress` (D5 "Commands and progress"), tested at cell boundaries and in the per-tile loops Magic guards with SigInterruptPending | todo (SHELL, R4); blocked on EXTCMD |
| Cancel leaves every previous `.ext` file unchanged and no temporary file (Q-EXTRACT-27) | `extract::AtomicOutput` (D12) | todo; blocked on EXTNODE and EXTCMD |
| Errors and warnings go to the console and to feedback, as for DRC | the M3 feedback path | todo (SHELL, R4) |

## Console-only commands

| Element | Implemented as | Status / gap |
|---|---|---|
| `ext2spice`, `ext2sim`, `extresist`, `antennacheck` reachable from the console, no menu item (Magic's wrapper has none) | registered in the layout client by the scaffold | partial: registered (R1); their run paths answer `CORE-NOTIMPL-008` until SPICE, SIM, RESCMD and ANTENNA land them |
