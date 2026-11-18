# M6 UI conformance

How the M6 desktop items match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the approved boards they reuse (`openspec/changes/m6-lef-def-oasis/design.md` D16). M6 adds no menu item, no new screen and no new board (Open Question 5, decided (a) on 2026-11-18): Magic's wrapper has no LEF, DEF or OASIS item (wrapper.tcl:1235-1239), and the console reaches the `lef`, `def` and `oasis` commands. The M6 items are OASIS recognition on the existing open paths and the progress and cancel of LEF, DEF and OASIS reads and writes through the M5 hosts. Same format and rules as [m5-ui-conformance.md](m5-ui-conformance.md), which stays the record for the M5 items.

Every row names one element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally). If SHELL finds an element missing from every board, DESIGN draws it first (M2–M5 rule) and the row says so.

Owner: SHELL (seeded by the M6 scaffold, task 2.6; completed in tasks 14.1–14.3). The "Performance" section belongs to PERF (tasks.md section 16). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-006` stub during M6).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1–M5.

## How this was checked

Not checked yet: the scaffold seeded the rows (round 1). The core functions are `CORE-NOTIMPL-006` stubs (`oasis::read`, `oasis::write`, `lef::read`, `lef::write`, `def::read`, `def::write`), so every row is **todo**.

Missing from every board: nothing expected. The open paths are the M4 picker and drag and drop, and the progress of a read or write reuses the M5 rows (the running header of `M4-Console` and the Interrupt card of `M4-Script-Consent` tile 7).

## Menu bar (M4-Menu-Bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| No LEF, DEF or OASIS menu item (Open Question 5 (a); Magic's wrapper has none) | the M5 wrapper items unchanged | todo: task 14.3 verifies that the bar gains no item and that no board is added |

## Format recognition on open (`oasis-command` "Format recognition on open", D15)

| Element | Implemented as | Status / gap |
|---|---|---|
| File ▸ Open (the native picker) of an OASIS file runs `oasis read` with the current settings, whatever the extension | the generic open path in `app/` sniffs the first 13 bytes, after gzip, for `%SEMI-OASIS\r\n` (`oasis::sniff_format`, OASCMD) | todo: tasks 13.x (OASCMD) and 14.1 (SHELL) |
| Drag and drop of an OASIS file onto the window | the same open path | todo: task 14.1 |
| A file that is not OASIS opens as before (`.mag`, GDS) | the same open path | todo: task 14.1 |

## Progress and cancel (M5 hosts: `MaskSession::read_host`, `write_host`, `emit_host`)

| Element | Implemented as | Status / gap |
|---|---|---|
| A long `oasis read` shows the console's running header and, after 2 s, the canvas Interrupt card | `MaskSession::read_host` with `oasis::read`'s `mask::Progress` reports | blocked: `oasis::read` is a stub (OASR); task 14.2 |
| A long `oasis write`: prepare on the command thread, `oasis::emit` on a worker, edits queued | `write_host` and `emit_host`, as `gds write` (D4: the M5 three-phase write) | blocked: `oasis::write` and `oasis::emit` are stubs (OASW); task 14.2 |
| A cancelled `oasis write` leaves no output file | the writers' temporary file (`mask::OutputFile`) | blocked: task 14.2 |
| `lef read` and `def read` report LefEstimate progress (Q-LEFDEF-88) and can be interrupted | `read_host` with the LEF/DEF readers' reports | blocked: LEFR and DEFR stubs; task 14.2 |
| `lef write` and `def write` run under the write host | `write_host` (single-phase, exclusive lock) | blocked: LEFW and DEFW stubs; task 14.2 |

## Performance (PERF)

`hefesto-performance` "Batch throughput relative to Magic" for LEF/DEF and OASIS (design D22, Open Question 4 (a)). PERF records the `lefdef-bench` and `oasis-bench` results here (tasks.md section 16). Not measured yet.
