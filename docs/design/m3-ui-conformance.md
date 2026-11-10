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

Owner of this section: DESIGN (task 2.1). `DRC-Browser.dc.html` updated with the M3 states and the new `M3-DRC-Status.dc.html`, rendered in both themes to `docs/images/ui/m3-*.png`. Each SHELL screen state of section 9 maps to a board:

| SHELL state (tasks.md section 9) | Board | Status |
|---|---|---|
| Top-bar DRC chip: not run, checking, clean, N errors, off | `M3-DRC-Status` | todo |
| Status-bar DRC state (same five states) | `M3-DRC-Status` | todo |
| Browser: empty, running, stale, populated | `DRC-Browser` | todo |
| Errors grouped by why-text with counts; `#k of n` stepping | `DRC-Browser` | todo |
| Error view on the canvas; `drc why` popover | `DRC-Browser` | todo |
| Ignored rules; style selector | `DRC-Browser` | todo |
| Later-milestone elements (Fix with agent M11, nets M7, measured vs required, sign-off M13) disabled or hidden | `DRC-Browser` | todo |

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

| Measurement | Target | Hefesto | Magic | Notes |
|---|---|---|---|---|
| Background start latency after a paint (100 × 100 `sky130_fd_sc_hd__dfxtp_1` array) | ≤ 100 ms | | — | |
| `drc check; drc catchup`, dfxtp_1 array, single-threaded | no slower than Magic | | | |
| `drc check; drc catchup`, all-cells block, single-threaded | no slower than Magic | | | |
| Pan and zoom frame times while the worker checks the all-cells block | interactive (`hefesto-architecture` "Concurrency model") | | — | SHELL, task 9.1 |

## Remaining gaps (open items)

None recorded yet.
