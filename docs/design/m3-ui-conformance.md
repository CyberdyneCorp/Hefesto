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

SHELL pass, 2026-11-10, macOS 27 arm64 (Apple M3 Pro, MoltenVK, Retina 2×, 1600×1000 logical), build `build/m3r2b-shell` (Debug, `-Werror`, desktop on), branch `m3-round2b`. Every capture is the product app on `sky130_fd_sc_hd__dfxtp_1.mag` (sky130A at the pinned PDK version, `drc(fast)` the default style) opened through the core `load` path and driven by `--run`, so every state is reached through the command API; two 0.10 × 0.10 µm metal1 squares 0.10 µm apart are painted first so that the cell has errors of several rules:

```
CAD_ROOT=<dir with magic/sys styles> \
hefesto -T sky130A/libs.tech/magic/sky130A.tech sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__dfxtp_1.mag \
        --width 1600 --height 1000 --run '<commands>' --capture out.ppm --capture-frames 30
```

`E` below is `box 0 0 10 10; paint m1; box 30 0 40 10; paint m1; drc check; drc catchup; panel drc show`. The captures are halved to 1600 × 990 PNG in `docs/images/app/`.

| Capture | `--run` | Board |
|---|---|---|
| [m3-drc-not-run.png](../images/app/m3-drc-not-run.png) | `panel drc show` | M3-DRC-Status (Not run chip and status bar), DRC-Browser ("Empty · not checked") |
| [m3-drc-browser.png](../images/app/m3-drc-browser.png) | `E; drc find 3; findbox zoom; zoom 0.25` | DRC-Browser (populated, stepping `#3 of 11`, inspector), M3-DRC-Status (N errors) |
| [m3-drc-why.png](../images/app/m3-drc-why.png) | `E; drc find 3; findbox zoom; zoom 0.25; drc why` | M3-DRC-Status tile 4 (`drc why` popover) |
| [m3-drc-light.png](../images/app/m3-drc-light.png) | `--theme light`, `E; drc ignore li.3; drc find 1; findbox zoom; zoom 0.25` | DRC-Browser (Ignored section, Ignore field prefilled with the rule tag), light theme |

