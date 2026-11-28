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

Round 6 (task 16.1, branch `m7-round6-core`, 2026-11-27): the button is live (`extract_button.hpp` view model, `m7_window.cpp` wiring; `tests/unit/shell/m7_extract_button_test.cpp`), and `extract` / `extract all` run through `MaskSession::write_host` when the desktop sets it (`tests/unit/extract/command/host_test.cpp`: the host runs it, its Interrupt keeps the previous `.ext`). The CI step "Capture the Extract button" (ci.yml, desktop legs) runs the product app at 1920 × 1000 in a directory holding EX-026's `exttest.tech`; after its `--input` script the app prints `hefesto: extract button enabled|disabled: <tooltip>` and the button's place. Captures: CI run 46045080575 and CI run 46051112560, artifact `extract-captures-linux-x64-gcc-latest` (Linux x64 GCC under Xvfb; the macOS leg is not gating). At 1280 px the deferred actions (Netlist, Extract, Simulate, Ask agent) give way first, as in M1, so the captures use 1920 px.

Round 7 (task 16.1, branch `m7-round7-decide`, 2026-11-28): `--capture-running` arms only once the `--input` script has started (`capture_running_armed`, `input_script.hpp`; `tests/unit/shell/m7_capture_running_test.cpp`), so the Interrupt card of the `--run` script's `load big` is no longer the frame taken; the capture layout grew from a 400 × 400 to a 900 × 900 nfet grid so that `extract all` runs past the 2 s card delay, and the cancel comes 2.5 s after the click. The dead M1 tooltip `Extraction: arrives in M7` is gone from `deferred_actions()` (the live button takes its tooltip from `extract_button_view`). Captures: CI run 46155373928, artifact `extract-captures-linux-x64-gcc-latest`.

Missing from every board: nothing. The button is the M1 top-bar button, and the progress and cancel reuse the M5 rows (the running header of `M4-Console` and the Interrupt card of `M4-Script-Consent` tile 7).

## Top bar (M1 board, top bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| "Extract" button enabled when the technology has an extract style (`EditorState::extract.current` not null) | the M1 slot, painted by `add_extract_button` from `extract_button_view` (style, active window root, no job running); tooltip `Extract <root>: extract all (style <style>)` | done (Linux): `extract-enabled` of CI run 46045080575 shows Extract enabled (secondary, full opacity) between Netlist and Simulate with exttest and `ex` loaded; the app printed `hefesto: extract button enabled: Extract ex: extract all (style exttest)` |
| "Extract" button disabled, with a tooltip, when the technology has no extract style | the M1 disabled state (45 % opacity); tooltip `Extract: technology <tech> has no extract style (no extract section); load a technology with one` (also disabled, with its reason, without a root or while a job runs; with no technology loaded the tooltip is `Extract: no technology loaded; load a technology with an extract section`) | done (Linux): `extract-disabled` of CI run 46045080575 (built-in `minimum` technology, pointer resting on the button) shows the disabled button and that tooltip |
| Clicking "Extract" runs `extract all` on the root of the active window, echoed and logged as a typed command | the click runs `extract all` through `run_typed_line` (Console echo, history, session log); a click on the disabled button does nothing | done (Linux): `extract-cancelled` of CI run 46051112560 (an `--input` click on the button) shows the console with `Extracting big into big.ext:` and `Processing big`, the status `● ok`, and the button focused; `big.ext` was written |
| The button is hidden at narrow widths in the M1 order (`top_bar_hidden_groups`) | unchanged: the button stays in the deferred-actions group (`top_groups_[0]`), so it gives way first | partial: the 1920 px captures show it; no narrow-width capture yet (the M8 round-6 1280 px capture shows the whole deferred group hidden, the same group) |

## Progress and cancel (M5 hosts)

| Element | Implemented as | Status / gap |
|---|---|---|
| A long `extract all` shows the console's running header and, after 2 s, the canvas Interrupt card | `extract all` runs through `MaskSession::write_host` (the M5 `MaskJobHost::run_job`), whose card reads "Writing <root>.ext · N s"; `extract::run` reports through `mask::Progress` at cell boundaries and checks cancel in the per-tile loops | done (Linux): `extract-running` of CI run 46155373928 (900 × 900 nfet grid, `--capture-running` armed after the `--input` script started) shows the extraction's own card over the dimmed canvas: `running 13.4 s`, `% extract all`, the orange `Interrupt ^C` button, the Extract button focused; the run then completed and wrote `big.ext` (the whole step took 263 s, the load included). The card names the command (`% extract all`), as the M5 script card does, not "Writing <root>.ext". Host route also unit-tested (`host_test.cpp`) |
| Cancel leaves every previous `.ext` file unchanged and no temporary file (Q-EXTRACT-27) | the card's Interrupt (or Ctrl-C) sets the job's cancel flag; `extract::AtomicOutput` (D12) abandons the open file | partial: unit-tested through the host (`host_test.cpp` "an Interrupt from the host keeps the previous .ext"; `run_test.cpp`); not captured: in CI run 46155373928 the run lasted well past the Ctrl-C injected 2.5 s after the click, yet `big.ext` was rewritten (`timestamp 100`) and the capture shows the finished run (`● ok`). The `--input` player only plays its steps from `on_frame`, which does not run while the script blocks in `pump_script_frame`, so the scripted Ctrl-C reaches the window only after the run ended (it was 3 s in CI run 46051112560 for the same reason). A cancelled-run capture needs `pump_script_frame` to play the due `--input` steps (a harness change in `m4_managers_window.cpp`, outside M7's files) |
| Errors and warnings go to the console and to feedback, as for DRC | the command's diagnostics reach the Console and status line as any typed command; `finish_run` adds the feedback areas | partial: `extract-cancelled` of CI run 46051112560 shows the run's informative lines (EXTRACT-CMD-002, EXTRACT-NODE-002) and the EXTRACT-STYLE-018 warning in the Console's Diagnostics tab; no run with feedback captured |

## Console-only commands

| Element | Implemented as | Status / gap |
|---|---|---|
| `ext2spice`, `ext2sim`, `extresist`, `antennacheck` reachable from the console, no menu item (Magic's wrapper has none) | registered in the layout client by the scaffold | partial: registered (R1); their run paths answer `CORE-NOTIMPL-008` until SPICE, SIM, RESCMD and ANTENNA land them |
