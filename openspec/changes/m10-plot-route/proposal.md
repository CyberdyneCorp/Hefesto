# Proposal: M10 plotting and maze routing (plot-output, router-maze, router-maze-iroute)

> **Status: plan drafted 2026-11-24. No decision is approved yet.** `check_milestone_readiness.py --milestone M10` reports 19 blocking entries for these three capabilities (Q-MISC-41..44; Q-LEGACY-6, -31..33, -85..89, -91..96). Audit decisions D-37 and D-38 ("blocks M10") are open. This plan raises 20 new entries (Q-MISC-48..55, Q-LEGACY-37..48) and six judgement items. All of them, each with a recommended option, are in `docs/decisions/m10-plot-route-decision-table.md`. Nothing in sections 2–15 of `tasks.md` starts before gate G0 (design D1).

## Why

The roadmap's M10 lists "Plotting: PostScript, PNM and SVG" and "Routing: the maze router and `iroute`. These are prerequisites for the agent in M11." The 3D view (`openspec/changes/m10-3d-view`) is the first M10 workstream and is in progress; this change plans the next two. They are independent of the 3D view and of each other, so they can run in parallel with it.

Routing matters beyond Magic parity: M11's agent tools wrap `iroute` (`agent-architecture`, `agent-schematic-driven-layout`) and must run on iPadOS. Plotting matters for the desktop's image export and for the reliability heat-map export, which audit item D-37 currently leaves without an owner.

What exists today (main at 54fbc6d4):
- **Specs.** `plot-output` (14 requirements, 12 exact or behavioral), `router-maze` (20, 19 exact) and `router-maze-iroute` (10, 8 exact or behavioral) are written against Magic 8.3.694. The `mazeroute` requirement of `scripting-auxiliary-tools` was deferred to M10 by Q-SCRIPT-30.
- **Technology.** The `plot` and `mzrouter` sections are registered and kept as raw lines (`tech/tech_file.cpp:46`, `:53`; `technology.hpp:80-93`). Nothing reads them.
- **Commands.** `plot`, `iroute`, `*iroute` and `*mzroute` are not registered; only their Magic help strings exist (`ui/magic_help.cpp`). The UI-G-005 deviation names them as "M10". `mazeroute` is registered and answers `TOOLS-MAZE-001` (`tools/commands.cpp`); SCRIPT-AUX-02 is deferred to M10.
- **Display.** `display::build_scene` and the style-file readers (`display/scene.hpp`, `display/styles.hpp`) give the scene and colours a plot or export needs. There is no SVG, PNG or PostScript writer in the tree. zlib is already built.
- **PDKs.** sky130A, gf180mcuD and ihp-sg13g2 have an empty `mzrouter` section, so in Magic `iroute` refuses on all three; their `plot` sections have only `style pnm`. Only scmos (golden reference) has an `irouter` style and PostScript styles.
- **Golden.** MISC-PLOT-01..12 and LEG-MZ-01..20 are planned in `CASES.md`, none authored or captured. The capture image builds Magic without Cairo, so MISC-PLOT-12 (SVG) cannot be captured there. No PostScript, PNM or SVG comparator exists.

## What Changes