Not reachable from `--run` and covered by tests instead: the checking state (the worker is busy only between frames of a real session: `drc_link_test.cpp` drives the pump and observes busy → idle), the stale state (the list's stamp, `drc_link_test.cpp`), the chip menu and the style menu (right click; their entries are the `drc_chip_menu` / `drc_style_menu` view models, every entry run through a real Dispatcher and found in the command log in `drc_link_test.cpp`), table clicks and keys (Raven Table callbacks over `DrcBrowserModel`, `drc_models_test.cpp`), and the job notice (needs the window unfocused for 5 s; `drc_job_notice` tests and the `hefesto::notifications` setting test).

## M3 boards (DESIGN)

Owner of this section: DESIGN (task 2.1). `DRC-Browser.dc.html` updated with the M3 states and the new `M3-DRC-Status.dc.html`, rendered in both themes to `docs/images/ui/m3-*.png`. Each SHELL screen state of section 9 maps to a board. The user approved both boards as drawn on 2026-11-10 (chip precedence off > checking > not run > N errors > clean; N = `drc list count total` beside the per-group area counts; Re-check = `select top cell; drc check; drc catchup`). Status: **approved** = reviewed and approved; **implemented** = built by SHELL (rows of the sections below give the per-element status):

| SHELL state (tasks.md section 9) | Board | Status |
|---|---|---|
| Top-bar DRC chip: not run, checking, clean, N errors, off | `M3-DRC-Status` | approved, implemented |
| Status-bar DRC state (same five states) | `M3-DRC-Status` | approved, implemented |
| Browser: empty, running, stale, populated | `DRC-Browser` | approved, implemented |
| Errors grouped by why-text with counts; `#k of n` stepping | `DRC-Browser` | approved, implemented |
| Error view on the canvas; `drc why` popover | `DRC-Browser` | approved, implemented |
| Ignored rules; style selector | `DRC-Browser` | approved, implemented |
| Later-milestone elements (Fix with agent M11, nets M7, measured vs required, sign-off M13) disabled or hidden | `DRC-Browser` | approved, implemented |

## DRC status (top bar and status bar)

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| DRC chip with shape + text (replaces M1's static "◆ DRC not run", `drc_chip_m1`) | `drc_status_view()` from `DrcStatusInput` (DrcLink busy/idle and checked-since-opened, `drc::count_errors` of the root over the current window's area, i.e. `drc list count total`, read only while idle; `DrcSession::background`, `style`); precedence off > checking > not run > N errors > clean; "■ 12,408 DRC errors" grouped, singular for 1; a button: click issues `panel drc show`, the ▾ zone or right click / Menu key opens the chip menu | done | Captures not-run, browser. The chip is sized for "88,888 DRC errors" so the bar does not shift. |
| Chip menu (Open DRC panel, Check now, Find next, Why, Background checking, Style ▸) | `drc_chip_menu()`: `panel drc show`, `select top cell; drc check; drc catchup`, `drc find; findbox zoom`, `drc why`, `drc on`/`drc off` (checked while on), one checked "Style: <name>" per `DrcSession::styles` → `drc style <name>` | partial | Raven 0.2.2 menus have no submenu: the styles are flat entries after a separator. The key caps (`!`, `?`) are text in the label. |
| Chip tooltip and accessible name | "12 error tiles in ota_5t · style drc(full) · ! next error"; name "DRC: 12 errors, style drc(full). Open the DRC panel"; state changes announced politely (`ui::announce`) | done | |
| Status-bar DRC state | `StatusBarInput::drc = DrcStatusView::status_bar`: "■ DRC 12 errors · drc(full)", "◆ DRC checking… · drc(full)", "● DRC 0 errors · …", "◆ DRC off · 12 errors, last check", "◆ DRC not run"; colored by its kind, never color only | done | |
| Dock indeterminate progress while checking | `Window::set_busy_progress` (`Rwindow_set_taskbar_progress`, INDETERMINATE while busy, NONE on idle) | partial | Seen in code paths and unit-tested state only; the Dock badge was not captured (no screenshot of the Dock). |

## DRC results browser

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| Error list grouped by why-text (virtual Raven Table, 200,000 rows) | `ui::Table` (Raven `Rtable_set_virtual_rows`: no widget per row) over `DrcBrowserModel`; columns Why-text / Areas (sortable: header click sorts by text or count, default count descending); groups collapsed, Return or double click expands them to their rectangles in discovery order; selecting a rectangle issues `box values <x0>i <y0>i <x1>i <y1>i; findbox zoom` (internal units, exact under any `units`); filter field | partial | Capture browser. The list is DRCWhyAll over the root cell's bounding box (`drc::why_errors(all)`, the function behind `drc listall why`), run on the UI thread when the panel is shown and the checker is idle, without issuing `select top cell` (which would move the user's box and selection after every edit); user actions stay commands. Raven Table has no tree rows or per-row dimming: groups use ▸/▾ prefixes and ignored rows ⊘. 200,000-row model test in `drc_models_test.cpp`. |
| Summary: error tiles and areas | `DrcBrowserModel::summary`: "■ 11 error tiles · 35 areas · 6 why-texts · 1 ignored" (tiles = `drc list count total`, areas = rectangles of DRCWhyAll); ◆ before the cell was checked | done | Captures. |
| Prev / Next and `#k of n` | `DrcStepper`: `drc find <k±1>; findbox zoom` with an explicit index, wrapping n→1 / 1→n, the first Next is `drc find 1` (Q-DRC-15); k from the last `Error area #k:` printed by any `drc find` (also `!` and the command bar), n = error tiles | partial | Capture browser (`#3 of 11`). The stepper sits in the DRC panel under the summary, not over the canvas as on the board (Raven has no anchored canvas toolbar beside the zoom control yet). |
| Re-check | `select top cell; drc check; drc catchup` ("Check" before the root was checked); disabled while busy | done | |
| Ignore a rule | inspector field prefilled with the rule tag of the found error (else the first group) → `drc ignore <text>` (braced when it has spaces); the Ignored heading row activates `drc ignore none`; ignored groups listed with ◆ counts | done | Capture light (`li.3` ignored). The chip count does not drop (tiles stay). |
| Style selector | "Style <name> ▾" opens the `drc_style_menu` (checked current) → `drc style <name>`; disabled while busy | done | |
| Stale marking after edits | the list's stamp (worker edit generation, commit epoch, style changes, `DrcLink::Stamp`) differs from the database's; the list rebuilds when the checker goes idle with DRC on, or after a `drc catchup` (Re-check) with DRC off | partial | No capture (the stale state lasts only while the worker is busy in a live session); state precedence in `drc_browser_state` tests. With DRC off, CmdPaint does not mark the area (Magic), so edits under `drc off` do not make the list stale — the board's "Edits still mark areas" (M3-DRC-Status, Off) does not hold for paint; open item for DESIGN. |
| Empty · not checked, Empty · clean, Running notices | `drc_browser_notice`: ◆ "Not checked yet", ● "No DRC errors in <cell>", ◆ "Checking <cell>…" | done | Capture not-run. |
| Inspector: error area under the box | right dock while the DRC panel is shown: "DRC ERROR AREA #k OF n", the found why-texts (■), Cell / Area / Size / Style, Zoom to (`findbox zoom`), Why here (`drc why`) | done | Captures browser and light. |
| Later-milestone parts | Fix… (M11) disabled in the panel toolbar; Sign-off (M13) collapsed and disabled; Measured vs required (M11), Nets (M7), Suggested fix · Preview fix (M11) disabled rows with tooltips naming the milestone; the tech-file rule excerpt hidden | done | |

