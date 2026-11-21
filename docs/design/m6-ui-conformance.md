# M6 UI conformance

How the M6 desktop items match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the approved boards they reuse (`openspec/changes/m6-lef-def-oasis/design.md` D16). M6 adds no menu item, no new screen and no new board (Open Question 5, decided (a) on 2026-11-18): Magic's wrapper has no LEF, DEF or OASIS item (wrapper.tcl:1235-1239), and the console reaches the `lef`, `def` and `oasis` commands. The M6 items are OASIS recognition on the existing open paths and the progress and cancel of LEF, DEF and OASIS reads and writes through the M5 hosts. Same format and rules as [m5-ui-conformance.md](m5-ui-conformance.md), which stays the record for the M5 items.

Every row names one element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally). If SHELL finds an element missing from every board, DESIGN draws it first (M2–M5 rule) and the row says so.

Owner: SHELL (seeded by the M6 scaffold, task 2.6; completed in tasks 14.1–14.3). The "Performance" section belongs to PERF (tasks.md section 16). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-006` stub during M6; the code is retired at M6 exit).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1–M5.

## How this was checked

Round 4 (SHELL, tasks 14.1-14.3) built the open path and the host wiring and tested them below the toolkit: `tests/unit/shell/m6_open_test.cpp` (desktop builds) and `tests/unit/oasis/command/open_test.cpp` (every build). No capture of the product app was taken in round 4 (no desktop build on that host), so no row is **done** yet; each row says what its capture must show.

Missing from every board: nothing. The open paths are the M1 Open button and the window itself as a drop target (no new control), and the progress of a read or write reuses the M5 rows (the running header of `M4-Console` and the Interrupt card of `M4-Script-Consent` tile 7).

## Menu bar (M4-Menu-Bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| No LEF, DEF or OASIS menu item (Open Question 5 (a); Magic's wrapper has none) | the M5 wrapper items unchanged; no board added | partial: test "the desktop adds no LEF, DEF or OASIS menu item" checks the bar's menus and that no item runs `lef`, `def` or `oasis`; the capture is pending |

## Format recognition on open (`oasis-command` "Format recognition on open", D15)

| Element | Implemented as | Status / gap |
|---|---|---|
| File ▸ Open (the native picker, the top bar's Open button) of an OASIS file runs `oasis read` with the current settings, whatever the extension | the picker offers every file (`kOpenFilterPatterns` `*`); `shell::plan_open` asks `app::open_command`, which sniffs the first 13 bytes after gzip (`oasis::sniff_format`), and the main window runs the line as a typed command (echoed, logged, through the read host) | partial: test "the desktop opens an OASIS file named .gds as OASIS"; capture pending. The Open button's tooltip now says "Open a layout file: a Magic cell, GDS or OASIS (by content)" |
| Drag and drop of an OASIS file onto the window | `ui::Window::on_files_dropped` (Raven's window drop callback, where no widget takes the drop), then the same open path for each file | partial: built against Raven's `Rwindow_set_drop_callback`; the drop is not injected in a test (`Rwindow_inject_drop` needs a window); capture pending |
| A file that is not OASIS opens as before (`.mag` with `load`, GDS with `gds read`) | the same open path | partial: tests in `open_test.cpp` ("GDS and Magic cells keep their commands"); a file of no known format is reported (OASIS-CMD-005) in the Diagnostics panel and the other dropped files still open |

## Progress and cancel (M5 hosts: `MaskSession::read_host`, `write_host`, `emit_host`)

| Element | Implemented as | Status / gap |
|---|---|---|
| A long `oasis read` shows the console's running header and, after 2 s, the canvas Interrupt card | the `oasis read` command runs `oasis::read` through `MaskSession::read_host`, with its `mask::Progress` reports (as `gds read`) | partial: test "oasis read runs through the desktop's read host"; capture pending |
| A long `oasis write`: prepare on the command thread, `oasis::emit` on a worker, edits queued | the `oasis write` command (and a bare `oasis`) runs `oasis::write` through `write_host`; `emit_host` is OASW's (D4: the M5 three-phase write) | blocked: `oasis::write` is still the scaffold stub until OASW (task 12.1) lands |
| A cancelled `oasis write` leaves no output file | the writers' temporary file (`mask::OutputFile`) | blocked: test "a cancelled OASIS write leaves no output file" passes against the stub; its end-to-end check needs OASW |
| `def read` reports progress and can be interrupted | `read_host` with the DEF reader's reports (DEFR, round 3) | partial: capture pending |
| `lef read` reports LefEstimate progress (Q-LEFDEF-88) and can be interrupted | partly: `lef read` runs through `read_host` (round-4 integration, `lef/read/args.cpp`), so the desktop shows its card; `lef::read` reports no LefEstimate fraction and cannot be interrupted (`LefReadOptions` has no progress field) | todo: LEFR adds `LefReadOptions::progress` (LefEstimate and the interrupt check) |
| `lef write`, `lef writeall` and `def write` run under the write host | yes (round-4 integration): `lef write`/`writeall` pass the host's `Progress` into `LefWriteOptions::progress`; `def write` runs under the host without progress (`def::write` builds the file in memory and reports none) | done; a `DefWriteOptions::progress` is a later improvement |

## Performance (PERF)

`hefesto-performance` "Batch throughput relative to Magic" for LEF/DEF and OASIS (design D22, Open Question 4 (a): each workload gated at its own callgrind baseline + 2 %; the Magic and GDS-path comparisons are information). PERF records the `lefdef-bench` and `oasis-bench` results here (tasks.md section 16).

**How it is measured.** `perf.yml` `lefdef-bench` and `oasis-bench`, on the x86_64 runner, with a GCC 14 Release build of hefesto-cli (`-g -fno-omit-frame-pointer`, as `gds-bench`):

- **LEF/DEF workloads** (`bench/lefdef/lefdef_bench.py`, sky130-lef and sky130 sets):
  - `lef-read`: `lef read sky130_fd_sc_hd.lef`, after the technology LEF `sky130_fd_sc_hd__nom.tlef`;
  - `def-read`: `def read lefdef_block.def` over the library. No pinned data set has a routed sky130 DEF, so `bench/lefdef/defgen.py` generates one from the cell LEF: 5000 cells in abutting N/FS rows, 5001 nets routed on li1, met1 and met2 with L1M1 and M1M2 vias, met1 power rails and met4 straps. The same LEF always gives the same bytes;
  - `lef-writeall`: `lef writeall -notop` of a wrapper placing every cell read, as `LEFDEF-RT-sky130_fd_sc_hd` does.
  Magic 8.3.694 (reference image) runs the same scripts.
- **OASIS workloads** (`bench/oasis/oasis_bench.py`, the `gds-bench` workloads; sky130 and sky130-gds-sc sets). Magic has no OASIS, so the comparison is Hefesto's GDS path over the same layouts:
  - `read`: `oasis read sky130_fd_sc_hd.oas` against `gds read sky130_fd_sc_hd.gds`. hefesto-cli converts the library once (`gds read`, a wrapper placing every cell read, `oasis library yes`, `oasis write`), so both reads create the same cells;
  - `write`, `write-merge`: `oasis write` of the all-cells block with `oasis merge no`/`yes`, against `gds_bench.py`'s `gds write` scripts.
- **Scripts.** Each workload has a base script (the set-up) and an op script (the set-up and the operation). Its cost is callgrind's op total minus its base total.
- **Gate.** `bench/lefdef/baseline_gate.py` fails a workload above its baseline + 2 % (`bench/lefdef/baseline.json`, `bench/oasis/baseline.json`). A workload with no recorded baseline is reported as "no baseline" and passes. A missing Hefesto profile fails the job.
- **Information only.** Magic's count (LEF/DEF) or the GDS path's count (OASIS), the wall time of op minus base (`--repeat 3`), and the instructions by function (`bench/drc/callgrind_pass_diff.py`). The timing step fails when the cell counts of a workload differ (Hefesto against Magic, OASIS read against GDS read) or a write is empty.

**Recording the baselines.** Once a Performance run on main (or a `workflow_dispatch` of `perf.yml` on main) has passed `lefdef-bench` and `oasis-bench`:

1. Download its artifacts: `gh run download <run> -n lefdef-bench -D lefdef-bench` and `gh run download <run> -n oasis-bench -D oasis-bench`.
2. Record the counts:
   - `python3 bench/lefdef/baseline_gate.py lefdef-bench --baseline bench/lefdef/baseline.json --tool hefesto --reference magic --workload lef-read --workload def-read --workload lef-writeall --record <run> --commit <sha>`;
   - `python3 bench/lefdef/baseline_gate.py oasis-bench --baseline bench/oasis/baseline.json --tool oasis --reference gds --workload read --workload write --workload write-merge --record <run> --commit <sha>`.
3. Add the rows to the table below and commit both baseline files. From then on, each workload is gated at its baseline + 2 %.

A change to a workload (for example `defgen.py`'s parameters or a script) needs its baseline recorded again in the same pull request.

| Run | Workload | Hefesto | Comparison | Gate |
|---|---|---|---|---|
| local, 2026-11-20, macOS arm64, `macos-debug` hefesto-cli at `m6-r5-perf` (base 0c5f29fb); wall time of a Debug build only, a smoke run of the scripts (no callgrind or Magic on this Mac) | lef-read | 1.55 s (440 cells) | not measured | CI |
| same | def-read | 5.78 s (441 cells; 5000 instances, 2 pins, 2 special nets, 5001 nets read) | not measured | CI |
| same | lef-writeall | 2.86 s (2.7 MB written) | not measured | CI |
| CI `lefdef-bench` and `oasis-bench` | all | pending: the first dispatch reports the counts, then the baselines are recorded as above | | |