- **Decisions first (DECIDE).** The 19 blocking entries, the 20 new ones, D-37/D-38 and six judgement items are presented with a recommendation each; the approved answers become `Resolution:` lines and the spec deltas of this change are synced into the living specs at G0.
- **Golden capture (GOLDEN, before implementation).** MISC-PLOT-01..20 and LEG-MZ-01..31 authored and captured from Magic 8.3.694 in CI; SCRIPT-AUX-02 re-captured; three new comparators (`ps_text`, `pnm_image`, `svg_shape`); a `magic-cairo` capture variant for the SVG case and the `plot help` listing; an optional `plot-validate` mode running Ghostscript and resvg as CI-only subprocesses.
- **Plot technology and command (PLOTTECH, PLOTCMD).** The `plot` section reader (PostScript and PNM styles; the raster devices accepted and ignored, Q-MISC-51), the `plot` command with Magic's option set plus `svg`, the plotted region, the plot parameters, and atomic output files (Q-MISC-53).
- **PostScript (PS).** A byte-exact port of Magic's writer with the embedded `magicps.pro`, text escaping (Q-MISC-42), the zero-area fix (Q-MISC-48) and the compat rows Q-MISC-41 and -43.
- **PNM (PNM).** A byte-exact port of Magic's rasteriser with the bundled `mos.7bit.mraster` colour files, a committed Lanczos table for cross-platform identity (Q-MISC-54), and the memory fixes (Q-MISC-49).
- **View export (EXPORT).** SVG and PNG export of a layout view from the scene model, backend-free and headless, owned by `hefesto-rendering` (D-37, D-38); `plot svg` uses it, and so will the reliability heat maps and the desktop export.
- **Maze router technology (MZTECH).** The `mzrouter` section reader with whole-statement validation (Q-LEGACY-31, -39), DRC-derived widths and spacings, and, with `magic-compat` OFF, an `irouter` style derived from the technology's `lef` section for PDKs that have none (Q-LEGACY-37).
- **Maze engine (MZ).** A port of Magic's maze router that keeps its visit order (blockage planes, estimation, windowed bloom search, heaps, path to paint), with no global state, a structured result, a cancellation token, a deterministic bloom budget and a search-memory limit, so that M11's agent can call it from scratch sessions (Q-LEGACY-41, -48).
- **`iroute` (IROUTE).** Every subcommand with Magic's texts and formats, `saveParameters`, endpoints with a non-blocking prompt rule (Q-LEGACY-40), `-timeout` on the cancellation token, results painted as one undo group, and the `*iroute`/`*mzroute` diagnostics.
- **`mazeroute` (MAZETOOL).** The M4-deferred tool, replacing `TOOLS-MAZE-001`; the maze helper as a desktop panel.
- **Desktop (DESIGN, SHELL).** File > "Export Image…" (PostScript, PNM, SVG, PNG) and the maze helper panel, boards first.
- **Exit (PERF, INTEGRATION).** `plot-bench` and `route-bench` against Magic, fuzz targets for every new reader, notices, the close-out of the M4 and UI-G-005 deferrals, and the exit check.

### Non-goals