## Canvas and notifications

| Board element | Implemented as | Status | Gap / why |
|---|---|---|---|
| `drc why` result and the error tile under the box | any plain `drc why` (chip menu, Why here, `?`, the command bar) opens a popover beside the box with one ■ row per why-text in discovery order, "● No errors found." when clean; it closes on Esc or the next command. Error tiles are drawn by the scene in the technology's error styles (`error_waffle`) for the root only ("DRC error display") and redrawn per committed square | partial | Capture why. The popover is a Raven menu (no anchored popover widget, M2 gap) placed right of the box. The canvas tag "#k why-text" on the found tile is not drawn yet. |
| Job-completion notification for a long `drc catchup` (window unfocused) | a busy → idle run of at least 5 s that ends while the window is not active (`Window::active`) posts "DRC finished · <cell>" with "■ 12 errors · drc(full) · 48 s": the Dock icon asks for attention (`Rwindow_request_attention`), a Raven Toast waits on the canvas and the Console logs it; `hefesto::notifications off` (session layer of the `notifications` setting) turns it off | partial | Raven 0.2.2 has no system notification API (macOS Notification Center, freedesktop): open item for Raven. Clicking the notice to focus and open the panel needs that API too. |

## Background checker in the app loop

| Item | Implemented as | Status | Gap / why |
|---|---|---|---|
| Worker started and stopped with `drc on` / `drc off` | `DrcPump` owns the session's `drc::BackgroundHost`, constructed after startup in `run_window` and destroyed before the window; `drc on`/`off` switch DRCBackGround, which the worker reads | done | `drc_link_test.cpp`: off keeps it idle, on resumes it. |
| Commits between commands from the event loop | the worker's ready callback calls `DrcPump::notify`, which posts one task (`ui::post_task` → `Rtask_post`, coalesced) running `BackgroundHost::service()` on the UI thread | done | `drc_link_test.cpp`: one task for repeated callbacks; the background result equals a later `drc check; drc catchup` on the same cell. |
| Scene building under the shared database lock | `LayoutCanvas` builds whole and partial scenes holding `db::DatabaseLock::Shared`; the browser's DRCWhyAll and the error count hold the lock | done | |
| Redisplay of committed square areas | `DrcLink::on_square_committed` → `WindowManager::area_changed(cell, square)`; `MainWindow::on_drc_service` runs `update_display` and the canvas splices the area | done | Test: a committed square leaves a pending redisplay in the window. |

