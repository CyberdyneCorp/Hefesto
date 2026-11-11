# Hefesto Roadmap

Hefesto is built desktop-first (decision P-11):

- **Desktop app:** Linux and macOS, written in C++20 on the Raven GUI library. It is built first.
- **iPadOS app:** Swift, SwiftUI and Metal on the same C++20 core. It starts only after the desktop core works and is well tested.

Every capability in `openspec/specs/` is assigned to exactly one milestone. `scripts/check_roadmap_coverage.py` enforces this in CI. A capability may already be used before its milestone, but its milestone is where it must be complete and verified.

Sizes are given in requirements, not dates. Each milestone's work is planned as OpenSpec changes (`/opsx:propose`), with a `design.md` for architecture decisions.

## Principles

1. **The core never depends on the UI.** Raven is confined to the desktop shell (`hefesto-architecture`). The CLI and every test run headless.
2. **Verification drives each milestone.** Before work on a milestone starts:
   - Its open questions are resolved (`Resolution:` lines, per `hefesto-magic-compat-mode`). `scripts/check_milestone_readiness.py --milestone M<n>` lists the entries its capabilities cite that are still unresolved; CI runs it with `--through M1`, the gate for starting development.
   - Its golden cases are captured from the reference tools: Magic 8.3.694 @ fc5daa9e, netgen @ 93dcda14, ngspice @ 7aa47164, xschem @ 4142e6d6 (for the schematic cases).
3. **Exit criteria are the same for every milestone:**
   - The area's golden cases pass under the canonical comparison of `hefesto-output-determinism`.
   - Unit, property and fuzz tests are green (`hefesto-testing-strategy`).
   - Output is byte-identical on Linux x86_64, Linux aarch64 and macOS arm64.
   - `openspec validate --all --strict` passes.
4. **Risky questions are settled early,** in the milestone that first depends on them, not at release time. See the schedule at the end of this document.

## Overview

| Milestone | Theme | Capabilities | Requirements | Release |
|---|---|---|---|---|
| M0 | Foundations and Raven go/no-go | 8 | 39 |  |
| M1 | Core model, formats, PDK manager, 2D viewer | 25 | 284 |  |
| M2 | Interactive editing and design system | 19 | 194 |  |
| M3 | Design rule checking | 5 | 72 |  |
| M4 | Scripting and PDK integration | 8 | 90 | **Desktop Alpha** |
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

**Status (complete, 2026-11-11):** OpenSpec change `openspec/changes/m3-drc`, rounds 1, 2a, 2b and 3; all 68 tasks of its `tasks.md` are done. The rule compiler, check kernel, engine with the background checker (worker thread under the shared database lock, TSan-tested), the `drc` command set and mask hints are implemented and unit-tested; M3 requirement coverage is 69 of 69, and M0–M3 pass the readiness gate. The CLI hosts the background checker at Magic's interactive prompt and the desktop app hosts it with a live DRC status chip, a DRC results browser and `drc why` on the canvas (`docs/design/m3-ui-conformance.md`; boards approved 2026-11-10). Golden: all 120 DRC cases are captured (the 113 of `tests/golden/DRC/CASES.md`, five rule-printing and parse cases, and the two standard-cell corpus cases); every one passes except DRC-CIF-4..7, deferred to M5 as below (DRC-CIF-2 and DRC-CIF-3 already match, because Magic reports no mask-layer error on them), and DRC-CMD-1, which differs only by the approved fix Q-DRC-22. The sky130 `sky130_fd_sc_hd` and gf180mcu `gf180mcu_fd_sc_mcu7t5v0` cells give Magic's error sets exactly. Decisions of 2026-11-10: Q-MODEL-32 and Q-MODEL-33 (compat), Q-DRC-24, Q-DRC-25, Q-MISC-14 and Q-MISC-15 (fixes). DRC throughput: single-threaded, Hefesto takes 0.72–0.78× Magic's time on the x86_64 CI runner and 0.65–0.69× on Apple arm64, with identical error counts; the `drc-bench` job holds it within 1.10× (`docs/design/m3-ui-conformance.md` "Performance").

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