- The rest of M10, each a later M10 change: `hefesto-collaboration-and-diff` (structural and visual diff, git helpers, sync); the reliability capabilities `reliability-em-rules`, `reliability-ir-drop`, `reliability-em-check` and `reliability-visualization` (this change only provides the PNG export they will use, and re-points one sentence of `reliability-visualization` to it); the digital flow `digital-flow-block-import`, `digital-flow-run`, `digital-flow-results` and `digital-flow-mixed-signal`. The 3D view is its own change, `m10-3d-view`.
- The M12 routers and tools: `router-channel`, `router-gate-array` (its maze stems use this change's engine when M12 comes), `router-net2ir` and `netlist-window`.
- Raster printer devices (Versatec, HP RTL/HPGL2, gremlin, pixels) and printer spooling (Q-MISC-51, Q-MISC-44).
- Analog routing constraints and the agent's routing tools (M11, Q-AGENT-1). M10 provides the engine interface they need (design D23).
- The iPadOS screens of the export dialog (M14); M10 builds their view models.
- Pixel identity with Magic's Cairo SVG (Q-MISC-52).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

This change implements the existing requirements of `plot-output`, `router-maze` and `router-maze-iroute`, and the deferred "Netlist maze-routing script" of `scripting-auxiliary-tools`. Like M9, it carries spec deltas, because its decisions are not approved yet; they are synced into the living specs at G0 (design Open Question 1).

The deltas under `specs/` record the changes the recommended answers imply:
- `plot-output`: MODIFIED "Plot command and plotted region", "Plot parameters" and "Raster printer and legacy plot devices" (Hefesto's device set, Q-MISC-51), "PostScript labels" (escaping, Q-MISC-42), "SVG plot" (native export, headless size, Q-MISC-52) and "Platform mechanism of plot output" (embedded prologue, bundled colour files, no spooling, Q-MISC-44); ADDED "Plot behaviour with magic-compat off" (Q-MISC-41, -43, -55), "Plot output safety and limits" (Q-MISC-48, -49, -50, -53, -54) and "Plot export from the desktop".
- `router-maze`: MODIFIED "mzrouter section grammar" (whole-statement validation, Q-LEGACY-31, -39; the `width` rule, Q-LEGACY-33); ADDED "Maze router behaviour with magic-compat off" (Q-LEGACY-33, -85, -87, -89), "Maze router engine interface" (J4, Q-LEGACY-41) and "Maze search safety and limits" (Q-LEGACY-38, -47, -48, -90).
- `router-maze-iroute`: MODIFIED "iroute availability and dispatch" (Q-LEGACY-37) and "Route context and endpoints" (Q-LEGACY-40); ADDED "Derived irouter style" (Q-LEGACY-37), "Route timeout" (Q-LEGACY-41) and "iroute behaviour with magic-compat off" (Q-LEGACY-42, -43, -44, -45).
- `hefesto-rendering`: ADDED "View export to SVG and PNG" (D-37, D-38).
- `hefesto-performance`: ADDED "Plot and maze-route throughput".
- `reliability-visualization`: MODIFIED "Touch and platform parity", whose PNG export now goes through `hefesto-rendering` (D-37).

## Impact

- **New code (core):** `src/core/{include/hefesto,src}/plot/{tech,params,cmd,io,ps,pnm}/`, `display/export/`, `route/{tech,maze,iroute,diag}/`, `tools/mazeroute.cpp`; `tests/unit/{plot,route}/`, `tests/unit/display/export/`; `tests/fuzz/{fuzz_plot_section,fuzz_pnm_cmap_dstyle,fuzz_mzrouter_section,fuzz_iroute_commands,fuzz_maze_route}.cpp`.
- **New code (desktop):** `src/shell/desktop/src/{plot_export,maze_helper}_*.cpp`.
- **Data:** `assets/plot/magicps.pro`, `assets/plot/mos.7bit.mraster.cmap`, `assets/plot/mos.7bit.mraster_dstyle` (Magic, UC Berkeley notice, unmodified); `plot/pnm/lanczos_table.inc` (generated, committed).
- **Shared changes:** `tech::Technology` gains the parsed `plot` and `mzrouter` sections; `EditorState` gains the plot parameters and the `iroute` session; `tech/loader.cpp` hands the raw lines over; `tests/golden/deviations.toml` (UI-G-005, SCRIPT-AUX-02, two new deviations); `scripts/check_requirement_coverage.py` (M4 deferral removed); `tools/` retires `TOOLS-MAZE-001`.
- **Tooling:** three comparators, `tools/golden/authoring/legacy_cases.py`, `plottest.tech`, `tools/golden/gen_lanczos_table.py`, the `magic-cairo` image variant, the `plot-validate` mode, `bench/{plot,route}/`.
- **Docs:** `docs/diagnostics/codes/{plot,route}.md`, `docs/design/m10-plot-route-ui-conformance.md`, `docs/contributing/`, roadmap status, CHANGELOG, `THIRD_PARTY_NOTICES.md`.

## Dependencies

| Needs | From |
|---|---|
| Decisions approved, deltas synced | DECIDE (section 1), gate G0 |
| MISC-PLOT captured | GOLDEN (section 2), gate G1-PLOT, before the plot writers |
| LEG-MZ and SCRIPT-AUX-02 captured, verify rows settled | GOLDEN (section 2), gate G1-ROUTE, before the engine and routes |
| Technology loader, DRC rule tables, `lef` section reader | M1, M3, M6 (exist) |
| Display scene and style files, font rasteriser | M1 (exist) |
| Layer-list parsing, window lookup, prompt host, undo, selection | M1, M2, M4 (exist) |
| zlib | exists (`hefesto::zlib`) |
| Nothing from the 3D view | — (design D1) |
| Consumers | M11 agent tools (design D23); the later reliability change (PNG export); M12 gate-array maze stems |
