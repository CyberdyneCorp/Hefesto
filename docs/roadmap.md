# Hefesto Roadmap

Hefesto is built desktop-first (decision P-11):

- **Desktop app:** Linux and macOS, written in C++20 on the Raven GUI library. It is built first.
- **iPadOS app:** Swift, SwiftUI and Metal on the same C++20 core. It starts only after the desktop core works and is well tested.

Every capability in `openspec/specs/` is assigned to exactly one milestone. `scripts/check_roadmap_coverage.py` enforces this in CI. A capability may already be used before its milestone, but its milestone is where it must be complete and verified.

Sizes are given in requirements, not dates. Each milestone's work is planned as OpenSpec changes (`/opsx:propose`), with a `design.md` for architecture decisions.

## Principles

1. **The core never depends on the UI.** Raven is confined to the desktop shell (`hefesto-architecture`). The CLI and every test run headless.
2. **Verification drives each milestone.** Before work on a milestone starts:
   - Its open questions are resolved (`Resolution:` lines, per `hefesto-magic-compat-mode`; an entry whose `Status:` is pending user confirmation is not resolved yet). `scripts/check_milestone_readiness.py --milestone M<n>` lists the entries its capabilities cite that are still unresolved; CI runs it with `--through M1`, the gate for starting development.
   - Its golden cases are captured from the reference tools: Magic 8.3.694 @ fc5daa9e, netgen @ 93dcda14, ngspice-47 @ a80f6e3e (the engine's pin, Q-SIM-7 and Q-SIM-23; the M0 image built master @ 7aa47164 until M9 task 3.2 moves it), xschem @ 4142e6d6 (for the schematic cases).
3. **Exit criteria are the same for every milestone:**
   - The area's golden cases pass under the canonical comparison of `hefesto-output-determinism`.
   - Unit, property and fuzz tests are green (`hefesto-testing-strategy`).
   - Output is byte-identical on Linux x86_64, Linux aarch64 and macOS arm64.
   - `openspec validate --all --strict` passes.
4. **Risky questions are settled early,** in the milestone that first depends on them, not at release time. See the schedule at the end of this document.

## Overview

| Milestone | Theme | Capabilities | Requirements | Release |
|---|---|---|---|---|
| M0 | Foundations and Raven go/no-go | 8 | 41 |  |
| M1 | Core model, formats, PDK manager, 2D viewer | 25 | 284 |  |
| M2 | Interactive editing and design system | 19 | 194 |  |
| M3 | Design rule checking | 5 | 72 |  |
| M4 | Scripting and PDK integration | 8 | 94 | **Desktop Alpha** |
| M5 | Mask generation, GDS, CIF | 8 | 203 |  |
| M6 | LEF/DEF and OASIS | 9 | 215 |  |
| M7 | Extraction and netlisting | 21 | 207 |  |
| M8 | Schematic capture (xschem-compatible) | 6 | 101 |  |
| M9 | LVS, simulation and advanced analog simulation | 17 | 184 | **Desktop Beta** |
| M10 | 3D, plotting, diff, routing, EM/IR, digital flow | 14 | 150 |  |
| M11 | AI layout agent | 9 | 36 |  |
| M12 | Legacy tools | 8 | 102 |  |
| M13 | Tapeout, plugins, operations and Desktop 1.0 hardening | 11 | 106 | **Desktop 1.0** |
| M14 | iPadOS app (feature scope plus iPad parts of earlier capabilities) | 1 | 4 | **iPad 1.0** |

## M0 — Foundations and Raven go/no-go

**Goal.** Set up the repository, the build and CI, and a headless core. Prove that Raven can host a high-performance layout canvas. Build the capture setup for expected outputs.

**Status (2026-11-10):** merged (OpenSpec change `m0-foundations`). Build, core and CLI, desktop shell and canvas spike, golden tooling and CI run on every Linux and macOS leg. The Raven go/no-go is final: **GO** (user decision 2026-11-10, M2 task 11.4), on the Apple M3 Pro measurements and the Linux lavapipe CI job; the reference M1 frame-rate run, Linux X11/Wayland live input and Orca are M4 confirmation checks (see `docs/decisions/raven-go-no-go.md`). M0 tasks 3.7 and 6.3 are closed with it; 3.6 (the M1 Mac and Linux spike runs) moves to M4, and 3.3 stays open.

**Capabilities:** `hefesto-platform-targets`, `hefesto-architecture`, `hefesto-build-and-distribution`, `hefesto-licensing`, `hefesto-diagnostics`, `hefesto-testing-strategy`, `hefesto-output-determinism`, `hefesto-magic-compat-mode`

**Deliverables**
- **Repository and build:** CMake 3.25 presets for Linux and macOS, a CI matrix (Linux x86_64/aarch64 with GCC and Clang, macOS arm64), dependencies pinned (Raven commit, zlib), and the Apache-2.0 headers.
- **Core and CLI skeletons:** a core library and a `hefesto-cli` skeleton, with the command-API dispatcher and structured diagnostics.
- **Desktop shell skeleton:** a C++20 wrapper over Raven's C API, and a main window with dock area, panels, a command bar and a canvas viewport.
- **Canvas spike:** Hefesto's own Vulkan renderer drawing 10M rectangles inside a Raven viewport, on Linux and on macOS through MoltenVK. This is measured against the targets in `hefesto-performance`.
- **Golden capture setup:** reproducible headless builds of Magic, netgen and ngspice in a pinned container; a capture runner; the canonical comparators; one end-to-end golden case passing.

**Exit criteria**
- The common exit criteria, for this milestone's capabilities.
- **Raven go/no-go.** This is decided by:
  - canvas frame rate on both platforms
  - viewport embedding working
  - text and theming
  - accessibility hooks
  - API stability risk

  If the decision is no-go, only the desktop shell is replaced. The core is unaffected.

## M1 — Core model, formats and 2D viewer

**Goal.** Open real PDK layouts (for example the sky130 standard cells) and display them with Magic-identical styles. Save them byte-identical.

**Capabilities:** `tech-file-format`, `tech-layers-and-planes`, `tech-contacts-and-stacking`, `tech-compose-and-connect`, `tech-styles-section`, `tech-loading-and-scaling`, `tech-command`, `layout-geometry-model`, `layout-paint-semantics`, `layout-hierarchy`, `layout-labels-ports`, `layout-properties`, `cell-file-format`, `cell-library-resolution`, `cell-io-safety`, `cell-timestamps`, `hefesto-mag-extensions`, `display-style-files`, `hefesto-rendering`, `ui-windows-and-views`, `font-labels`, `app-startup`, `hefesto-settings`, `hefesto-files-and-pdks` (desktop parts), `hefesto-pdk-manager`

**Deliverables**
- **Loading:** load tech files, including the PDK tech files.
- **Data model:** a corner-stitched data model that reproduces Magic's tile order (X-1).
- **`.mag` files:** read and write, with hierarchy, labels and properties, plus library path resolution and PDK discovery.
- **Viewer:** a 2D viewer with zoom and pan, expand and unexpand, layer styles from `.dstyle`/`.cmap` (OpenGL variant, X-5), and font labels.
- **PDK manager:** browse, download, verify and install sky130, gf180mcu and IHP sg13g2, with the version pinned per project.
- **Startup:** startup and settings layers, including execution of `.magicrc` files once M4 adds Tcl. Until then only the non-script settings apply.

**Exit criteria**
- The common exit criteria.
- The golden cases TECH, CELL, MODEL and the viewer-related UI cases pass.
- Round-tripping the sky130 standard cell library gives byte-identical files.

**Status (complete, 2026-11-09; change `openspec/changes/m1-core-viewer`, all tasks ticked).** Technology files, the corner-stitched database, `.mag` read and write (with crash backups, archives and locks), display styles and fonts, the Vulkan renderer, the core view commands, the PDK manager (in-process HTTPS downloads with progress, updates, offline packs), settings and the shared startup are implemented and tested. Golden cases: all 142 are captured from Magic and replay identically or differ only where an approved decision says Hefesto deliberately differs (`tests/golden/deviations.toml`). All 441 `sky130_fd_sc_hd` cells round-trip byte-identically. CI is green on every leg (Linux GCC/Clang x64 and arm64, ASan+UBSan, libFuzzer, determinism, macOS oldest/latest, lavapipe GPU readback). M1 requirement coverage is 246 of 247 (GDS font attributes move to M5). The desktop UI was reviewed with the user (task 8.5). The Raven go/no-go of M0 (task 6.3) stays provisional until the reference-hardware check. **Design-system pull-forward:** M1 builds the desktop Desktop-Layout screen on the forge tokens, Inter and JetBrains Mono, status shapes and PDK layer colors (`hefesto-design-system`, decision P-23), ahead of M2; gaps are recorded in `docs/design/m1-ui-conformance.md`.

## M2 — Interactive editing and design system

**Goal.** Make Hefesto a usable editor, with Magic's full editing command set and a modern desktop UI.

**Pull-forward:** the `ui-gui-managers` requirement "Tool modes" (the `tool` command) is implemented in M2 because the M2 tools need it; the rest of `ui-gui-managers` stays in M4.

**Status (complete, 2026-11-10; all tasks ticked):** OpenSpec change `openspec/changes/m2-interactive-editing`, three implementation rounds done (progress in its `tasks.md`). The editing command set, layout undo, selection and connectivity, hierarchy, labels, command dispatch (Magic's Tcl-build name rules, Q-UI-41), macros and tools, highlights and feedback, the color window and the wiring tool are implemented in the core and unit-tested. The desktop routes canvas input through Magic's macro tables and has a live tool rail, selection inspector, layer palette paint layer and menus, command history and completion popovers, undo/redo with an undo history, the color window screen, a key-binding sheet, docking and F6 focus cycling, and prompt dialogs. Golden: 264 cases, 226 pass, 38 differ only by an approved decision or a later milestone's feature (`deferred`), 0 fail. Requirement coverage is 163 of 163. Decisions of 2026-11-10: the board behaviors (Paint/Label/Measure tools, dialog buttons, panel layout and F6, undo history) are confirmed and specified in `hefesto-input-and-touch` and `hefesto-app-ux`; the Raven go/no-go is final (GO), with the M1 frame-rate run, Linux X11/Wayland input and Orca moved to M4 as confirmation checks; the shell gaps listed under M4 "Carried from M2" move to M4. The usability review was done and approved with the user (`docs/design/m2-ux-review.md`), including a VoiceOver pass; the M2 boards stay in `docs/design/mockups/`, now the mockups' source of truth.

**Capabilities:** `cmd-coordinates-and-units`, `cmd-box`, `cmd-paint-erase`, `cmd-selection`, `cmd-hierarchy`, `cmd-labels-properties`, `cmd-misc-edit`, `layout-undo`, `layout-connectivity`, `ui-command-dispatch`, `ui-tools-and-input`, `ui-feedback-and-highlights`, `ui-box-stack-and-measure`, `ui-color-window`, `ui-wizard-and-debug`, `hefesto-input-and-touch` (desktop), `hefesto-app-ux`, `hefesto-design-system`, `wiring-tool`

**Deliverables**
- **Editing:** box tool, paint and erase, selection (chunk, region, net), copy, move and stretch, transforms, arrays, cell hierarchy editing, labels and ports, properties, and undo/redo.
- **Commands:** macros and key bindings (Magic default set), the command bar with history and completion, and the wiring tool.
- **Design system:** the forge tokens, icons and component patterns (`hefesto-design-system`), implemented as a Raven theme.
- **Desktop UI:** layer palette, hierarchy browser, property inspector, status bar, dockable panels, light and dark themes.

**Exit criteria**
- The common exit criteria.
- The CMDEDIT golden cases and the UI command cases pass.
- An internal usability review of the M2 UX design has been done (`docs/design/m2-ux-review.md`).

## M3 — Design rule checking

**Goal.** Run continuous and on-demand DRC whose errors are identical to Magic's.

**Status (complete, 2026-11-11):** OpenSpec change `openspec/changes/m3-drc`, rounds 1, 2a, 2b and 3; all 68 tasks of its `tasks.md` are done. The rule compiler, check kernel, engine with the background checker (worker thread under the shared database lock, TSan-tested), the `drc` command set and mask hints are implemented and unit-tested; M3 requirement coverage is 69 of 69, and M0–M3 pass the readiness gate. The CLI hosts the background checker at Magic's interactive prompt and the desktop app hosts it with a live DRC status chip, a DRC results browser and `drc why` on the canvas (`docs/design/m3-ui-conformance.md`; boards approved 2026-11-10). Golden: all 120 DRC cases are captured (the 113 of `tests/golden/DRC/CASES.md`, five rule-printing and parse cases, and the two standard-cell corpus cases); every one passes except DRC-CIF-4..7, deferred to M5 as below (DRC-CIF-2 and DRC-CIF-3 already match, because Magic reports no mask-layer error on them), and DRC-CMD-1, which differs only by the approved fix Q-DRC-22. The sky130 `sky130_fd_sc_hd` and gf180mcu `gf180mcu_fd_sc_mcu7t5v0` cells give Magic's error sets exactly. Decisions of 2026-11-10: Q-MODEL-32 and Q-MODEL-33 (compat), Q-DRC-24, Q-DRC-25, Q-MISC-14 and Q-MISC-15 (fixes). DRC throughput: single-threaded, Hefesto takes 0.72–0.78× Magic's time on the x86_64 CI runner and 0.65–0.69× on Apple arm64, with identical error counts (`docs/design/m3-ui-conformance.md` "Performance"). Since the user decision of 2026-11-12 the DRC throughput target is judged by callgrind instructions per pass of the all-cells block on the x86_64 runner, not by wall time: no more than the M3 baseline of 12.14 billion + 2 % (12.38 billion), gated by the `drc-profile` job of `perf.yml` (pull requests touching the DRC paths, main, the schedule and dispatch); the timed ratio to Magic is kept as information.

**Capabilities:** `drc-rule-language`, `drc-rule-semantics`, `drc-engine`, `drc-commands`, `mask-hints`

**Deliverables**
- **DRC engine:** every rule type, background checking without blocking the UI, interaction checks across the hierarchy, and the `drc` command set.
- **DRC results browser:** steps through errors in `drc find` order.

**Exit criteria**
- The common exit criteria.
- All 113 DRC golden cases pass, except the six mask-layer check cases DRC-CIF-2..7, which pass in M5 (they need mask-layer generation).
- The sky130 and gf180mcu standard cells are DRC-clean exactly where Magic reports them clean, with identical error sets elsewhere.

## M4 — Scripting and PDK integration → Desktop Alpha

**Goal.** Run real PDK flows: PDK startup scripts, device generators (PCells) and the PDK's menus.

**Status (complete, 2026-11-12; the user's Desktop Alpha sign-off and the manual checks are outstanding):** OpenSpec change `openspec/changes/m4-scripting`: three implementation rounds, the closing round (PR #38), the final round (PR #39) and the polish round (PR #40: workstreams decide, carry, screens). Every exit criterion that does not need a person is met (exit table in `tasks.md` 13.6):

| Criterion | Result | Evidence |
|---|---|---|
| Unit, property and fuzz tests green on every leg | met | CI 43287048713: Linux x86_64 GCC 11/latest and Clang 14/latest, Linux aarch64 GCC and Clang, macOS arm64 latest and Xcode oldest, ASan/UBSan, TSan, lavapipe, fuzz (60 s per target), `pcell-equivalence`; macOS presets debug, asan, bench, xcode locally (2728/2728) |
| Golden cases pass (Magic 8.3.694) | met | 496 captured: 433 pass, 63 differ only where an approved decision or a later milestone says so; all 90 SCRIPT cases committed (72 pass, 18 recorded); the three Desktop Alpha cases pass on every leg; golden-capture verify 43287048743 |
| Output byte-identical on Linux x86_64, Linux aarch64, macOS arm64 | met | determinism job of CI 43287048713 with `--require-area SCRIPT --require-area DESKTOP` |
| M4 requirement coverage | met | 57 of 57 (`mazeroute`'s requirement deferred to M10, Q-SCRIPT-30) |
| Open questions resolved; readiness | met | M0–M4 ready; `--milestone M5` reports M5's 105 blocking entries |
| `openspec validate --all --strict` | met | 174 passed (OpenSpec 43287049001) |
| DRC throughput (instruction count, decision 2026-11-12) | met | 12.25 billion instructions per block pass ≤ 12.38 (M3 12.14 + 2 %); Magic 10.98 (1.12 ×); instruction gate in Performance 43287048659; timed ratio (information) 0.76 × block, 0.74 × array, counts equal 1314/81 (Performance 43272005414) |
| M2 carry-over (D17) | met, except 11.1 | 11.2–11.8 captured on macOS and compared with the M2 boards; 11.1 is the reference-hardware and Linux live-input checks (manual) |
| Desktop Alpha checklist on Linux and macOS | **user** | not signed off; see below |

Decided 2026-11-12 and applied: Q-CELL-45 and Q-SCRIPT-47 (compat), Q-SCRIPT-45 with the native sg13g2 fixed devices, the DRC slowdown bisected and fixed (sky130 mask hints copied quadratically in the DRC yank); in the polish round Q-SCRIPT-48 (a confirmation that cannot be asked goes ahead with magic-compat ON, as Magic does, and is declined with it OFF), Q-CELL-46 (`cellname list childinst` in Magic's id-hash order ON, placement order OFF) and Q-CELL-47 (the `save` file-name answer cut to 7 characters ON); recorded deviations and deferrals SCRIPT-API-01 and -PDK-09 (Q-SCRIPT-49), -API-20 (`extract`, M7), -GUI-05 (Q-SCRIPT-50), -GUI-12/-13 (Q-SCRIPT-51), the ON-mode refused-load text (Q-SCRIPT-52), DRC-PDK-3 with IHP's `cap_cmim` (Q-DRC-26); the DRC throughput target is the instruction count of a block pass, gated by `perf.yml` `drc-profile`.

Outstanding, none of it machine-checkable here:
- The user's Desktop Alpha sign-off on Linux and macOS (13.6), with the real-pointer and keyboard checks of the M2 carry-over (the captures used Raven's injected events) and the Linux captures (CI has no display).
- The Raven confirmation checks (11.1): the reference M1 frame-rate run, Linux discrete-GPU frame rate, X11/Wayland live input, Orca, an audible VoiceOver run; and filing Raven issues 1–14 (`docs/decisions/raven-go-no-go.md`).
- The App Review inquiry (2.2), drafted for the user to submit.
- Board differences recorded in `docs/design/m4-ui-conformance.md` "SCREENS polish pass" and `m2-ui-conformance.md` "M4 polish round" (Raven issues 9–14, control density): the user decides whether they are acceptable or built to the board later. Tasks 10.1–10.4 and 10.6 stay open on them; 9.5 waits for `gds read` (M5); 2.3 lacks a `linux-debug`/`linux-bench` run.

**Capabilities:** `hefesto-scripting`, `scripting-api`, `scripting-pdk-integration`, `pcell-toolkit`, `readspice`, `scripting-auxiliary-tools`, `ui-gui-managers`, `hefesto-security`

**Deliverables**
- **Tcl:** an embedded, sandboxed Tcl (X-3) with the `magic::` namespace and tag callbacks.
- **PDK support:** the PDK `.magicrc` files and device scripts for sky130, gf180mcu and IHP sg13g2.
- **PCells:** generator dialogs, and native C++ generators validated against the Tcl ones.
- **Security:** consent before running PDK scripts.

**Desktop Alpha:** open, edit, DRC-check and place PCells in real PDK designs on Linux and macOS.

**Carried from M2** (user decision 2026-11-10; details in `docs/design/m2-ui-conformance.md` and the notes of m2-interactive-editing tasks 10.1 and 10.3-10.8):
- Raven confirmation checks (`docs/decisions/raven-go-no-go.md`, "Decision (2026-11-10)"): the frame-rate run on the reference M1 Mac and on Linux with a discrete GPU (M0 task 3.6), live mouse, keyboard and trackpad input on Linux under X11 and Wayland, Orca on Linux, and filing the Raven issues upstream (function-key text, key repeat, custom-widget accessibility press and state, anchored popover).
- Floating panels and drag-to-dock, with panel headers, the drop-zone highlight and collapsed edge strips (the shell moved onto Raven's dock area).
- The panel layout (dock, size, collapsed, floating) saved per user in the settings and restored at launch.
- The modal key-binding sheet: key capture, a conflict warning with Replace, Reset table and Load file.
- The label tool's text entry as a popover on the canvas (needs an anchored popover in Raven); Escape after a measure issuing `unmeasure`; the Paint tool disabled until a paint layer is chosen.
- Undo steps named after the user command that made them (a per-step command label in the undo log), and the undo history on press-and-hold of Undo.
- A per-area minimap update instead of the deferred full rebuild.
- A pixel-width box and selection outline in `render::highlight_batches` (Magic's 2-pixel sides).
- The wiring tracking HUD, the "Wiring · tracking" mode chip and the Wire inspector.
- The capture of the crash-recovery dialog.
- VoiceOver press on custom-drawn controls (tool rail, layer rows), once Raven exposes an accessible press action.

**Early de-risking (start here):** submit the App Review inquiry about running sandboxed Tcl on iPadOS (X-3 risk). The answer decides whether the M14 iPad app runs user-imported PDK scripts or curated packs.

## M5 — Mask generation, GDS and CIF

**Capabilities:** `mask-output-styles`, `mask-output-operators`, `mask-hierarchical-generation`, `mask-input-styles`, `gds-read`, `gds-write`, `cif-format-io`, `drc-cif-rules`

Depends on `mask-hints`, which is completed in M3.

**Deliverables**
- **Mask generation:** cifoutput and cifinput styles with every operator.
- **GDSII:** read and write, including compressed files, read-only vendor views and content-hash timestamps.
- **CIF:** read and write.
- **DRC:** rules that run on generated mask layers.

**Exit criteria**
- The common exit criteria.
- The MASK golden cases pass.
- The DRC mask-layer check cases DRC-CIF-2..7, deferred from M3, pass.
- GDS output is byte-identical to Magic's, apart from the masked time fields.
- Every sky130, gf180mcu and IHP library GDS round-trips.

**Status (complete, 2026-11-24; change `openspec/changes/archive/2026-11-24-m5-mask-gds-cif`, archived by the M5 close-out, PR #107; round 7 integration on branch `m5-round7`).** Mask generation (cifoutput and cifinput styles with every operator), `gds read`/`gds write` (compressed files, read-only vendor views, content-hash timestamps), `cif read`/`cif write` and the mask-layer DRC rules are implemented; the desktop app has Read GDS / Write GDS with a progress card for long reads and writes. Every MASK golden case and every library round trip now matches Magic or carries an approved deviation, and the M5 exit gate (`m5_cases.py --exit-check`) runs in CI and passes. The user approved the round 7 decisions on 2026-11-17 (Q-MASK-418, Q-MASK-419 and Q-MASK-420 compat, Q-MODEL-37 fix), and every exit criterion below is met at 788b187f (task 15.6). The close-out ticked tasks 15.1 (every macOS preset built and tested with -Werror) and 15.7 (documentation, with the `gds-time-zone` setting of Q-MASK-132).

| Criterion | Result | Evidence |
|---|---|---|
| Unit, property and fuzz tests green on every leg | met at 788b187f | CI 44230386061 (788b187f, `m5-round7`): every Linux and macOS leg, GCC 11 and latest with -Werror, ASan/UBSan, TSan, fuzz (60 s per target), lavapipe, pcell-equivalence, and the lint job's M5 exit gate |
| MASK golden cases pass or carry an approved deviation | met at 788b187f | no case is deferred to M5 and `m5_cases.py --exit-check` exits 0 (lint job of CI 44230386061); MASK-OP-021 (Q-MASK-417), MASK-RT-sky130_fd_io-f01/-f09 (Q-MASK-418) and MASK-RT-gf180mcu_fd_io-f02 (Q-MODEL-37) replay under approved decisions. Local replay at 788b187f with the PDK and Magic reference data: 672 pass, 0 fail, 77 deviations, 3 skipped (the standard-cell GDS sets, replayed in CI) |
| DRC-CIF-2..7 pass | met | DRC-CIF-1..8 replay on every leg |
| GDS byte-identical to Magic apart from masked time fields | met at 788b187f | every GW case and every round trip; fix deviations MASK-GW-003 (Q-MASK-201), GW-006 (Q-MASK-209), GW-039 (Q-MASK-219), GW-005 (Q-MASK-208); golden-capture 44230393953 (verify) green |
| Every sky130, gf180mcu and IHP library GDS round-trips | met at 788b187f | Performance 44230390136 (788b187f): `gds-roundtrip` with no case deferred: sky130 8/8, sky130-io 9/9, sky130-sram 4/4, gf180mcu 32/32, gf180mcu-io 3/3, ihp 2/2, ihp-io 1/1, ihp-sram 30/30; golden-capture 44230393953 (verify) green on every capture and round-trip shard |
| Determinism covers MASK | met at 788b187f | determinism job of CI 44230386061 with `--require-area MASK` |
| M5 requirement coverage | met | 194 of 194 |
| `openspec validate --all --strict` | met | 177 passed |
| `perf.yml` green and the equivalence report reviewed | met at 788b187f | Performance 44230390136: `gds-bench`, `drc-profile`, `drc-bench`, `golden-slow` and every `gds-roundtrip` shard. Equivalence reports reviewed (task 15.6): 88 of 89 files round-trip twice (`sky130_ef_io__gpiov2_pad_wrapped.gds` uses a cell defined in another file), 3522 layer and 244 label differences between the two writes, no structure or SREF difference, the same as at 280d99c5; expected by design D19 |

## M6 — LEF/DEF and OASIS

**Capabilities:** `lef-tech-section`, `lef-read`, `lef-write`, `def-read`, `def-write`, `oasis-format`, `oasis-read`, `oasis-write`, `oasis-command`

**Deliverables**
- **LEF/DEF:** abstract views, and DEF import and export for place-and-route flows.
- **OASIS:** read and write, equivalent to the GDS path, and validated with KLayout (`strm2oas`, `strmxor`).
- **Pulled forward from M7:** the `.ext` read-and-flatten subset that `def write` calls, built to `ext-flattening-read`, `ext-flattening-hierarchy` and `ext-flattening-names` (the DEF-write net cases take Magic-written `.ext` files as inputs), and the device, antenna tie-down, substrate and active-type parts of the `extract-tech-section` reader that `lef write` and `def write` read. Both are built in M7's area layout with M7's tags, and M7 extends them (`openspec/changes/archive/2026-11-22-m6-lef-def-oasis` Open Questions 1 and 2, user decision 2026-11-18). Extraction itself stays in M7.

**Exit criteria**
- The common exit criteria.
- The LEFDEF and OASIS golden cases pass. OASIS-012, OASIS-030 and OASIS-061 run without commercial-tool inputs, under an approved deviation (Q-OASIS-2, user decision 2026-11-18).
- The 419-byte OASIS reference file is confirmed by a third-party reader.

**Status (complete, 2026-11-22; change `openspec/changes/archive/2026-11-22-m6-lef-def-oasis`, archived by the M6 close-out, PR #84; round 6 exit-round integration in PR #83).** The tech `lef` section, `lef read`/`write`/`writeall`, `def read`/`write`, the OASIS codec, `oasis read`/`write` with read-only vendor views and GDS transcoding, the `.ext` read-and-flatten subset and the desktop open of OASIS files are implemented, and every decision M6 cites is resolved (the user's decisions of 2026-11-21, G1..G5, are recorded as Q-LEFDEF-139/141/151 and Q-OASIS-14/55/56). On 2026-11-22 the user deferred MASK-OS-016 past M6 as a known Magic crash (Magic exits with SIGSEGV on it, so it cannot be captured; Q-LEFDEF-152, deferred to M13) and accepted the unit and desktop coverage of OASIS-092, which has no headless entry point (Q-OASIS-92, tasks 13.2 and 14.1). `m6_cases.py --exit-check` exits 0 and every exit criterion is met (task 17.7):

| Criterion | Result | Evidence |
|---|---|---|
| Unit, property and fuzz tests green on every leg | met | CI 44987472406 (46d90d0d): every leg, both macOS presets, ASan, TSan, fuzz and the golden replay legs |
| LEFDEF and OASIS golden cases pass or carry an approved deviation; `m6_cases.py --exit-check` has no entry deferred to M6 | met | `--exit-check` exits 0 (M6 close-out, PR #84): MASK-OS-016 (Magic SIGSEGV, capture_blocked) is deferred to M13 (Q-LEFDEF-152) and OASIS-092 is covered by unit and desktop tests (Q-OASIS-92), both user decisions of 2026-11-22. Recaptured in golden-capture 44971808830 and stable in verify 44973981552: OASIS-020 and OASIS-021-* (approved Q-OASIS-56 deviation, 45 cells) and LEFDEF-STARTUP-lef (replays). LEFDEF-128 replays and LEFDEF-232 differs only through Q-LEFDEF-1, after the message-order fix of `lef write` and `def read -annotate`. OASIS-012/030/061 carry Q-OASIS-2. Local replay: LEFDEF 116 pass, 28 deviation, 4 skip, 0 fail |
| `oasis-validate`: no `strmxor` difference, no strict warning for OASIS-051/057/060/061/062 | met | run 44987468409 (46d90d0d): 229 checks, 0 failures, labels cases included |
| 419-byte reference file confirmed (task 17.6) | met | run 44987468409: KLayout 0.30.0 reads it with no warning, `strmcmp` identical, gdstk 1.0.2 reads it and validates its signature |
| Determinism covers LEFDEF and OASIS | met | determinism job of CI 44987472406 with `--require-area LEFDEF --require-area OASIS`, every leg identical |
| M6 requirement coverage | met | 146 of 146 exact and behavioural; 208 of 208 with native |
| `check_milestone_readiness.py --milestone M7` | met | M7: ready |
| `openspec validate --all --strict` | met | 179 passed |
| `perf.yml` green, `lefdef-bench` and `oasis-bench` within baseline + 2 % | met | baselines recorded from Performance dispatch 44971811678 (m6-round6, e3ff89d0); dispatch 44976052953 (c7bfb1ee) green, every workload within its gate |
| Every result from a run at the head declared done | met | the code results are from the round-6 heads named above; the close-out (PR #84) changes only golden tooling, decisions and documentation, and its CI run is green |

## M7 — Extraction and netlisting

**Status (2026-11-26): in progress (implementation round 5 integrated).** Every decision row raised before round 5 is approved (`docs/decisions/m7-decision-table.md`); round 5 raised nine more (Q-EXTRACT-42/43, Q-NETLIST-70..76) and one amendment (Q-NETLIST-69), each implemented with its recommended option and pending the user. Requirement coverage is 100 % for all 21 capabilities. `extract length`, the `extresist` geometry, FastHenry and box runs, EX-064..070, LEFDEF-336, NL-RES-01..08 and every NL-RESNET case replay; `CORE-NOTIMPL-008` is retired. The four M7 fuzz targets, the security review (M7-S1..S8, every finding fixed or accepted) and the EXTRACT and NETLIST determinism areas have landed. `m7_cases.py --exit-check` still reports work left: the six PDK standard-cell cases (task 17.4) and NETLIST-SMOKE-01. Open: the benches (18.x), the desktop Extract button (16.1), the EXTTECH dispatch table (4.1, 4.7, 4.8, after M10's TECH change) and the catalogue audit (17.2, 17.7).

**Capabilities:** `extract-tech-section`, `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy`, `extract-substrate`, `extract-file-format`, `extract-commands`, `ext-flattening-read`, `ext-flattening-hierarchy`, `ext-flattening-names`, `netlist-spice-options`, `netlist-spice-devices`, `netlist-spice-hierarchy`, `netlist-sim`, `netlist-extcheck`, `extresist-command`, `extresist-network`, `extresist-output`, `antenna-check`

**Deliverables**
- **Extraction:** hierarchical extraction with parasitics (including the fringe-halo model) and device recognition.
- **Netlists:** ext2spice in all five dialects, and ext2sim.
- **R-C extraction:** full R-C extraction with extresist.
- **Antenna:** antenna checks.
- **Extract tech section:** `extract-tech-section` starts from the subset that M10's 3D view writes first, in that capability's home with its tags (style, scale, units, `planeorder`, `noplaneordering`, `height`; `openspec/changes/m10-3d-view` design D5, Open Question 4, user decision 2026-11-13). M7 extends it; there is one extract reader.
- **Pulled forward to M6:** M6 builds the `.ext` read-and-flatten subset (`ext-flattening-read`, `-hierarchy`, `-names`) that `def write` calls, and the device, antenna tie-down, substrate and active-type parts of the `extract-tech-section` reader, in this milestone's area layout and with its tags (`openspec/changes/archive/2026-11-22-m6-lef-def-oasis` design D10, user decision 2026-11-18). M7 extends that code rather than replacing it; those requirements stay M7's in the coverage report.

**Exit criteria**
- The common exit criteria.
- The EXTRACT and NETLIST golden cases pass.
- `.ext` and SPICE output matches Magic's after canonicalization on the PDK standard cells.

## M8 — Schematic capture

**Goal.** Draw schematics in an editor that is compatible with xschem, using the PDKs' symbol libraries. Schematics then drive simulation and LVS.

**Status (2026-11-18): planned; golden capture done, implementation not started.** The 24 open SCHEM questions and D-21 were approved on 2026-11-13 (`docs/decisions/m8-decision-table.md`): xschem quirks are governed by the `xschem-compat` setting (default ON), and the specs follow a two-team clean-room process (`hefesto-licensing`). The plan is OpenSpec change `openspec/changes/m8-schematic`. Done: the xschem PDK data sets and the inventory (section 2), the clean-room check and pull-request template (tasks 1.3, 1.4), and the 41 SCHEM golden cases, captured from xschem 3.4.8RC with gawk in runs 44239093096 and 44239987461, with the capture-fixed texts recorded in the specs (gate G2, task 3.6). Decided 2026-11-18: Q-SCHEM-30 (fix: relative symbol references resolve against the containing schematic's folder in both modes, with a warning; SCHEM-PR-05 has an approved deviation), the Q-SCHEM-21 OFF-mode amendment (a template LF followed by `+` is kept), and the SCHEM-SIM-01/02 simulation results move to M9 (both cases deferred to M9, captures kept). Open: gate G1 needs the outside reviewer's confirmation of `docs/contributing/cleanroom.md` (task 1.1) and the second spec-team signature of the paraphrase review (task 1.2). Implementation starts after G1.

**Status (2026-11-26): in progress (implementation rounds 1 to 6).** File format, symbols and libraries (with the clean-room system symbol library), SPICE netlisting through the session environment and the `schematic`/`xschem` commands have landed with their input limits (Q-SCHEM-32, -40, -41, -42); round 4 added schematic editing (task 9.1, 9.3, most of 9.2), the read-only desktop Schematic tab with its view models (13.1, part of 13.2), `schem-bench` (14.1) and the install-root `CAD_ROOT` default (Q-SCHEM-45). Round 5 added object order and the clipboard, reload/close state clearing, interruptible netlisting, the schematic deck and launchers (11.1), the M9 deferral of the rest of simulation integration (11.2), the security sign-off of the script paths (12.1), off-UI-thread symbol resolution and the desktop panels, and 30 SCHEM golden cases replaying their file outputs. Round 6 implemented Q-SCHEM-34 to -37 (approved 2026-11-25) in both modes, embedded `embed=true` symbols on save, the PDK netlist digests on three platforms (8.6), SCHEM stdout/stderr comparison after the Q-SCHEM-46 capture filter (31 cases replayed), the Netlist button, system clipboard and `macro schematic` key table of the Schematic tab, and spec text for the net driver check and the xschem subcommands the remaining golden cases use. Open: the round-7 implementation of that spec text, the CORE generator and flat-netlist bugs of SCHEM-NL-05/21/23, the 13.2 screenshot comparison, and Q-SCHEM-52 to -54, -60 to -62, -66 and -67, which await the user; the M8 exit check (14.3, design D21) lists the blockers. Progress per task: `openspec/changes/m8-schematic/tasks.md`.

**Capabilities:** `schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration`, `schematic-command`

**Deliverables**
- **Files and libraries:** read and write xschem `.sch`/`.sym` files, and the PDK symbol libraries for sky130, gf180mcu and IHP. Implemented clean-room, because xschem is GPL.
- **Editing:** wires, instances, labels, buses, hierarchy and net highlighting, with desktop input.
- **Netlisting:** SPICE netlists compatible with xschem's, in simulation mode and in LVS mode.
- **Testbenches:** testbench schematics, ready for the embedded simulator in M9.

**Exit criteria**
- The common exit criteria.
- The SCHEM golden cases pass, including round trips of every PDK `.sch`/`.sym` file and netlists matching xschem's for the PDK example schematics. SCHEM-SIM-01/02 are deferred to M9 (their simulation results need the embedded simulator; decided 2026-11-18), as is SCHEM-LVS-01 (its LVS result needs the `lvs-*` capabilities; Q-SCHEM-39, decided 2026-11-22) except its `inv_1.spice` netlist output, which the M8 exit checks (`deferred_outputs`, `replay.py --milestone M8`), and SCHEM-PR-05 differs only by the approved fix Q-SCHEM-30.
- A `hefesto-security` review signs off the schematic script permission model (Q-SCHEM-25).

## M9 — LVS, simulation and advanced analog simulation → Desktop Beta

**Status (2026-11-22): plan drafted; every decision approved by the user on 2026-11-22 (G0 recorded on branch `m9-round0-decide`).**
- The plan is OpenSpec change [`openspec/changes/m9-lvs-simulation`](../openspec/changes/m9-lvs-simulation/README.md).
- Its decisions are in [`docs/decisions/m9-decision-table.md`](decisions/m9-decision-table.md). They cover:
  - the 34 entries that `check_milestone_readiness.py --milestone M9` reports as blocking;
  - Q-SIM-1 and the rest of Q-SIMADV-5;
  - audit decisions D-22 to D-28;
  - 18 entries the plan proposes (Q-LVS-30 to -36, Q-SIM-20 to -28, Q-SIMADV-13, Q-SCHEM-39);
  - four planning questions.
- Recommended direction:
  - LVS is clean-room under a two-team process (Q-LVS-30).
  - ngspice runs in the worker `hefesto-simd`, which alone loads a shared `libngspice-hefesto`. That worker is the licence and crash boundary on desktop (Q-SIM-20, -21).
  - D-28 is split: the desktop part is decided in M9, the iPad part before M14 (Q-SIM-26).
  - Desktop Beta is signed off on one inverter flow per PDK (Q-SIM-27).
- Implementation starts after gate G0. The four verify rows (Q-SIM-8, Q-SIMADV-3, -4, -9) are approved as conditional rules; their `Verified` lines come from task 3.3.

**Capabilities:** `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`, `simulation-engine-embedding`, `simulation-netlist-preparation`, `simulation-analyses-and-results`, `simulation-ui-and-crossprobe`, `simulation-command`, `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`

**Deliverables**
- **LVS:** built-in LVS compatible with netgen, implemented clean-room from the specs. Reports in `comp.out` and JSON formats, and cross-probing between the report and the layout.
- **Simulation:** embedded ngspice in a worker process, a waveform viewer, and probing from the layout.
- **Schematic simulation results (from M8):** the golden cases SCHEM-SIM-01 (IHP `dc_lv_nmos.sch` simulation results) and SCHEM-SIM-02 (sky130 `test_inv.sch` operating-point annotation), deferred from M8 on 2026-11-18, pass.
- **Schematic simulation integration (from M8, `m8-schematic` design D13):** running the schematic deck (`schem::build_deck`, its validation hand-off and capture points), operating-point annotation, embedded graphs, waveform and schematic-to-layout cross-probing, the schematic-driven LVS command and results interchange with xschem (`schematic-simulation-integration`).
- **Advanced analog simulation:** sweeps, corner matrix, Monte Carlo and mismatch with reproducible seeds, measurements, specs and yield statistics.

**Desktop Beta:** the complete flow (schematic → simulation → layout → DRC → GDS → extraction → LVS → post-layout simulation) on Linux and macOS.

**Early de-risking:** legal review of how ngspice's LGPL (KLU, numparam) and MPL (OSDI) parts are linked. This must be decided before M14 (Q-SIM). The desktop linking question (Q-SIM-20: a replaceable shared `libngspice-hefesto` loaded only by the sandboxed worker `hefesto-simd`) was decided in M9 round R0 on 2026-11-22 and goes through the same legal review before the Desktop Beta; the iPadOS deadline (before M14) is unchanged (Q-SIM-26, D-28).

## M10 — 3D, plotting, diff, routing, EM/IR, digital flow

**Status (2026-11-26): 3D view phase 1 complete and the phase 2 features landed (exit check 14.2 pending), phase 3 (overlays) waits for M7 and M9 results (change `openspec/changes/m10-3d-view`; rounds 3, 4 and 5 merged in PR #86, PR #102 and PR #122, round 6 in review); plotting and routing are planned (below); the other M10 workstreams are not started.**
- P1 (layout-layer 3D view, `wind3d` and Magic's 3D commands, the desktop 3D tab, picking and net highlighting) meets its exit criteria (design D19): `view-3d` and `hefesto-3d-visualization` P1 requirements covered 7 of 7, MISC-3D-01..16 replay (MISC-3D-11 is the documented Q-MISC-73 deviation), the 3D tab works on Linux (X11, Wayland) and macOS, and 1M rectangles draw at about 46 fps at level 1 on the M3 Pro (gate ≥ 30 fps).
- Round 4 (2026-11-24, branch `m10-3d-round4`): the desktop 3D tab uses the board look in both modes (Q-MISC-84) with non-conducting area layers ghosted by default (Q-MISC-85); tilted views no longer show the stack from below; bottom toolbar strip added (tasks 16.1-16.10).
- Round 5 (2026-11-25, branch `m10-3d-round5`): P1 leftovers done (accessibility pass, floating toolbar, fit at 1280 px, Release frame rates on the M3 Pro); P2 started: mask-layer source in the core and the desktop tab, cross-sections with the clipping plane and the A–A′ panel, `export3d` (glTF, GLB, STL; byte-identical digests on Linux x86_64, Linux aarch64 and macOS arm64), the board's substrate slab (tasks 9.6, 10.1-10.3, 11.1-11.5, 12.5, 13.3, 16.11).
- Round 6 (2026-11-26, branch `m10-3d-round6`): decisions of 2026-11-25 and 2026-11-26 recorded (Q-MISC-86..89); P2 finished: mask-layer net overlay (10.4), "Span connected layers" as window state so `export3d` exports spanned, mask generation errors once per build with magic-compat ON, the mask-layer board view in proportion with magic-compat ON, coplanar faces drawn without z-fighting, the Export format menu, "Detach view" (the "Open in separate window" item), the round's accessibility pass; P2 features landed, exit check 14.2 pending ("Detach view" awaits the user's review and the idle-machine 13.2 run is outstanding).
- Open: P3 overlays (15.x, after M7 extraction and M9 results), the M1 and reference-hardware frame-rate runs (follow-ups of task 13.2, which is done for the M3 Pro gate), the 64 B/rect memory target (kept at the 170 B/rect gate, decision M2).
- Next: P3 (overlays) after M7 extraction and M9 results.

**Plotting and routing (2026-11-24): planned, decisions pending (change `openspec/changes/m10-plot-route`, decision table `docs/decisions/m10-plot-route-decision-table.md`).**
- Scope: `plot-output` (PostScript and PNM byte-identical to Magic, Hefesto's SVG), view export to SVG and PNG in `hefesto-rendering` (D-37, D-38), `router-maze` and `router-maze-iroute` (a session-scoped engine the M11 agent can call), and `mazeroute` deferred from M4 (Q-SCRIPT-30).
- Gate G0: 19 blocking entries for these capabilities, D-37 and D-38, and 22 new entries (Q-MISC-48..55, Q-LEGACY-37..49, Q-LEGACY-54). Golden cases MISC-PLOT-01..20 and LEG-MZ-01..31 are captured before implementation.
- Collaboration and diff, the reliability capabilities and the digital flow are later M10 changes; they are not started.

**Capabilities:** `view-3d`, `hefesto-3d-visualization`, `plot-output`, `hefesto-collaboration-and-diff`, `router-maze`, `router-maze-iroute`, `digital-flow-block-import`, `digital-flow-run`, `digital-flow-results`, `digital-flow-mixed-signal`, `reliability-em-rules`, `reliability-ir-drop`, `reliability-em-check`, `reliability-visualization`

**Deliverables**
- **3D view:** parity with Magic's 3D view, plus interactive 3D inspection (cross-sections, net highlighting, picking linked to the 2D view), glTF and STL export, and parasitic and field overlays.
- **Plotting:** PostScript, PNM and SVG.
- **Collaboration:** structural and visual diff, the git diff and merge helpers, and sync.
- **Routing:** the maze router and `iroute`. These are prerequisites for the agent in M11.
- **Reliability:** electromigration and IR-drop analysis from extresist networks and simulation currents, shown as 2D and 3D heat maps.
- **Digital flow (desktop-only):** import hardened blocks, run LibreLane as an external process, browse results, and place digital blocks into mixed-signal layouts.

## M11 — AI layout agent

**Capabilities:** `agent-architecture`, `agent-model-providers`, `agent-safety-and-approval`, `agent-natural-language-commands`, `agent-device-and-cell-generation`, `agent-drc-lvs-fix-loop`, `agent-schematic-driven-layout`, `agent-evaluation`, `simulation-optimization-hooks`

**Deliverables**
- **Agent:** the agent with pluggable providers, disabled by default.
- **Approval:** the propose-and-approve flow with visual diff.
- **Abilities:** all four (natural-language commands, device and cell generation, the DRC/LVS fix loop, schematic-driven layout).
- **Benchmarks:** the benchmark suite with CI replay.

**Exit criteria**
- The common exit criteria.
- A qualified provider meets the pass-rate threshold on the AGENT benchmark.
- The open questions Q-AGENT-1 (analog routing) and Q-AGENT-5 (confidential PDKs) are resolved.

## M12 — Legacy tools

**Capabilities:** `router-channel`, `router-gate-array`, `router-net2ir`, `plow`, `netlist-window`, `irsim-interface`, `scheme-interpreter`, `openaccess`

**Deliverables**
- **Routers and plow:** the channel router and gate-array router, `net2ir`, and plow.
- **Netlists and simulation:** the netlist window, and IRSIM on desktop.
- **Informative-only items:** the Scheme interpreter and OpenAccess are specified as informative. Implement them only if users need them.

## M13 — Tapeout, plugins, operations and Desktop 1.0

**Capabilities:** `hefesto-performance`, `tapeout-fill-and-density`, `tapeout-seal-ring`, `tapeout-signoff-checklist`, `tapeout-shuttle-packages`, `hefesto-onboarding-and-help`, `hefesto-product-operations`, `plugin-api`, `plugin-packaging-and-distribution`, `plugin-security-and-platforms`, `plugin-sdk`

**Deliverables**
- **Tapeout:** fill generation and density checks, seal rings, the sign-off checklist, and export packages for shuttles (Tiny Tapeout, ChipFoundry, wafer.space, IHP). The KLayout sign-off approach follows decision P-18 (external KLayout on desktop; resolves Q-TAPEOUT-1).
- **Plugins:** a stable plugin C ABI with a C++20 SDK, plus signed packaging and distribution.
- **Operations:** opt-in crash reporting and telemetry, signed update channels, and published system requirements.
- **Onboarding:** the new-project wizard, templates and example designs per PDK, interactive tutorials, and command help.
- **Performance:** the targets met on the reference workloads. Batch runs no slower than Magic.
- **Accessibility and localization:** completed.
- **Packaging:** an AppImage and a native Linux package, and a signed, notarized macOS app with MoltenVK bundled.
- **Release gates:** license and acknowledgements checks. Every open question is resolved for the shipped scope.
- **Clean-room counsel review (exit item):** legal counsel has reviewed the two-team clean-room processes of the schematic editor (xschem) and of LVS (netgen, Q-LVS-30) and the system-library interface table, and the review is recorded (J-1, Q-SCHEM-10, Q-LVS-30; capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source", scenario "Counsel review before Desktop 1.0"; early de-risking schedule, "Before Desktop 1.0"). Desktop 1.0 does not ship the xschem-compatible system symbol library without it.

**Desktop 1.0:** release on Linux and macOS, ready to take a design to a shuttle.

## M14 — iPadOS app → iPad 1.0

Starts after Desktop 1.0, or earlier only if the desktop core's golden suite has been stable for a full milestone (P-11).

**Capabilities:** `hefesto-ipad-feature-scope`

**Scope:** the feature set of `hefesto-ipad-feature-scope`. It is delivered as the iPadOS parts of capabilities that are already complete:
- `hefesto-architecture`: the iPadOS shell, in Swift and SwiftUI, using Swift/C++ interoperability
- `hefesto-rendering`: the Metal backend
- `hefesto-input-and-touch`: touch and Pencil, with mode-based gestures (X-5)
- `hefesto-files-and-pdks`: the sandboxed PDK store and security-scoped documents
- `hefesto-scripting`: Tcl inside the iPadOS sandbox, or curated packs, depending on App Review
- `simulation-engine-embedding`: in-process ngspice with OSDI and code models compiled in
- `agent-model-providers`: the offline provider
- `hefesto-app-ux`: SwiftUI panels, scenes and Stage Manager
- `hefesto-settings`: iCloud sync
- `schematic-editing`: touch and Pencil schematic editing
- `hefesto-pdk-manager`: sandboxed PDK installation, the only way to get PDKs on iPadOS

Desktop-only at first: the digital flow, the legacy tools, `shell` and git integration.

**Exit criteria**
- The full golden suite passes in the iPadOS simulator and on device, byte-identical to Linux (`hefesto-testing-strategy`).
- The `hefesto-performance` targets for iPad are met: frame rate and memory.
- App Review approval.

## Early de-risking schedule

| When | Item | Decides |
|---|---|---|
| M0 | Raven canvas spike (Linux and MoltenVK) | Whether Raven is used for the desktop shell |
| M0 | Golden capture setup for Magic | Whether the X-6 verification approach works at all |
| M1 entry | Resolve open questions TECH, CELL, MODEL | Starting M1 work on settled behavior |
| M4 | App Review inquiry on sandboxed Tcl | iPad scripting: user-imported scripts or curated packs (X-3) |
| M5 | ~~KLayout sign-off decision (Q-TAPEOUT-1)~~ Done: decided by P-18 (2026-11-06) | How gf180mcu and IHP sign-off DRC, fill and density are run on desktop and on iPad |
| M8 | SCHEM golden capture from xschem 3.4.8RC with gawk | The exact texts of Q-SCHEM-14, -21, -22, -27 and -28, and the start of M8 implementation |
| M9 | ngspice LGPL/MPL linking review | How the iPad simulation engine is built |
| M11 | Analog routing evaluation | Whether to add a dedicated analog router (Q-AGENT-1) |
| Before Desktop 1.0 | Counsel review of the two-team clean-room process for xschem and for netgen (LVS, Q-LVS-30) and of the system-library interface table (J-1, Q-SCHEM-10); an M13 exit item | Whether the xschem- and netgen-derived specs and the symbol interface may ship |
| Before the M14 iPad scope freeze | Usability test of finger vs Pencil wire drawing (Q-SCHEM-24) | The final iPad schematic gesture set |
