# M3 UI conformance

How the M3 desktop DRC screens match the forge design system (`openspec/specs/hefesto-design-system/spec.md`, decision P-23) and the M3 boards drafted under [mockups/](mockups/) (`openspec/changes/m3-drc/design.md` D11). Same format and rules as [m2-ui-conformance.md](m2-ui-conformance.md) and [m1-ui-conformance.md](m1-ui-conformance.md), which stay the records for the M1 and M2 screens.

Every row names one board element, how the shell implements it, and any gap with its reason. Where Raven cannot match a board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally).

Owner: SHELL (seeded by the M3 scaffold, task 1.5; completed in task 9.5). The "M3 boards" section belongs to DESIGN (task 2.1) and "Performance" to ENGINE (task 6.8). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the milestone named.
- **blocked**: built in the shell, but the core function it needs is not available yet (a `CORE-NOTIMPL-003` stub during M3).
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with its board, as in M1 and M2.

## How this was checked

To be filled by SHELL (task 9.5): platform, build, the commands driving each capture (every browser action is a `drc` command, design D11), and the board each capture is compared with.

## M3 boards (DESIGN)

Owner of this section: DESIGN (task 2.1). `DRC-Browser.dc.html` updated with the M3 states and the new `M3-DRC-Status.dc.html`, rendered in both themes to `docs/images/ui/m3-*.png`. Each SHELL screen state of section 9 maps to a board. In this table, **drawn** means the board is drawn and rendered and waits for the user's review; it becomes **approved** after that review:

| SHELL state (tasks.md section 9) | Board | Status |
|---|---|---|
| Top-bar DRC chip: not run, checking, clean, N errors, off | `M3-DRC-Status` | drawn (awaiting user review) |
| Status-bar DRC state (same five states) | `M3-DRC-Status` | drawn (awaiting user review) |
| Browser: empty, running, stale, populated | `DRC-Browser` | drawn (awaiting user review) |
| Errors grouped by why-text with counts; `#k of n` stepping | `DRC-Browser` | drawn (awaiting user review) |
| Error view on the canvas; `drc why` popover | `DRC-Browser` | drawn (awaiting user review) |
| Ignored rules; style selector | `DRC-Browser` | drawn (awaiting user review) |
| Later-milestone elements (Fix with agent M11, nets M7, measured vs required, sign-off M13) disabled or hidden | `DRC-Browser` | drawn (awaiting user review) |

## DRC status (top bar and status bar)

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| DRC chip with shape + text (replaces M1's static "◆ DRC not run", `drc_chip_m1`) | | todo | Driven by `drc::DrcEvents` and the root cell's error-tile count (design D11). |
| Status-bar DRC state | | todo | |

## DRC results browser

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Error list grouped by why-text (virtual Raven Table, 200,000 rows) | | todo | Built from `drc listall why` over the root cell. |
| Prev / Next and `#k of n` | | todo | Issues `drc find`. |
| Re-check | | todo | Issues `drc check; drc catchup`. |
| Ignore a rule | | todo | Issues `drc ignore`. |
| Style selector | | todo | Issues `drc style`. |
| Stale marking after edits | | todo | Until the worker goes idle. |

## Canvas and notifications

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| `drc why` result and the error tile under the box | | todo | |
| Job-completion notification for a long `drc catchup` (window unfocused) | | todo | |

## Background checker in the app loop

| Item | Implemented as | Status | Gap / why |
|---|---|---|---|
| Worker started and stopped with `drc on` / `drc off` | | todo | `drc::BackgroundChecker` (design D8). |
| Commits between commands from the event loop | | todo | `BackgroundChecker::commit_ready`. |
| Scene building under the shared database lock | | todo | `db::DatabaseLock::Shared` (`editor::EditorState::database_lock`). |
| Redisplay of committed square areas | | todo | `DrcObserver::on_square_committed`. |

## Accessibility and keyboard

To be filled by SHELL: accessible names of the chip, browser rows and actions; keyboard stepping; screen-reader announcements of DRC state changes.

## Performance

Owner of this section: ENGINE (task 6.8, design D16).

Measured 2026-11-10 on an Apple M3 Pro (12 cores, macOS 27), branch `m3-round2`, Release builds (`-Werror`), sky130A at the pinned PDK version, `sky130A.tech` default DRC style with `scalegrid 1 2` and `drc euclidean on`. Magic 8.3.694 (`fc5daa9e`, Tcl build without OpenGL/Cairo, as in `tools/golden/container/Containerfile`) runs `-dnull -noconsole` in a Linux arm64 container on the same machine. The machine was otherwise idle for the recorded runs (load average below 8); runs taken while other builds loaded it varied by up to 2x and are not recorded.

| Measurement | Target | Hefesto | Magic | Notes |
|---|---|---|---|---|
| Background start latency after a paint (100 × 100 `sky130_fd_sc_hd__dfxtp_1` array) | ≤ 100 ms | median 1.6 ms, max 3.6 ms (macOS); median 1.9 ms, max 5.4 ms (Linux arm64 container, GCC 14) | — | Met. From the end of the `paint` command to the worker's first kernel call, 20 paints of 60 × 40 m1 spread over the array, the worker hosted by `drc::BackgroundHost` with real rules. The square is then committed in median 38 ms (max 65 ms). Hidden test `[benchmark]` in `tests/unit/drc/engine/latency_bench_test.cpp`. |
| `drc check; drc catchup`, dfxtp_1 array (FIXED_BBOX pitch), single-threaded | no slower than Magic | 0.068 s | 0.069 s | Met (parity). 81 errors in both. |
| `drc check; drc catchup`, all-cells block (441 cells in rows of 40000 units, abutting at FIXED_BBOX), single-threaded | no slower than Magic | 1.19 s | 1.16 s | Within the 10 % noise band of the CI job (+2.6 %). 1292 errors in both. |
| Pan and zoom frame times while the worker checks the all-cells block | interactive (`hefesto-architecture` "Concurrency model") | | — | SHELL, task 9.1 |

Method (`bench/drc/drc_bench.py`): the same command script runs through `hefesto-cli --magic -dnull -noconsole` and `magic -dnull -noconsole`; the DRC time is (min of the script with 4 passes of `drc check; drc catchup` − min of the script with 1 pass) / 3 over 5 runs each, which cancels process start, technology and cell loading and container file-system cost; the error counts (`drc list count total`) must agree. CI: `.github/workflows/perf.yml` runs both tools on one GitHub runner (Hefesto native, Magic in the pinned reference image) and fails beyond `--max-ratio 1.10` or on a count mismatch.

Gap plan: the block is at parity, not ahead. If the CI job shows Hefesto outside the band on its runner, the next steps are a profile of the block run split into basic check, interaction flattening and commit time per square, then the parallel mode of D16 (independent squares on several workers).

## Remaining gaps (open items)

None recorded yet.
