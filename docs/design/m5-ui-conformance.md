# M5 UI conformance

How the M5 desktop items (the GDS menu items, the `gds` and `cif` file prompts, and the progress and cancel of long reads and writes) match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the approved M4 boards they reuse (`openspec/changes/m5-mask-gds-cif/design.md` D16). M5 adds no new screen and no new board (Open Question 3, decided (a) on 2026-11-13): every element reuses `M4-Menu-Bar` and the long-running state of `M4-Script-Consent` and `M4-Console`. Same format and rules as [m4-ui-conformance.md](m4-ui-conformance.md), which stays the record for the M4 screens.

Every row names one element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally). If SHELL finds an element missing from every board, DESIGN draws it first (M2–M4 rule) and the row says so.

Owner: SHELL (seeded by the M5 scaffold, task 2.5; completed in task 12.3). The "Performance" section belongs to PERF (tasks.md section 14). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-005` stub during M5).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1–M4.

## How this was checked

Pass 1 (task 12.3, branch `m5-r5-shell` on main at 9997d707): MacBook (Apple silicon, macOS, cocoa), `macos-debug` preset with `-DHEFESTO_BUILD_DESKTOP=ON -DHEFESTO_WARNINGS_AS_ERRORS=ON`, no PDK. Checked by the view-model and prompt tests named in each row (`hefesto_shell_tests`, `hefesto_gui_tests`) and a window smoke run (`hefesto --smoke-frames 10 --no-vsync`: 10 frames on cocoa). No capture of the M5 items was compared with a board yet: the items are native menu entries and native file dialogs, which `--capture` does not draw, and the Interrupt card of a mask job is not reachable from the window until the `gds` and `cif` commands route through the host (gap below). No row is therefore **done**.

Missing from every board: nothing. The menu items are `M4-Menu-Bar` entries, the file prompts are the native dialogs of M4, and the progress of a read or write reuses the running header of `M4-Console` and the Interrupt card of `M4-Script-Consent` tile 7, with the card's title and detail lines carrying the file and `Completed N%`. No open item for DESIGN.

## Menu bar items (M4-Menu-Bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| File ▸ Read GDS issues the logged `magic::promptload gds` | the wrapper item (`gui/wrapper.cpp`); `later_milestone` (`menu_bar_model.cpp`) no longer disables it | partial: enabled, and choosing it logs `promptload gds`, asks the native open dialog and loads the single new top cell (`tests/unit/shell/m5_gds_menu_test.cpp`); no capture pass |
| File ▸ Write GDS issues the logged `magic::promptsave gds` | the wrapper item; enabled as Read GDS | partial: enabled, and choosing it logs `promptsave gds`, asks the native save dialog and writes the chosen file (`m5_gds_menu_test.cpp`); no capture pass |
| No CIF menu item (Magic's wrapper has none); `promptload cif` / `promptsave cif` reachable from the console and scripts | the prompt procedures (`gui/prompts.*`) | partial: no CIF entry in the bar (`m5_gds_menu_test.cpp`); the prompts run from the console (`tests/unit/gui/prompts_test.cpp`) |

## File prompts (`ui-gui-managers` "File prompts")

| Element | Implemented as | Status / gap |
|---|---|---|
| The dialogs' file types | `gui/prompts.cpp` passes wrapper.tcl's Tk extension lists (`GDS .gds .strm .cal`, `CIF .cif`, `Magic .mag`, `All files *`), which an intercepted `tk_getOpenFile` prints as Magic does (SCRIPT-GUI-11); the desktop host (`shell/file_picker_host.cpp`) turns each `.ext` into `*.ext` | partial: the native dialogs get glob patterns, and the save dialog's default extension (`gds`, `cif`) comes from the first one. Fixed in M5: the Tk extension lists reached the native dialogs unchanged and matched no file (regression test in `tests/unit/shell/file_picker_host_test.cpp`) |
| `promptload gds`: native open dialog through `platform::FilePicker`, the root name passed to `gds read` | `gui/prompts.*` (the `gds` kind): `gds read <root>`, then `load` of the only new top cell, else `Top-level cells read from GDS file: <list>` | partial: tested; gap: as in Magic, the root name of `chip.gds.gz` or `chip.cal` finds no file (`gds read` tries `.gds`, `.gds.gz`, `.gds2`, `.strm` and the bare name), see tasks.md 12.1 |
| `promptsave gds`: native save dialog, the extension rule of Q-MASK-210 | `gui/prompts.*`: `gds write <chosen file>`, as wrapper.tcl | partial: tested in both modes with a dotted directory (`/work/v1.0/chip.gds` is written as chosen). Design D16 says the pickers pass the root name; for `gds write` the root name would lose the extension under magic-compat ON in a dotted directory (Q-MASK-210 ON adds none when any `.` is in the path), so the chosen name is passed (decision raised in tasks.md 12.1) |
| `promptload cif`: the `cif_temp` load-and-delete sequence of Magic's wrapper.tcl | `gui/prompts.*` (the `cif` kind): `cellname create cif_temp`, `load cif_temp`, `cif read <root>`, `load` of its first child, `cellname delete cif_temp -noprompt`, Magic's "Cells read from GDS file: …" when there are several | partial: tested ("promptload cif reads through cif_temp and loads its first child") |
| `promptsave cif`: the `.cif` suffix rule of Q-MASK-320 | `gui/prompts.*`: `cif write <root name>` in both modes | partial: tested in both modes (`chip.cif` chosen writes `chip.cif`, never `chip.cif.cif`) |
| Scenario "GDS with a single top cell" (m4 task 9.5) | the `gds` prompt kind | partial: the scenario's test passes (`prompts_test.cpp` "GDS with a single top cell") |

## Progress and cancel (Q-MASK-414; M4-Console, M4-Script-Consent long-running state)

The host is `shell::MaskJobHost` (`src/shell/desktop/include/hefesto/shell/mask_job_host.hpp`), owned by the main window: its frames are the script pump's (`MainWindow::pump_script_frame`, input blocked), its card replaces the script card while a job runs, and the card's Interrupt cancels the job. Until the `gds` and `cif` commands hand their reads and emit phases to it (open item of task 12.2: `MaskSession` hooks set by the window), a read or write from the menu or the console still runs inline on the command thread without frames.

| Element | Implemented as | Status / gap |
|---|---|---|
| A long read shows the Tcl console's running header and, after 2 s, the canvas Interrupt card | `MaskJobHost::run_read`: on the command thread; each reader report pumps a frame (rate-limited); the card "Reading chip.gds · N s" | blocked: host tested (`tests/unit/shell/m5_mask_job_test.cpp`); the readers report no fraction yet and the `gds read` / `cif read` commands do not call the host (open items) |
| A long write: `prepare_write` on the command thread (exclusive lock), `emit_write` on a worker (shared lock), edits queued until it ends | `MaskSession::write_host` (`MaskJobHost::run_job`: the whole write on the command thread, frames pumped from the subtree walk's ticks, Interrupt stops it) and `emit_host` (`run_emit`: the emit phase on a `platform::run_on_stack` worker, 64 MiB stack, covered by the command's exclusive hold); console lines wait (`TclConsoleSession::set_external_busy`); DRC commits wait (`DrcPump::set_hold`) | tested through the `gds write`/`cif write` commands as the window wires the hosts: cancel during the walk and the emit, a covered write while frames read the layout and use index with vendor views and Tcl attached, a write while the background DRC has work (tsan leg), a console line queued; the asynchronous write the board describes (shared-lock emit, edits queued) is not built: background DRC pauses during a write (decision raised) |
| Cancel leaves no output file | the card's Interrupt sets the job's `mask::Progress` cancel flag; the writers' temporary file is removed (`mask::OutputFile`) | blocked: tested for GDS and CIF writes through the host ("a cancelled GDS or CIF write leaves no output file"); reachable from the window once the commands call the host |
| `Completed N%` in the console | the card's detail line (`completed_text`), from the generators' `mask::Progress` reports (about every 5 s) | blocked: card text tested; informative, masked in golden output |

## Performance (PERF)

`hefesto-performance` "Batch throughput relative to Magic" for GDS read and write (design D20; Open Question 2 decided (a): callgrind instructions per workload at or below Magic's count, then the baseline + 2 %). PERF records the `gds-bench` and `gds-roundtrip` results here (tasks.md section 14). The requirement now names the instruction gate (decision P-24, `docs/decisions/product.md`).

**How it is measured.** `perf.yml` `gds-bench`, on the x86_64 runner, with a GCC 14 Release build of hefesto-cli and Magic 8.3.694 from the reference image:

- **Workloads.** `bench/gds/gds_bench.py` holds the three D20 workloads:
  - `read`: `gds read sky130_fd_sc_hd.gds`;
  - `write`: `gds write` of the all-cells block with `gds merge no`;
  - `write-merge`: the same write with `gds merge yes`.
- **Scripts.** Each workload has a base script (technology load, and for the writes the block placed from the `.mag` cells without their vendor-GDS properties) and an op script (the same plus the operation).
- **Gate.** The instructions of the operation are callgrind's op total minus its base total. `bench/gds/gds_gate.py` gates each workload:
  - at most Magic's count, until `bench/gds/baseline.json` records Hefesto's baseline;
  - then at most that baseline + 2 %.
- **Information only.** The wall time of op minus base for both tools on the same runner (`--repeat 3`, minimum of each). The instructions by function, from `bench/drc/callgrind_pass_diff.py`.

`gds-roundtrip` replays every `MASK-RT-*` case, one job per shard of `scripts/gds_rt_shards.py`. It then writes Hefesto's equivalence report of a second read and write (`bench/gds/gds_equivalence.py`) into the job summary.

| Run | Workload | Hefesto | Magic | Gate |
|---|---|---|---|---|
| local, 2026-11-15, macOS arm64 (Apple silicon), `macos-release` hefesto-cli at `m5-r5-perf` (base 9997d707), sky130A at the M4 pin; wall time only (no callgrind or Magic on this Mac) | read | 1.36 s (447 cells) | not measured | CI |
| same | write | 0.51 s (450 cells, 5.4 MB) | not measured | CI |
| same | write-merge | 0.57 s | not measured | CI |
| CI `gds-bench`, Performance run 43754937070 (`m5-round5` at 4aa162f7), x86_64 runner, GCC 14 Release, callgrind (op minus base) | read | 9.214 billion instructions; 1.035 s | 10.929 billion; 1.589 s | met (0.84 x Magic) |
| same | write | 6.231 billion; 0.800 s | 7.099 billion; 1.207 s | met (0.88 x) |
| same | write-merge | 6.429 billion; 0.814 s | 7.174 billion; 1.150 s | met (0.90 x) |

| CI `gds-bench`, Performance run 43766374927 (`m5-round5` at c937e133, after the `getcell` fix), same runner and builds | read | 9.215 billion; 0.761 s (447 cells) | 10.929 billion; 0.948 s (447 cells) | met (0.84 x Magic); job passed |
| same | write | 6.185 billion; 0.645 s (448 cells) | 7.099 billion; 0.788 s (448 cells) | met (0.87 x) |
| same | write-merge | 6.383 billion; 0.668 s (448 cells) | 7.174 billion; 0.798 s (448 cells) | met (0.89 x) |

The masked write outputs of the two tools differed in runs 43754937070 and 43766374927, and Hefesto ran without Magic's `.pt3` fonts, so every font label lost its PRESENTATION, STRANS and MAG records (a local Debug write of the block: 5 415 200 bytes without the fonts, 5 549 876 with them): the gate compared less work than Magic's. Round 5 review: `gds_bench.py` (and the callgrind step) now gives Hefesto the pinned Magic checkout's fonts in `CAD_ROOT=<work>/cadroot`, as the golden replay does, and fails when the two writes differ after masking the dates (`--outputs` keeps both files for diagnosis); Performance run 43789233970 (`m5-round5` at c3e0a8d0, with the fonts and the identity check) met the gate with identical outputs: read 9.218 vs 10.929 billion instructions (0.84 x), write 6.192 vs 7.099 (0.87 x), write-merge 6.390 vs 7.174 (0.89 x); wall time 0.80 / 0.83 / 0.86 x (information). Run 43754937070's timing step had failed on the cell count after the writes (Hefesto 450, Magic 448). The two extra cells were `getcell` renames (`sky130_fd_sc_hd__nand2_2_a`, `nor2_2_a`): a cell read through `macro_sparecell` records its file with the extension, and `getcell` compared it with its own path plus ".mag". Fixed at integration (0e43f7dc, regression test in `tests/unit/cell/console_test.cpp`). The baseline is recorded in `bench/gds/baseline.json` from the first `gds-bench` run on main with identical outputs, as its comment says. DRC on the same branch (Performance run 43753404487): `drc-profile` 12.24 billion instructions per block pass (target 12.38 billion, met; Magic 10.98 billion, information); `drc-bench` block 1.15 s against Magic's 1.37 s (0.84 x, information), 1314 errors each.

Equivalence report, local (same build): `sky130_fd_sc_hd.gds`. First pass: read 1.9 s, write 0.4 s. Second pass: read 2.2 s, write 0.3 s. There are 5 differences, all in TEXT. For example, `sky130_fd_sc_hd__dlrtn_1`'s `VGND` label on 68/5 is at (230, 0) after the first write and at (235, 0) after the second. The other labels are in `a32oi_4`, `dlrtn_2`, `dlrtn_4` and `dlrtp_4`. These differences are information only (design D19) and are left for review by GDSR/GDSW.
