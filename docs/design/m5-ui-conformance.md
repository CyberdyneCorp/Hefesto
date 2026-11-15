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

No pass yet. SHELL records each pass here (machine, build, PDK pin, command lines and captures), as in [m4-ui-conformance.md](m4-ui-conformance.md).

## Menu bar items (M4-Menu-Bar)

| Element | Implemented as | Status / gap |
|---|---|---|
| File ▸ Read GDS issues the logged `magic::promptload gds` | the wrapper item (`gui/wrapper.cpp`), disabled by `later_milestone` in `menu_bar_model.cpp` | later (M5): SHELL enables it in task 12.1 |
| File ▸ Write GDS issues the logged `magic::promptsave gds` | the wrapper item (`gui/wrapper.cpp`), disabled by `later_milestone` in `menu_bar_model.cpp` | later (M5): SHELL enables it in task 12.1 |
| No CIF menu item (Magic's wrapper has none); `promptload cif` / `promptsave cif` reachable from the console and scripts | the prompt procedures (`gui/prompts.*`) | todo (task 12.1) |

## File prompts (`ui-gui-managers` "File prompts")

| Element | Implemented as | Status / gap |
|---|---|---|
| `promptload gds`: native open dialog through `platform::FilePicker`, the root name passed to `gds read` | `gui/prompts.*` (the `gds` kind) | todo (task 12.1); `gds read` answers `CORE-NOTIMPL-005` until GDSR lands it |
| `promptsave gds`: native save dialog, the extension rule of Q-MASK-210 | `gui/prompts.*` (the `gds` kind) | todo (task 12.1); `gds write` answers `CORE-NOTIMPL-005` until GDSW lands it |
| `promptload cif`: the `cif_temp` load-and-delete sequence of Magic's wrapper.tcl | `gui/prompts.*` (the `cif` kind) | todo (task 12.1) |
| `promptsave cif`: the `.cif` suffix rule of Q-MASK-320 | `gui/prompts.*` (the `cif` kind) | todo (task 12.1) |
| Scenario "GDS with a single top cell" (m4 task 9.5) | the `gds` prompt kind | todo (task 12.1) |

## Progress and cancel (Q-MASK-414; M4-Console, M4-Script-Consent long-running state)

| Element | Implemented as | Status / gap |
|---|---|---|
| A long read shows the Tcl console's running header and, after 2 s, the canvas Interrupt card | `mask::Progress` hosted on the command thread with frames pumped (design D16) | todo (task 12.2) |
| A long write: `prepare_write` on the command thread (exclusive lock), `emit_write` on a worker (shared lock), edits queued until it ends | `gds::write` / `cif::write` two-phase entry points (design D4) | todo (task 12.2) |
| Cancel leaves no output file | the writers' temporary file and rename (design D8) | todo (task 12.2) |
| `Completed N%` in the console | informative text, masked in golden output | todo (task 12.2) |

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
| CI `gds-bench` | read, write, write-merge | to record | to record | to record |

Equivalence report, local (same build): `sky130_fd_sc_hd.gds`. First pass: read 1.9 s, write 0.4 s. Second pass: read 2.2 s, write 0.3 s. There are 5 differences, all in TEXT. For example, `sky130_fd_sc_hd__dlrtn_1`'s `VGND` label on 68/5 is at (230, 0) after the first write and at (235, 0) after the second. The other labels are in `a32oi_4`, `dlrtn_2`, `dlrtn_4` and `dlrtp_4`. These differences are information only (design D19) and are left for review by GDSR/GDSW.
