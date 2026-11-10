# M2 usability review

Task 2.3 of `openspec/changes/m2-interactive-editing`, and the M2 exit criterion "An internal usability review of the M2 UX design artifact has been done" in `docs/roadmap.md`.

- **Date:** 2026-11-10
- **Reviewer:** the project owner, guided in a working session
- **Build:** `main` at 50bd4e4 (after PR #24), desktop app, macOS arm64 (Apple M3 Pro, MoltenVK)
- **PDK and cells:** sky130A; writable copies of `sky130_fd_sc_hd__dfxtp_1`, `inv_1` and `mux2_1` in a scratch folder
- **Boards:** the 11 M2 boards in [`mockups/`](mockups/README.md) (`M2-*.dc.html`), rendered in `docs/images/ui/m2-*.png`

## Tasks

| # | Task | How | Result |
|---|---|---|---|
| 1 | Paint and erase | palette layer, Paint tool, drag; Option+drag erases | ok |
| 2 | Select and move | Select tool, click paint, inspector, `move e 1` | ok |
| 3 | Undo and redo | top-bar Undo/Redo; right click on Undo for the history | ok |
| 4 | Label a port | Label tool on metal1, name, Enter; `port make` | ok |
| 5 | Draw a wire | Wire tool: click, move, click legs, right click to finish | ok |
| 6 | Bind a key | Keys tab (`panel keys show`), edit a row's `macro`, press the key | ok |
| 7 | Recover a crash | edit, kill the process, relaunch: recovery dialog | ok |
| 8 | VoiceOver | Cmd+F5; layer palette rows and the command bar read aloud | ok |

## Findings

None. The reviewer found the app and the boards in order ("seems all good") and approved the M2 design and the review.

Known gaps that were already recorded before the review, and that the review did not change, are listed in [`m2-ui-conformance.md`](m2-ui-conformance.md) and in `docs/roadmap.md` M4 "Carried from M2".

## Decisions made with the review

- The M2 boards stay in the repository: `docs/design/mockups/` is the source of truth for the reference mockups from now on (`hefesto-design-system`). They were not copied into the old claude.ai design artifact (task 2.1).
- VoiceOver on macOS passed. Linux X11/Wayland input and Orca remain M4 confirmation checks (task 10.1, Raven go/no-go).