## Accessibility and keyboard

| Item | Implemented as | Status | Gap / why |
|---|---|---|---|
| Chip | role button, name "DRC: <state>[, style <s>]. Open the DRC panel"; Enter/Space opens the panel, Menu key / Shift-F10 the menu; focus ring | done | |
| State changes | announced politely with the status-bar text when the chip's state or count changes | done | |
| Browser table | Raven Table: arrows move the selection (a rectangle row moves the box), Return expands a group or clears ignores on the heading; Raven reads the cell texts; name "DRC errors by why-text" | partial | Raven 0.2.2 has no per-row accessible description: the "expanded/collapsed" state and the area count are in the cell text, `row_accessible_name()` is ready for when Raven takes one. |
| Panel buttons and fields | named buttons (Style, Re-check, Prev, Next, Zoom to, Why here, Ignore), "#3 of 11" also as "Error 3 of 11"; filter and ignore fields named | done | |
| F6 | the left region focuses the DRC table while the DRC panel is shown | done | |

## Performance

Owner of this section: ENGINE (task 6.8, design D16).

Measured 2026-11-10 on an Apple M3 Pro (12 cores, macOS 27), branch `m3-round2`, Release builds (`-Werror`), sky130A at the pinned PDK version, `sky130A.tech` default DRC style with `scalegrid 1 2` and `drc euclidean on`. Magic 8.3.694 (`fc5daa9e`, Tcl build without OpenGL/Cairo, as in `tools/golden/container/Containerfile`) runs `-dnull -noconsole` in a Linux arm64 container on the same machine. The machine was otherwise idle for the recorded runs (load average below 8); runs taken while other builds loaded it varied by up to 2x and are not recorded.