## M6 — LEF/DEF and OASIS

**Capabilities:** `lef-tech-section`, `lef-read`, `lef-write`, `def-read`, `def-write`, `oasis-format`, `oasis-read`, `oasis-write`, `oasis-command`

**Deliverables**
- **LEF/DEF:** abstract views, and DEF import and export for place-and-route flows.
- **OASIS:** read and write, equivalent to the GDS path, and validated with KLayout (`strm2oas`, `strmxor`).

**Exit criteria**
- The common exit criteria.
- The LEFDEF and OASIS golden cases pass.
- The 419-byte OASIS reference file is confirmed by a third-party reader.

## M7 — Extraction and netlisting

**Capabilities:** `extract-tech-section`, `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy`, `extract-substrate`, `extract-file-format`, `extract-commands`, `ext-flattening-read`, `ext-flattening-hierarchy`, `ext-flattening-names`, `netlist-spice-options`, `netlist-spice-devices`, `netlist-spice-hierarchy`, `netlist-sim`, `netlist-extcheck`, `extresist-command`, `extresist-network`, `extresist-output`, `antenna-check`

**Deliverables**
- **Extraction:** hierarchical extraction with parasitics (including the fringe-halo model) and device recognition.
- **Netlists:** ext2spice in all five dialects, and ext2sim.
- **R-C extraction:** full R-C extraction with extresist.
- **Antenna:** antenna checks.

**Exit criteria**
- The common exit criteria.
- The EXTRACT and NETLIST golden cases pass.
- `.ext` and SPICE output matches Magic's after canonicalization on the PDK standard cells.

## M8 — Schematic capture

**Goal.** Draw schematics in an editor that is compatible with xschem, using the PDKs' symbol libraries. Schematics then drive simulation and LVS.

**Capabilities:** `schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration`, `schematic-command`

**Deliverables**
- **Files and libraries:** read and write xschem `.sch`/`.sym` files, and the PDK symbol libraries for sky130, gf180mcu and IHP. Implemented clean-room, because xschem is GPL.
- **Editing:** wires, instances, labels, buses, hierarchy and net highlighting, with desktop input.
- **Netlisting:** SPICE netlists compatible with xschem's, in simulation mode and in LVS mode.
- **Testbenches:** testbench schematics, ready for the embedded simulator in M9.

**Exit criteria**
- The common exit criteria.
- The SCHEM golden cases pass, including round trips of every PDK `.sch`/`.sym` file and netlists matching xschem's for the PDK example schematics.

## M9 — LVS, simulation and advanced analog simulation → Desktop Beta

**Capabilities:** `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`, `simulation-engine-embedding`, `simulation-netlist-preparation`, `simulation-analyses-and-results`, `simulation-ui-and-crossprobe`, `simulation-command`, `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`

**Deliverables**
- **LVS:** built-in LVS compatible with netgen, implemented clean-room from the specs. Reports in `comp.out` and JSON formats, and cross-probing between the report and the layout.
- **Simulation:** embedded ngspice in a worker process, a waveform viewer, and probing from the layout.
- **Advanced analog simulation:** sweeps, corner matrix, Monte Carlo and mismatch with reproducible seeds, measurements, specs and yield statistics.

**Desktop Beta:** the complete flow (schematic → simulation → layout → DRC → GDS → extraction → LVS → post-layout simulation) on Linux and macOS.

**Early de-risking:** legal review of how ngspice's LGPL (KLU, numparam) and MPL (OSDI) parts are linked. This must be decided before M14 (Q-SIM).

## M10 — 3D, plotting, diff, routing, EM/IR, digital flow

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
| M9 | ngspice LGPL/MPL linking review | How the iPad simulation engine is built |
| M11 | Analog routing evaluation | Whether to add a dedicated analog router (Q-AGENT-1) |