| Measurement | Target | Hefesto | Magic | Notes |
|---|---|---|---|---|
| Background start latency after a paint (100 × 100 `sky130_fd_sc_hd__dfxtp_1` array) | ≤ 100 ms | median 1.6 ms, max 3.6 ms (macOS); median 1.9 ms, max 5.4 ms (Linux arm64 container, GCC 14) | — | Met. From the end of the `paint` command to the worker's first kernel call, 20 paints of 60 × 40 m1 spread over the array, the worker hosted by `drc::BackgroundHost` with real rules. The square is then committed in median 38 ms (max 65 ms). Hidden test `[benchmark]` in `tests/unit/drc/engine/latency_bench_test.cpp`. |
| `drc check; drc catchup`, dfxtp_1 array (FIXED_BBOX pitch), single-threaded | no slower than Magic | 0.068 s | 0.069 s | Met (parity). 81 errors in both. |
| `drc check; drc catchup`, all-cells block (441 cells in rows of 40000 units, abutting at FIXED_BBOX), single-threaded | no slower than Magic | 1.19 s | 1.16 s | Within the 10 % noise band of the CI job (+2.6 %). 1292 errors in both. |
| Pan and zoom frame times while the worker checks the all-cells block | interactive (`hefesto-architecture` "Concurrency model") | median 8.3 ms, p95 9.8 ms, p99 14.8 ms, max 21.4 ms (400 frames); same block with DRC off: median 8.4 ms, p95 9.5 ms, p99 10.0 ms, max 20.3 ms | — | Met. SHELL, task 9.1, 2026-11-10: Debug build `build/m3r2b-shell`, `hefesto -T sky130A.tech --run "source block.tcl; drc on; drc check" --bench-frames 400 --no-vsync` (block.tcl: `bench/drc/drc_bench.py`'s block placement after `snap internal; scalegrid 1 2; drc off; drc euclidean on`), the canvas panned every frame while the worker checked and committed squares (the canvas instances grew from 2063 to 3329 with the error tiles). |

Method (`bench/drc/drc_bench.py`): the same command script runs through `hefesto-cli --magic -dnull -noconsole` and `magic -dnull -noconsole`; the DRC time is (min of the script with 4 passes of `drc check; drc catchup` − min of the script with 1 pass) / 3 over 5 runs each, which cancels process start, technology and cell loading and container file-system cost; the error counts (`drc list count total`) must agree. CI: `.github/workflows/perf.yml` runs both tools on one GitHub runner (Hefesto native, Magic in the pinned reference image) and fails beyond `--max-ratio 1.10` or on a count mismatch.

CI runner (INTEGRATION, round 2b): numbers from the `drc-bench` artifact of PR #29's Performance run 42923284311 (2026-11-10), GitHub-hosted `ubuntu-24.04` x86_64 (Azure), `linux-release` with GCC 14, Magic 8.3.694 from the pinned reference image on the same runner, `--repeat 3`:

| Measurement | Target | Hefesto | Magic | Ratio | Result |
|---|---|---|---|---|---|
| Background start latency (100 × 100 dfxtp_1 array) | ≤ 100 ms | median 2.5 ms, max 5.5 ms (until idle: median 58 ms, max 104 ms) | — | — | Met |
| `drc check; drc catchup`, dfxtp_1 array | ≤ 1.10 × Magic | 0.104 s (1 pass 0.248 s, 4 passes 0.559 s) | 0.061 s (0.675 s, 0.857 s) | 1.70 | **Not met** (81 errors in both) |
| `drc check; drc catchup`, all-cells block | ≤ 1.10 × Magic | 1.83 s (3.18 s, 8.67 s) | 1.37 s (2.21 s, 6.32 s) | 1.34 | **Not met** (1292 errors in both) |

The job showed green on PR #29 only because the benchmark step piped into `tee` under the runner's default `bash -e` (no pipefail), which dropped `drc_bench.py`'s exit status; round 2b sets `shell: bash` on those steps (regression check `scripts/tests/test_workflows.py`), so the job now fails until the gap closes. It does on PR #30 (run 42947046848, a slower runner): array 0.14 s vs 0.12 s, block 2.35 s vs 1.99 s (about 1.18× for both), start latency median 3.3 ms, max 6.8 ms, identical counts. The array's Magic times include the container start of each run (0.68–0.90 s for one pass), so its ratio is noisier than the block's; the block's 1.34 is consistent across the three runs.

Gap plan: Hefesto is at parity on Apple arm64 (Clang) and 1.2–1.7× slower on the x86_64 runner (GCC 14). Next steps, in order: (1) profile the block run on x86_64 split into basic check, interaction flattening and commit time per square; (2) compare the GCC and Clang builds of the edge loop on the same runner, to separate compiler from algorithm; (3) the parallel mode of D16 (independent squares on several workers) if a single-thread gap remains. Needs a decision on whether M3 exits with this gap recorded or waits for the fix (see the round-2b PR).

## Remaining gaps (open items)

- Raven (to be proposed upstream, not patched): a system notification API (Notification Center, freedesktop) with a click action; submenus in in-window menus; an anchored popover widget (M2 gap, again for `drc why`); per-row accessible descriptions and tree rows in Table.
- DESIGN: the M3-DRC-Status "Off" note says edits still mark areas while DRC is off; Magic's CmdPaint (and Hefesto's) does not mark under `drc off`, so the stale state does not appear for such edits (Magic's erase, CmdE.c, marks either way). Board text to be corrected; the chip's Off tooltip, which said the same, was corrected in round 2b (`drc_models_test.cpp`).
- SHELL (next round): the canvas tag "#k why-text" on the found tile; the Prev/Next stepper over the canvas instead of in the panel; highlighting the group row of the found error when the group is collapsed.
- The browser's list is DRCWhyAll called directly (not a logged `select top cell; drc listall why`), to avoid moving the user's box and selection on every auto-refresh; user actions remain commands. Confirm with the user if the board's wording ("built from select top cell + drc listall why") must be literal.
