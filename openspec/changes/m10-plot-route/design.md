# Design: M10 plotting and maze routing (plot-output, router-maze, router-maze-iroute)

> Drafted 2026-11-24 against origin/main 54fbc6d4. **No decision this design cites is approved yet.** Every choice is drafted as the recommended option of `docs/decisions/m10-plot-route-decision-table.md`. If the user picks another option, the D-numbers listed in "Decisions this design depends on" are revised before gate G0 passes.

## Context

See proposal.md for the motivation and the current state of the code. The conventions of M0–M9 still hold:
- SPDX headers.
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`.
- Per-area diagnostic tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`.
- Cognitive complexity of at most 15 per function in the core and 8–12 in the shell view models, measured with the cognitive-complexity skill. The technology-section readers may go to 25 where a statement grammar is one switch; irreducible functions are flagged in the pull request.
- No UI, GPU, Raven or process headers in the core (`scripts/check_core_includes.py`).
- Golden expected outputs are captured only in CI, from Magic 8.3.694 at fc5daa9e.
- Golden cases run with `magic-compat` ON; every OFF behaviour is unit-tested, one test per mode.
- Crashes, undefined behaviour and hangs are fixed in both modes (X-2 tier 1).

What is particular to this change:

1. **Two independent streams.** Plotting (`plot-output`) and maze routing (`router-maze`, `router-maze-iroute`) share no code beyond the technology loader and the command layer. They are planned together because both are small next to the 3D view and both are M10 deliverables that later work needs: plotting for the reliability heat-map export (D-37) and the desktop's image export, routing for the M11 agent (roadmap "These are prerequisites for the agent in M11"). Each stream has its own gates (D1), so neither waits for the other.
2. **Two specs written to exact visit order.** `router-maze` marks 19 of 20 requirements exact, and its Purpose says the search's visit order must be copied to get identical routes. The engine is therefore a port of Magic's search, data structures included where they decide order (corner-stitched blockage planes, the heaps, the point hash), not a new router. Native improvements go only where the `magic-compat` rule allows: the OFF branch of four compat rows (D5) and the derived style (D13).
3. **No supported PDK can route today.** sky130A, gf180mcuD and ihp-sg13g2 ship an empty `mzrouter` section, so in Magic `iroute` and `mazeroute` refuse on all three (Q-LEGACY-37). The agent needs routing on these PDKs, which is why D13 derives a style with `magic-compat` OFF.
4. **Two exact binary formats.** PostScript and PNM are `exact`: every byte is compared with Magic's. The risks are single-precision formatting, the libm-dependent Lanczos table and FMA contraction (Q-MISC-54, D9, D20).
5. **One display-dependent format.** Magic writes SVG only through Cairo. Hefesto has no Cairo (P-3) and writes SVG itself from the scene model the renderer draws (D10).

Magic's code in this area (read-only, fc5daa9e; nothing under `plot/`, `irouter/`, `mzrouter/` or `tcltk/mazeroute.tcl` changed after it):
- `plot/` (about 6 300 lines): `plotCmd.c`, `plotMain.c`, `plotPS.c`, `plotPNM.c`; `plotVers.c`, `plotHP.c`, `plotGremln.c` and `plotPixels.c` are compiled out of the default build (Q-MISC-51).
- `mzrouter/` (about 12 000 lines) and `irouter/` (about 4 000 lines).
- `magic/magicps.pro` (72 lines, the PostScript prologue) and the `mos.7bit.mraster.*` colour files in `scmos/`.
- `tcltk/mazeroute.tcl` (675 lines, `mazeroute` and the maze helper window).

## Goals / Non-Goals

**Goals:**
- Every requirement of `plot-output` (14), `router-maze` (20) and `router-maze-iroute` (10) after this change's deltas, and the `mazeroute` requirement of `scripting-auxiliary-tools` deferred from M4 (Q-SCRIPT-30).
- MISC-PLOT and LEG-MZ goldens byte-identical to Magic under the canonical comparison, `magic-compat` ON.
- PostScript, PNM, SVG and PNG output byte-identical on Linux x86_64, Linux aarch64 and macOS arm64.
- One branch point per compat row, each tested in both modes.
- A router engine with no global state and a structured result, ready for M11's typed tools (D4, D23).
- `iroute` usable on the three supported PDKs with `magic-compat` OFF (D13).
- Retire `TOOLS-MAZE-001`, the SCRIPT-AUX-02 deferral, and the M10 names of the UI-G-005 deviation reason.

**Non-Goals:** the items listed under "Non-goals" in proposal.md.

## Decisions

### D1. Rounds and gates

| Gate | Passes when | Unlocks |
|---|---|---|
| G0 decisions | The decision table is approved; the answers are recorded as `Resolution:` lines (the two verify rows, Q-LEGACY-40 and Q-LEGACY-44, as conditional resolutions naming the option for each capture outcome) and D-37/D-38 in `audit/DECISIONS.md`; this change's spec deltas are synced into the living specs and `skip_specs: true` is set (Open Question 1 (a)). `check_milestone_readiness.py --milestone M10` lists no `plot-output`, `router-maze` or `router-maze-iroute` entry. | every other task |
| G1-PLOT golden | MISC-PLOT-01..20 captured (MISC-PLOT-12 and -13 from the `magic-cairo` variant). | PS, PNM implementation |
| G1-ROUTE golden | LEG-MZ-01..31 and the SCRIPT-AUX-02 re-capture captured; the two verify rows have their `Verified` lines. | MZ engine and IROUTE route tasks |
| Exit | D24 | the plotting and routing part of M10 |

| Round | Plot stream | Route stream | Shared |
|---|---|---|---|
| R0 | — | — | DECIDE (tasks section 1) |
| R1 | GOLDEN plot cases and the `magic-cairo` variant | GOLDEN LEG-MZ cases, the LEGACY authoring script | SCAFFOLD; PLOTTECH and MZTECH readers (they need no golden: their scenarios are unit tests and tech-load cases) |
| R2 | PLOTCMD, PS, PNM (after G1-PLOT) | MZ engine (after G1-ROUTE); IROUTE parameter subcommands and `saveParameters` (after MZTECH; they need no engine) | EXPORT (scene to SVG and PNG; needs neither stream) |
| R3 | `plot svg` on EXPORT | IROUTE route execution and endpoints (after MZ); derived style (D13); MAZETOOL (after IROUTE route) | DESIGN boards |
| R4 | — | — | SHELL (export dialog, maze helper panel); PERF; fuzz targets |
| R5 | — | — | INTEGRATION exit |

R1 starts as soon as G0 passes and does not wait for the 3D view's P2 or P3: no file of this change is owned by m10-3d-view (D3), and the two changes share only `docs/roadmap.md`, `CHANGELOG.md` and the capture workflow, which INTEGRATION edits in separate commits.

### D2. Licence boundary

- **Magic.** Magic is under the UC Berkeley permissive notice (P-10). PLOT ports `plot/` logic and embeds `magicps.pro` byte for byte; PNM bundles `mos.7bit.mraster.cmap` and `mos.7bit.mraster_dstyle` from Magic's `scmos/` as data under `assets/plot/`, unmodified, with Magic's notice beside them. MZ and IROUTE port `mzrouter/` and `irouter/`; MAZETOOL ports `tcltk/mazeroute.tcl` behaviour into C++. INTEGRATION adds the rows to the "Magic VLSI (ported logic)" table of `THIRD_PARTY_NOTICES.md` and a "bundled data" line for the three files; the PostScript prologue carries Tim Edwards's 2000 header in the file Hefesto writes, as Magic's output does. `check_third_party_notices.py` covers the new rows.
- **No new linked dependency.** PNG uses zlib, already built (`hefesto::zlib`, zlib licence, listed). The SVG and PNG writers, the PostScript writer and the PNM rasteriser are Hefesto code. Cairo is not used (P-3).
- **Tools used only in CI.** Ghostscript (AGPL-3.0) checks that PostScript output parses and renders it for a visual diff; resvg (Apache-2.0 OR MIT) rasterises SVG for the same purpose. Both run only as subprocesses in an optional `plot-validate` mode of `golden-capture.yml`, inside the capture image, as `oasis-validate` runs third-party validators; neither is linked, vendored or shipped, and both are recorded in `tools/golden/pins.toml` with their licences. Replay needs neither: PostScript and PNM are compared byte for byte (D19).
- **The Cairo capture variant** builds Magic with Cairo for MISC-PLOT-12 and -13 only (Q-MISC-52). Cairo is in the capture image (`libcairo2-dev` is already installed there) and never in Hefesto.
- **No xschem, netgen or ngspice involvement.** No clean-room rule applies to this change.

### D3. Layout of the code, workstreams and ownership

New core areas: `plot` (prefix `PLOT-`, `docs/diagnostics/codes/plot.md`) and `route` (prefix `ROUTE-`, `docs/diagnostics/codes/route.md`). The view export lives in the existing `display` area (prefix `DISPLAY-`).

| Module | Path (`src/core/{include/hefesto,src}/…`) | Owner |
|---|---|---|
| `plot` section reader, plot styles | `plot/tech/` | PLOTTECH |
| Plot parameters, `plot` command, plotted region | `plot/params/`, `plot/cmd/` | PLOTCMD |
| PostScript writer, embedded prologue | `plot/ps/` | PS |
| PNM style tables, rasteriser, Lanczos table | `plot/pnm/` | PNM |
| Atomic output files (temporary file and rename) | `plot/io/` | PLOTCMD |
| Scene to SVG, CPU raster, PNG encoder | `display/export/` | EXPORT |
| `mzrouter` section reader, styles, DRC derivation, derived style | `route/tech/` | MZTECH |
| Maze engine: blockage, estimate, search, heaps, paths, paint conversion | `route/maze/` | MZ |
| `iroute` command, parameters, `saveParameters`, endpoints | `route/iroute/` | IROUTE |
| `*mzroute`, `*iroute` diagnostics | `route/diag/` | IROUTE |
| `mazeroute` tool | `tools/mazeroute.cpp` (existing area) | MAZETOOL |
| Export dialog, maze helper panel | `src/shell/desktop/src/{plot_export,maze_helper}_*.cpp` | SHELL |

| Workstream (tasks.md section) | Capabilities | Owns (may edit) |
|---|---|---|
| DECIDE | — | `docs/decisions/open-questions/{MISC,LEGACY}.md` (the rows of this change), `docs/decisions/m10-plot-route-decision-table.md`, `docs/decisions/audit/DECISIONS.md` (D-37, D-38), the living specs touched by the deltas |
| SCAFFOLD | — | the module skeletons above, `docs/diagnostics/codes/{plot,route}.md`, the `CORE-NOTIMPL-010` row |
| GOLDEN | the MISC-PLOT, LEG-MZ and SCRIPT-AUX-02 cases | `tests/golden/MISC/` (PLOT cases), `tests/golden/LEGACY/`, `tools/golden/authoring/{m10_cases,legacy_cases}.py`, `tools/golden/compare/{ps_text,pnm_image,svg_shape}.py`, `tools/golden/container/` (the `magic-cairo` variant), `golden-capture.yml` (the variant and `plot-validate` steps) |
| PLOTTECH | `plot-output` "Technology plot section structure", "PostScript style lines", "PNM style lines", "Raster printer and legacy plot devices" | `plot/tech/`, the `plot` hand-off in `tech/loader.cpp` |
| PLOTCMD | "Plot command and plotted region", "Plot parameters", "Plot output safety and limits" (files) | `plot/params/`, `plot/cmd/`, `plot/io/`, the `plot` registration |
| PS | "PostScript file layout", "… paint output", "… cell outlines", "… labels", "… label margins", "Plot behaviour with magic-compat off" (PS points) | `plot/ps/`, `assets/plot/magicps.pro` |
| PNM | "PNM rasterisation and file format", "Platform mechanism of plot output", the PNM points of the safety requirement | `plot/pnm/`, `assets/plot/mos.7bit.mraster*` |
| EXPORT | `hefesto-rendering` "View export to SVG and PNG"; `plot-output` "SVG plot" | `display/export/`, the `svg` case of `plot/cmd/` (by agreement with PLOTCMD, D4) |
| MZTECH | `router-maze` "mzrouter section grammar", "Style defaults and lookup", "Width and spacing derivation from DRC"; `router-maze-iroute` "Derived irouter style" | `route/tech/`, the `mzrouter` hand-off in `tech/loader.cpp`, the post-load and `scalegrid` hooks |
| MZ | every other `router-maze` requirement | `route/maze/` |
| IROUTE | every `router-maze-iroute` requirement except "Derived irouter style" | `route/iroute/`, `route/diag/`, the `iroute`, `*iroute`, `*mzroute` registrations |
| MAZETOOL | `scripting-auxiliary-tools` "Netlist maze-routing script" | `tools/mazeroute.cpp`, `tools/commands.cpp`, `tools/codes.cpp` (retire `TOOLS-MAZE-001`), `tests/unit/tools/auxiliary_test.cpp` |
| DESIGN | `hefesto-design-system` patterns used by the export dialog and the maze helper | `docs/design/mockups/{PlotExport,MazeHelper}.dc.html`, `docs/design/m10-plot-route-ui-conformance.md` |
| SHELL | `plot-output` "Plot export from the desktop"; the maze helper window | `src/shell/desktop/src/{plot_export,maze_helper}_*.cpp`, the File menu entry and the Options > Maze Router entry in `menu_bar_model.cpp` |
| PERF | `hefesto-performance` "Plot and maze-route throughput" | `bench/plot/`, `bench/route/`, `perf.yml` jobs `plot-bench` and `route-bench` |
| INTEGRATION | exit | `docs/roadmap.md`, `CHANGELOG.md`, `THIRD_PARTY_NOTICES.md`, `tests/golden/deviations.toml` (UI-G-005, SCRIPT-AUX-02 and the new deviations), `scripts/check_requirement_coverage.py` (the M4 deferral), `docs/contributing/` |

`tech/loader.cpp` is edited by PLOTTECH and MZTECH in two separate hand-off functions; the first to merge adds the dispatch, the second adds one line. `src/core/src/ui/magic_help.cpp` already has the help strings of `plot`, `iroute` and `*iroute`; IROUTE adds `*mzroute` if it is missing and nothing else changes there.

### D4. Session state and shared interfaces

**Technology.** `tech::Technology` gains two parsed sections next to the raw ones it keeps today (`technology.hpp:80-93`, `raw_sections()`):
- `plot::PlotTech`: the PostScript patterns, colours and layer styles in Magic's (reversed) order; the PNM type colours and the loaded colour map and dstyle table; a flag for "no plot section" (the PNM warning); an `init_failed` flag (Magic's `Init_Error`).
- `route::MazeStyles`: the styles in Magic's order, each a value type (`route::MazeStyle`: route layers, contacts, spacings, search and wizard parameters), plus the derived `irouter` style when D13 applies, marked as derived.

Both are rebuilt on every technology load and on `scalegrid` (Magic's `MZAfterTech`, `IRAfterTech`), from the raw lines the loader already stores, so the `tech-file-format` rule "Hefesto SHALL register every section" holds unchanged and the existing raw-section test only gains assertions.

**Session.** `EditorState` gains:
- `plot::Params`: the 14 parameters of "Plot parameters", reset to defaults at start-up only (Magic keeps them across technology loads).
- `route::IrouteSession`: the session copy of the `irouter` style (or of the derived one), `irouter` verbosity and the `wizard window` setting. Re-copied from the technology on every load and `scalegrid`, as `router-maze-iroute` "iroute availability and dispatch" states.

**Plot interface.**
- `plot::Region resolve_region(const EditorState&, const PlotRequest&) -> diag::Outcome<Region>`: window, area and layer mask ("Plot command and plotted region").
- `plot::write_postscript(const Region&, const PlotTech&, const Params&, const PlotContext&, std::ostream&) -> diag::Outcome<void>`.
- `plot::write_pnm(const Region&, const PlotTech&, const Params&, const PlotContext&, std::ostream&) -> diag::Outcome<PnmInfo>`.
- `PlotContext` carries `magic_compat`, the cancel token, a progress sink and the read view of the layout (D16). `plot/io/` wraps every writer in `AtomicFile` (Q-MISC-53).

**View export (EXPORT).** `display::export_svg(const LayoutScene&, const ExportOptions&, std::ostream&)` and `display::export_png(const LayoutScene&, const ExportOptions&, std::ostream&)`. Both take the `LayoutScene` that `display::build_scene` (`display/scene.hpp:165`) builds for a window, the desktop canvas already draws (`layout_canvas.cpp:209`). `plot svg` builds the scene of the plotted window at its pixel size and calls `export_svg`. PNG has no `plot` option (Q-MISC-51); the desktop export dialog and, later, `reliability-visualization` call `export_png`.

**Router interface (MZ; Q-LEGACY-41, J4).**
```
struct RouteRequest {
  MazeStyle style;                    // value copy, never shared
  std::vector<StartTerm> starts;      // point + route type
  std::vector<DestArea> dests;        // rect + route type
  LayoutView layout;                  // read view: root cell, expansion mask, reference window area
  RunControls controls;               // cancel token, bloom budget (optional), search-memory limit, magic_compat
};
struct RouteResult {
  ResultCode code;                    // success, best_before_interrupt, failure, unroutable, interrupted, already_routed
  std::vector<PathPoint> path;        // point, route type, orientation
  PaintList paint;                    // rectangles per tile type, root coordinates
  Cost cost; Cost initial_estimate;
  SearchStats stats;                  // blooms, points processed, heap peaks, end reason
  std::vector<diag::Message> messages;// the engine's own lines (fence choice, zero length, statistics)
};
RouteResult route::maze::route(const RouteRequest&);
```
The engine owns its blockage planes, estimate plane, heaps and point hash per call; nothing is static. Painting `paint` into the edit cell (one undo group), the selection, redisplay and the DRC queue belong to the caller (`route/iroute/`, later the agent's tool). `layout` is a read view taken under the session's shared lock (D16). Magic's internal yank cells (`__BLOCK`, `__ESTIMATE`, …) are private planes of the call, never cells in the database, so the cell list a user sees never shows them.

### D5. Compatibility branch points

Each compat row has one branch point, named here and tested in both modes:

| Row | Branch point | ON | OFF |
|---|---|---|---|
| Q-MISC-41 | `plot::ps::emit_outline_rect` | lower-left inside the area | any overlap |
| Q-MISC-43 | `plot::ps::label_overhang` | Magic's mixed coordinates | relative coordinates |
| Q-MISC-55 | `plot::io::with_default_extension` | whole path | last component |
| Q-LEGACY-33 | `route::tech::apply_width_statements` | DRC wins | `width` wins |
| Q-LEGACY-85 | `route::maze::initial_lr_contact_fits` | `<=` (Magic) | `>` |
| Q-LEGACY-87 | `route::maze::extend_up_end_point` | unconditional tile bottom | nearest candidate |
| Q-LEGACY-89 | `route::maze::contact_same_node_exempt` | contact-plane tile | layer-plane tile |
| Q-LEGACY-42 | `route::iroute::report_already_routed` | `Route Failed.` | `Already routed.` |
| Q-LEGACY-43 | `route::iroute::format_saved_spacing` | bare integer | `i` suffix |
| Q-LEGACY-45 | `route::iroute::parse_route_coord` | plain number only, rect as given | any coordinate, rect normalised |
| Q-LEGACY-37 | `route::tech::maybe_derive_irouter` | never | when no `irouter` style |
| Q-LEGACY-44 | `script::source_line_prefix` (if the capture shows Magic fails) | Magic's failure | `:` accepted |

The fix rows (Q-MISC-42, -48, -49, -50 (1)–(3), -53; Q-LEGACY-31, -35, -36, -38, -39, -40, -41, -47 (overflow), -48, -90) have no branch point. The replicate rows have none either.

### D6. Technology readers (PLOTTECH, MZTECH)

**`plot` section.** A state machine over the section's lines with Magic's states (no style yet, inside a known style, skipping an unknown one until the next `style`). Style handlers:
- `postscript`: 9, 5 and 3-field lines as "PostScript style lines" states; all three lists prepended (Magic's order); numeric fields checked (Q-MISC-50 (2)); type lists expanded through the layer-composition sets and restricted to user-visible types (`DBUserLayerBits`).
- `pnm`: keyword prefixes `color`, `dstyle`, `default`, `draw`, `map` (`strncmp` semantics); file search for `color`/`dstyle` in the current directory, the technology's directory, then `assets/plot/` (the bundled files are what Magic finds on its library path for the `mos` style type). Parsing reuses `display::parse_color_map` and `display::parse_dstyle` (`display/styles.hpp:63`, `:137`) with the `.7bit.mraster` names; the blend formula v = P − 127 + floor(C/2), clamped at 0. The end-of-section rule applies `default` when no type received a colour. A format error sets `init_failed` until the next load (Magic's `Init_Error`).
- `gremlin`, `versatec`, `colorversatec`, `pixels`: lines accepted and ignored (Q-MISC-51).
Errors use Magic's texts through the technology loader's error reporting, so they appear with the line context the other sections use.

**`mzrouter` section.** One statement parser per keyword. Every statement is parsed into a local value and validated before it is added to the style (Q-LEGACY-31, -39). At the end of each style the layer, contact and type lists are in file order (Magic reverses its prepended lists; Hefesto appends). After the whole technology is read, and after `scalegrid`, MZTECH runs the DRC derivation of "Width and spacing derivation from DRC" against the loaded DRC rules (`drc::RuleSet`, M3), then the explicit `spacing` statements, then (OFF only) the `width` statements (Q-LEGACY-33), then the SUBCELL maxima. Spacing tables cover only defined tile types (Q-LEGACY-35). Style variants are discarded on reload, as `tech-loading-and-scaling` step 3 says.

### D7. Plot command and parameters (PLOTCMD)

- Option lookup is the command layer's unique-prefix lookup, case-insensitive, with Magic's error and help texts (Q-MISC-51 option set: `postscript`, `pnm`, `svg`, `parameters`, `help`).
- Argument counts, the `pnm` width parse ("Width should be an integer number of pixels") and the default width 1000 follow `plotCmd.c`. Layer lists go through the existing layer-list parser (`db/layer_list`, as `cmd-paint-erase` specifies, including `labels` and `subcell`).
- Region: the window under the pointer or the only layout window (the existing window lookup), the box if it is in the window's root cell, else the root bounding box; the layer mask from the window's visible layers, labels when shown and `dbw_labelSize >= 0`, subcell outlines always.
- Parameters: unique-prefix match, case-sensitive; the printed listing uses Magic's printed names (`PS_cellNameFont` for `ps_namefont`, and so on), groups and formats; every constraint message from `plotMain.c:445-646`; `pnmmaxmem` capped (Q-MISC-49). Values are kept in `plot::Params` with the types Magic uses (fonts as strings, sizes as `int`).
- Every output goes through `plot::io::AtomicFile`: a temporary file `.<name>.hefesto-tmp-<pid>-<n>` in the target directory, `fsync` and rename on success, removal on interrupt or error (Q-MISC-53). On iPadOS the target is inside the app container or a security-scoped URL, through the platform file service; the share sheet hands it out (D17).

### D8. PostScript writer (PS)

- A port of `PlotPS` in the order "PostScript file layout" gives: scale (single precision, as `float` arithmetic), margins, header, prologue, procedures, colours, patterns, page setup, paint, cell outlines, labels, trailer.
- The prologue is `assets/plot/magicps.pro`, embedded at build time by a CMake step that generates a byte array; a unit test compares it with the file's SHA-256 recorded in `docs/contributing/` and with the MISC-PLOT-01 capture.
- Floats are printed from the `float` value with `std::to_chars(…, std::chars_format::fixed, 3)` after promoting to `double` exactly as `printf("%.3f", (double)f)` does, so the digits are glibc's (correctly rounded) on every platform (Q-MISC-54).
- Paint: the hierarchical tile enumeration of `layout-geometry-model` (planes in plane order, canonical tile order, subcells recursively, transformed to root coordinates); the rectangle merge buffer and the line buffer as specified; split tiles through a port of `GrClipTriangle`. Boundary segments come from the neighbour walk per side in Magic's order.
- Text escaping (Q-MISC-42) is one function used by labels, cell names and instance ids; margins count unescaped characters.
- Zero-area areas are refused before the scale is computed (Q-MISC-48).
- Undefined colour or pattern references are checked once per plot against the style tables; the style is skipped with `PLOT-PS-002` (Q-MISC-50 (3)).

### D9. PNM writer (PNM)

- A port of `PlotPNM`: the paint bounding box clipped to the area, `scale` and `scale_over_2` in single precision, forced downsampling with Magic's three messages (Q-MISC-47 replicate: the message's formula is computed in 64-bit, giving Magic's numbers wherever Magic does not overflow), strips from the top, tile blending with contacts once (Magic's `DBTreeSrUniqueTiles`), split tiles by the triangle inequality, sampling and antialiasing as "PNM rasterisation and file format" states.
- Lanczos table (Q-MISC-54): the 1 024 + 1 entries Magic uses are generated once by `tools/golden/gen_lanczos_table.py`, which runs the Magic formula in C (`sin` from glibc, cast to `float`) inside the capture image, and are committed as `plot/pnm/lanczos_table.inc` with the generator and the glibc version in its header. A unit test recomputes the table with the platform's `sin` and reports (does not fail on) last-bit differences; the golden cases with antialiasing (MISC-PLOT-07, -08) pin the bytes.
- The plot sources are compiled with `-ffp-contract=off` (Clang and GCC) and the accumulation order is Magic's loop order, so float sums are the same on every platform.
- Memory: 64-bit sizes, a strip of at least 2·scale_over_2 + 1 rows, downsampling capped at 2^15, output at most 65 535 pixels per side (Q-MISC-49, D18).
- Output naming per Q-MISC-55 (D5); `P6` header, rows top first; "PNM image dimensions: %d x %d"; progress lines every 10 strips as Magic prints them; interrupt as Magic's message plus the atomic-file cleanup.
- With no `plot` section, the warning and the display-derived defaults ("Technology plot section structure"); the display styles come from the loaded display style set (`display::load_style_set`), which the headless CLI also loads.

### D10. View export: SVG and PNG (EXPORT; D-37, D-38, Q-MISC-52)

- **Owner.** `hefesto-rendering` owns view export (D-38 (recommended) and D-37 (a)), through the ADDED requirement "View export to SVG and PNG". `plot-output` "SVG plot" defines the command and points to it. The module is backend-free core code in `display/export/`, so the headless CLI, the iPad app and CI use it without a GPU.
- **Input.** The `LayoutScene` of a window: layers in display-style order with their rectangles, polygons and stipple and outline attributes, labels, cell outlines. The window's screen area gives the pixel size; headless windows have `view::kNullScreen` (512 × 484) unless resized.
- **SVG.** One `<pattern>` per stipple used (8 × 8 or 16 × 16 bits, from the dstyle table, one `<rect>` per set bit run), one `<g>` per (layer, style) with its fill and outline, geometry as `<path>` of merged rectangles, labels as `<text>` with the display font family and size, cell outlines as `<rect>` plus `<text>`. Coordinates are integer pixels after the view mapping, so the document is byte-identical across platforms. Element order is the scene's order. No scripts, no external references, no fonts embedded.
- **PNG.** A CPU scan-converter of the same scene (rectangles, triangles from split tiles, 1-pixel outlines, stipples), drawing in scene order into an RGB8 buffer, then PNG chunks (IHDR, IDAT through zlib `deflate` at level 6 with a fixed strategy, IEND), filter type 0 on every row so the bytes do not depend on a filter heuristic. Text is drawn with the glyph rasteriser the renderer's font atlas uses (`display/font.cpp`).
- **Comparison with Magic.** MISC-PLOT-12 compares the file name, the root `width` and `height` and well-formedness with Magic's Cairo SVG (`svg_shape` comparator, D19). Hefesto-only cases check structure and bytes.

### D11. Maze engine (MZ)

The engine is a port that keeps every structure whose iteration order decides a result:
- **Blockage planes.** Per route type, horizontal-strip and vertical-strip corner-stitched planes of the six tile kinds (space, same node, walk, destination, blocked, …), built lazily per bounds increment as "Lazy blockage generation" states, from the layout view's paint, subcells and spacings. They use the core's corner-stitched plane (`db::Plane`) with a private tile-type table, so enumeration order is `layout-geometry-model`'s canonical order (Q-LEGACY-92).
- **Estimation.** The estimate plane, corner Dijkstra and straight-shot estimators as specified (Q-LEGACY-96 replicate), in saturating 64-bit arithmetic (Q-LEGACY-90).
- **Search.** The four stacks and four heaps, the window sliding by rate × blooms, adjusted costs with the penalty mantissa (Q-LEGACY-32), the point hash with the hashed-best rule, stale paths consuming blooms (Q-LEGACY-95). Heaps are a port of Magic's `utils/heap.c` (sift-up and sift-down, lazy ordering before the first `top`), so ties come out in Magic's order (Q-LEGACY-6).
- **Extension.** Interesting points for the four directions, contact and walk extension; the replicate rows (Q-LEGACY-86, -88, -91, -94) as specified; the compat rows behind their branch points (D5); Q-LEGACY-93's no-op passes are not written.
- **Path to paint.** Merged segments, contact rectangles, the close-contact fixes, into `RouteResult::paint`.
- **Control.** The cancel token is polled where Magic polls `SigInterruptPending`; the bloom budget is checked where Magic checks `bloomLimit`; the memory limit counts the engine's own allocations (an arena per call) and is checked at the same place (Q-LEGACY-48). Undo is never touched by the engine; the fence message is returned in `messages` (Q-LEGACY-38 cannot recur).
- **Statistics.** Verbosity ≥ 2 lines (`Initial Cost Estimate`, `PATH #…`, the statistics report) are built from `SearchStats` by IROUTE with Magic's formats.

Size: about 10 000 lines of C++ for the engine. The functions that are long in Magic (`mzSearch`, the four `mzXtnd*` files, `mzBuildBlock`) are split by phase so each stays within cognitive complexity 15; the four direction files share one template parameterised by direction, with the Q-LEGACY-87 difference kept as an explicit branch in the upward instance.

### D12. iroute command (IROUTE)

- Dispatch, prefix matching, verbosity 0 suppression and the missing-style error as "iroute availability and dispatch" states; the derived style as D13.
- `contacts` and `layers`: the argument forms, the value cycling (Q-LEGACY-46 replicate), the table formats, `-list` for scripting (nested lists), Magic's error texts. Boolean display prints no spurious error (Q-LEGACY-36).
- `search`, `verbosity`, `version`, `wizard`: as specified; `penalty` overflow rejected (Q-LEGACY-47); `window .` needs a window under the pointer (headless: the only layout window).
- `spacings`: as specified, including `CLEAR`, NIL forms, partial application and the usage line (Q-LEGACY-46).
- `saveParameters`: Magic's exact text through `std::ofstream` on the path as given (no `~` expansion, as Magic's `fopen`), spacings formatted per Q-LEGACY-43.
- `route` and the no-argument form: option parsing with Magic's texts, `-timeout` (D14), endpoint resolution (D14), then `route::maze::route`, then the result handling of "Route results": paint copied into the edit cell with the root-to-edit transform as one undo group, the new selection, redisplay and the DRC re-check queued on the routed area (`docs/diagnostics/codes/drc.md:176`), the Tcl result string.
- `*iroute` (`debug`, `help`, `parms`) and `*mzroute` (`debug`, `dumpEstimates`, `dumpTags`, `help`, `numberLine`, `parms`, `plane`, `version`) as informative diagnostics; `dumpEstimates` prints from the last route's estimate plane, which `RouteResult` keeps only when a debug flag asks for it.

### D13. Derived `irouter` style (MZTECH; Q-LEGACY-37, J2)

With `magic-compat` OFF and no style named `irouter`, `route::tech::maybe_derive_irouter` builds one after the DRC derivation, from the technology's `lef` section as M6's reader already parses it (`lef::LayerTable`, `lef/tech/table.hpp`: route and cut records in file order):
- route layers: `routing` lines in order; costs alternate 32/64 and 64/32 starting at the lowest layer (scmos's `irouter` values for m2 and m1); jogCost 256, hintCost 1, overCost 1;
- contacts: `cut` lines whose two residues are both route layers, cost 1024;
- widths and spacings from DRC as for any style.

sky130A gets li, metal1–metal5 and the redistribution metal from its `routing` lines and mcon, via, via2–via4 from its `cut` lines; gf180mcuD gets metal1–metal5 and via1–via4; ihp-sg13g2 gets Metal1–Metal5, TopMetal1–2 and the contacts its `cut` lines name (Via1–Via4, TopVia1, and TopVia2 when its residues are both route layers). The derivation is a pure function of the technology, tested on the three PDKs and on scmos with its `irouter` style removed. The first `iroute` of a session that uses it prints the notice of "Derived irouter style" once.

Why not ship per-PDK overlays (option (c)): they would have to follow PDK releases and are Hefesto-authored data presented as PDK data. Why not derive in both modes (option (d)): with `magic-compat` ON Hefesto must answer as Magic, which refuses.

### D14. Endpoints, prompts, timeout and cancellation (IROUTE)

- **Endpoints.** A port of `irRoute`'s start and destination resolution: `-sPoint` and `-dRect` in edit coordinates transformed to root; labels from the selection first, then the hierarchy; the box; the selection through the connectivity table of each active route layer; the first active route layer for label endpoints (Magic's `break`).
- **Several nodes (Q-LEGACY-40).** The prompt goes through the command layer's prompt host (`cmd::PromptHost`). When the host cannot ask (script host, headless batch, agent), the empty answer is taken and the lines are still printed. The prompt is never read from stdin unless the session's console is that terminal.
- **Timeout (Q-LEGACY-41).** `-timeout n` sets a deadline on the route's cancel token; the token belongs to the command invocation, so it is gone on every exit path. No signal or global timer is used. The interrupt key sets the same token.
- **Budget.** `RunControls::bloom_budget` is set by the agent's tools, by `mazeroute` in golden replays (D15) and by tests; `iroute route` has no option for it.

### D15. `mazeroute` and the maze helper (MAZETOOL, SHELL; Q-SCRIPT-30, J6)

- `mazeroute <netfile>` replaces the `TOOLS-MAZE-001` stub with Magic's behaviour: the `.net` fallback, the header line skipped, nets separated by blank lines, terminals routed as a chain with `iroute route -dlabel <previous> -slabel <current> -timeout 3` through the command layer, so the console and the Tcl results are as Magic's (Q-SCRIPT-30 replicate). `routenet`'s acceptance of "success", "best before interrupt" and "already routed" is kept for the maze helper.
- **Golden replays.** SCRIPT-AUX-02 replays with Hefesto's `-timeout 3`: on its small layout every route ends long before 3 s in both tools, so the wall clock does not decide the result. A replay setting `router.replay-bloom-budget` exists for cases where it would.
- **Maze helper window** (J6 (a)): a desktop panel reached from Options > Maze Router with Magic's lists (unrouted and routed nets), route, rip up, verify, and save and load of failed nets, driving the same commands. Its view model is headless and tested; routing a net with fewer than two pins does nothing (Magic's helper stops with an unset-variable error there). It does not port `genmazeparams`, which is broken in Magic (it reads an undefined variable and uses a widget Tk does not have). Not on iPadOS (the netlist workflow is desktop-only, `hefesto-ipad-feature-scope`).

### D16. Threading

- **Plot.** `plot` runs on the command thread, as every Magic command does, holding the session's shared (read) lock for the whole plot so no edit changes the layout mid-file. Long plots report progress through the long-operation service and are cancelled through the interrupt token. The desktop runs the export dialog's command through the same command queue, so the UI stays responsive (the shell's existing long-command pattern). No parallel rasterisation: strips are rendered in order, which keeps the bytes identical and memory bounded by `pnmmaxmem`.
- **Route.** The search runs on the command thread with a read view taken under the shared lock; the engine allocates only from its per-call arena. Painting takes the exclusive lock briefly for one undo group. The engine is single-threaded by design: the visit order is the specification. Agent scratch sessions (M11) each have their own `EditorState`, lock and engine call, so several routes run in parallel across sessions with no shared state.
- **View export.** Pure functions of an immutable scene; the desktop may run them on a worker thread.

### D17. Desktop (DESIGN, SHELL; design Open Question 3)

- **Export dialog.** File > "Export Image…" (the File menu mirrors Magic's, whose "Print…" item is commented out; this is a native addition): format (PostScript, PNM, SVG, PNG), area (box or cell), layers (visible by default), PNM width, PNG size; it issues the console command (`plot …`) or the PNG export and shows progress and cancel. On iPadOS (M14) the same action is in the document menu and the file goes to the share sheet; M10 builds and tests the view model only.
- **Maze helper.** D15. Options > Maze Router stops being gated once the panel lands (`later_milestone()` in `menu_bar_model.cpp` does not list it today, so SHELL adds the entry).
- **No routing tool UI.** Interactive routing beyond `iroute` from the console and the maze helper waits for M11, where the agent's approval flow shows proposed routes.
- DESIGN draws `PlotExport.dc.html` and `MazeHelper.dc.html` with the forge components before SHELL starts, and records the conformance table.

### D18. Resource caps

| Resource | Cap | Behaviour at the cap | Row |
|---|---|---|---|
| `pnmmaxmem` | 1..4 194 304 KB | parameter rejected outside | Q-MISC-49 |
| PNM output | 65 535 pixels per side | `PLOT-PNM-003`, nothing written | Q-MISC-49 |
| PNM downsampling | 2^15 | `PLOT-PNM-002`, nothing written | Q-MISC-49 |
| PNG export | 16 384 pixels per side | `DISPLAY-EXPORT-001` | D10 |
| SVG export | 2 000 000 elements | the export stops with `DISPLAY-EXPORT-002`; nothing written | D10 |
| Maze search memory | `router.max-search-memory`, 1 GiB desktop, 256 MiB iPadOS | ends as an interrupt, `ROUTE-MZ-002` | Q-LEGACY-48 |
| Maze bloom budget | none by default; set by callers | ends as an interrupt | Q-LEGACY-41 |
| `iroute` value lists | the command line's own limit | — | — |

Caps are settings in `settings::ProjectSettings` where a user could reasonably need more (search memory), fixed otherwise.

### D19. Golden-case plan (GOLDEN)

**Plot (`tests/golden/MISC/CASES.md`).** MISC-PLOT-01..12 as listed, plus:

| ID | Input | Captures | Covers |
|---|---|---|---|
| MISC-PLOT-13 | `plot`, `plot help`, `plot foo`, `plot versatec` (Cairo variant) | stdout | option set (Q-MISC-51) |
| MISC-PLOT-14 | sky130A, `sky130_fd_sc_hd__inv_1`, `plot pnm inv` | inv.pnm, stdout | PDK PNM (`default`, `draw … no_color_at_all`) |
| MISC-PLOT-15 | gf180mcuD, a standard-cell inverter, `plot pnm inv 400` | .pnm, stdout | PDK PNM |
| MISC-PLOT-16 | ihp-sg13g2, `sg13g2_inv_1`, `plot pnm inv 400` | .pnm, stdout | PDK PNM |
| MISC-PLOT-17 | argument errors: `plot postscript`, `plot pnm a 1 2 3`, `plot pnm x abc`, `plot pnm x 0`, `plot parameters ps_cellid 3`, `plot parameters nosuch 1` | stdout, stderr | command and parameter errors |
| MISC-PLOT-18 | `plot pnm out 100`, `plot pnm ./out2 100`, `plot pnm sub.d/out3 100` | file names | extension rule (Q-MISC-55, ON) |
| MISC-PLOT-19 | empty area; `pnmbackground 0`; `pnmdownsample 1` | stdout, .pnm | PNM parameters and empty region |
| MISC-PLOT-20 | tech plot section errors: three-field `style`, unknown style, line before `style`, four-field ps line, non-integer second field, `versatec` block | tech-load messages | section structure, style lines, legacy devices |

The PostScript cases use a minimal Hefesto-authored test technology (`tools/golden/authoring/plottest.tech`) with `plot` styles, as the PDKs have only PNM. MISC-PLOT-03 includes a label with `(` (Q-MISC-42): it is a documented deviation (Hefesto escapes it, in both modes), with the reason and decision in `deviations.toml`.

**Route (`tests/golden/LEGACY/CASES.md`).** LEG-MZ-01..20 as listed, plus:

| ID | Input | Captures | Covers |
|---|---|---|---|
| LEG-MZ-21 | `iroute help`, `iroute help route`, `iroute version`, `iroute verbosity`, `verbosity 0` then `verbosity 2`, `iroute foo`, `iroute s` | stdout, stderr | dispatch, help, version, verbosity |
| LEG-MZ-22 | `iroute wizard`, each wizard parameter set and shown, invalid values | stdout, stderr | wizard parameters |
| LEG-MZ-23 | `iroute search`, `search rate 100`, `search width x`, too many arguments | stdout, stderr | search parameters |
| LEG-MZ-24 | route option errors: `-dRect` (non coordinate, nonnumeric, incomplete), `-timeout` missing and non-integer, missing label and layer list, no active layers, `-spoint 1um 1um` | stderr | route options (Q-LEGACY-45, ON) |
| LEG-MZ-25 | Tcl results of success, failure, unroutable (destination enclosed in blockage) and already routed | Tcl results, stdout, stderr | route results (Q-LEGACY-42, ON) |
| LEG-MZ-26 | routes with `-sPoint`/`-dRect`, `-dSelection`, `-dBox` | `.mag`, stdout | endpoints |
| LEG-MZ-27 | sky130A: `iroute`, `iroute layers` | stderr | missing style on a PDK (Q-LEGACY-37, ON) |
| LEG-MZ-28 | several nodes at the start point, batch run with stdin at end of file | stdout, `.mag`, exit status | endpoint prompt (Q-LEGACY-40 verify) |
| LEG-MZ-29 | start points on both sides of fence, then paint and `undo` | `.mag`, stdout | Q-LEGACY-38 (documented deviation: Magic's undo stays off) |
| LEG-MZ-30 | `scalegrid 1 2`, change spacings, `saveParameters` | file | Q-LEGACY-43 (ON) |
| LEG-MZ-31 | `*mzroute help`, `*mzroute parms`, `*iroute help`, `*iroute parms` | stdout (informative) | diagnostics |

LEG-MZ-11 also runs `source p.ir` in Magic's Tcl build to settle Q-LEGACY-44. SCRIPT-AUX-02 is re-captured unchanged and replayed; its deferral goes. A new authoring script, `tools/golden/authoring/legacy_cases.py`, generates the LEG-MZ inputs (scmos from Magic's `scmos.tech.in` as an `[[external]]` generated input, as today); the MISC-PLOT inputs extend `m10_cases.py`.

**Comparators (`tools/golden/compare/`).**
- `ps_text`: byte comparison, reporting the first differing line with its section (header, prologue, procedures, paint, labels).
- `pnm_image`: header, then bytes; on a difference, the number of differing pixels, the bounding box and the first one. Exact by default; no tolerance is configured for any MISC-PLOT case.
- `svg_shape`: parses with `xml.etree`, compares the root element's name, `width`, `height` and the file name; informative content.
- `.mag` routed paint: the existing `mag_backup` and `polygon_set` comparators.

**Capture image.** A `magic-cairo` variant (`--build-arg WITH_CAIRO=1`): the same Magic commit and configure line as the main image, with `--with-cairo` and the Tcl wrapper, run under Xvfb with `-d XR`, for MISC-PLOT-12 and -13 (manifest `image = "magic-cairo"`). It follows the `magic-gl` variant's pattern in the Containerfile and the workflow, and the replay skips its cases with a note when the variant is not built. GOLDEN first checks that the main image installs `magicps.pro` where Magic's `SysLibPath` finds it (MISC-PLOT-01 must contain the prologue, not `postscript_prolog_is_missing`).

**`plot-validate` mode** (optional, informative): Ghostscript renders every captured and replayed `.ps` to PPM and fails on a PostScript error; resvg rasterises Hefesto's SVG outputs. It runs on demand and before exit, not on every pull request.

### D20. Fuzz targets and determinism

- Fuzz targets: `fuzz_plot_section` (the `plot` section reader with random style blocks), `fuzz_pnm_cmap_dstyle` (colour-map and dstyle files), `fuzz_mzrouter_section`, `fuzz_iroute_commands` (random `iroute` subcommands and arguments against scmos, including `saveParameters` and re-sourcing the file), `fuzz_maze_route` (random small layouts and endpoints under a bloom budget). Each runs 10 minutes in the nightly fuzz job with no crash, leak or timeout, as the existing targets do.
- Determinism: the plot sources and `route/maze/` compile with `-ffp-contract=off`; no `std::unordered_*` iteration decides an output; every golden case runs on Linux x86_64, Linux aarch64 and macOS arm64 in CI and must match the same expected files.

### D21. Performance (PERF)

`hefesto-performance` gains "Plot and maze-route throughput" (delta):
- `plot-bench`: `plot postscript` and `plot pnm … 4000` of the `sky130_fd_sc_hd` all-cells block under callgrind, Hefesto against Magic, each isolated by subtraction (the DRC benchmark's method), plus peak memory of `plot pnm`.
- `route-bench`: the routed LEG-MZ cases and a 20-net `mazeroute` workload on scmos against Magic under callgrind; the 200 µm sky130A route with the derived style on the M3 Pro development machine (the M1 Mac number is recorded when the reference hardware run happens, as for the 3D view).
Both run in `perf.yml`, with results in the job summary and baselines recorded once Hefesto's first counts are in.

### D22. Changes to earlier code and close-out of deferrals

- `tech/loader.cpp`: hands the `plot` and `mzrouter` raw lines to their readers after load; `scalegrid` re-runs the post-load steps (Magic's `MZAfterTech`, `IRAfterTech`).
- `tools/`: `mazeroute` implemented; `TOOLS-MAZE-001` retired from `codes.cpp`, `diag_codes.hpp`, `docs/diagnostics/codes/tools.md`, the comment in `commands.hpp` and `auxiliary.hpp`, and the stub test.
- `tests/golden/deviations.toml`: SCRIPT-AUX-02 loses `deferred = "M10"`; UI-G-005's reason drops "plot, iroute, *iroute, *mzroute M10" (the deviation stays for M12's commands); new deviations for MISC-PLOT-03 (Q-MISC-42) and LEG-MZ-29 (Q-LEGACY-38), each with its decision.
- `scripts/check_requirement_coverage.py`: the M4 deferral of "Netlist maze-routing script" removed.
- `CORE-NOTIMPL-010` marks the unimplemented paths of this change while it lands (M9's plan uses 009) and is retired at exit.
- `docs/decisions/m4-decision-table.md`, `openspec/changes/m4-scripting/tasks.md:150-153`: the M10 follow-up noted as done.

### D23. The M11 seam

M11's typed `route` tool (`agent-architecture`, `agent-schematic-driven-layout`) calls `route::maze::route` with a `RouteRequest` built from its arguments and a bloom budget, in the agent's scratch session, and paints the result itself through the approval flow. M10 provides:
- the interface of D4 with no globals, a structured result and deterministic budgets;
- the derived style, so the request has a style on the three PDKs;
- the engine reentrant per session (D16).
M10 does not decide Q-AGENT-1 (analog routing). If M11 adds constraints (per-net width, shielding, symmetry), they go in `RouteRequest` as optional fields and in new spacing and cost rules behind a `magic-compat` OFF default; the Magic-compatible search stays the base.

### D24. Exit list

- Coverage: `check_requirement_coverage.py` for `plot-output`, `router-maze`, `router-maze-iroute`, the "View export to SVG and PNG" requirement of `hefesto-rendering`, "Plot and maze-route throughput" of `hefesto-performance`, and "Netlist maze-routing script" of `scripting-auxiliary-tools`.
- Goldens: MISC-PLOT-01..20, LEG-MZ-01..31 and SCRIPT-AUX-02 replay on the three CI platforms; only the documented deviations (MISC-PLOT-03, LEG-MZ-29) and none deferred.
- Both-mode unit tests for every row of D5.
- Fuzz targets of D20 green; `plot-validate` passes.
- PERF gates of D21 met or recorded with their baselines.
- `TOOLS-MAZE-001` and `CORE-NOTIMPL-010` retired; the M4 deferral and UI-G-005's M10 names removed.
- `THIRD_PARTY_NOTICES.md` rows and `check_third_party_notices.py`.
- `openspec validate --all --strict`; roadmap status and CHANGELOG.

## Area contracts

### PLOTTECH: `hefesto/plot/tech/`
`read_plot_section(const RawSection&, const TypeTable&, const StyleResolver&, TechDiagnostics&) -> PlotTech`. `PlotTech` holds `ps_patterns`, `ps_colors`, `ps_styles` (Magic's order), `pnm_colors` (per tile type), `cmap`, `dstyles`, `has_section`, `init_failed`.

### PLOTCMD: `hefesto/plot/params/`, `hefesto/plot/cmd/`, `hefesto/plot/io/`
- `Params` with `print(std::ostream&)` and `set(std::string_view name, std::string_view value) -> diag::Outcome<void>`.
- `resolve_region(const EditorState&, const PlotRequest&) -> diag::Outcome<Region>`.
- `AtomicFile open_atomic(const std::filesystem::path&) -> diag::Outcome<AtomicFile>` with `commit()` and destructor cleanup; `with_default_extension(path, ext, bool magic_compat)`.

### PS: `hefesto/plot/ps/`
`write_postscript(const Region&, const PlotTech&, const Params&, const PlotContext&, std::ostream&) -> diag::Outcome<void>`; `escape_ps_text(std::string_view) -> std::string`.

### PNM: `hefesto/plot/pnm/`
`write_pnm(const Region&, const PlotTech&, const Params&, const PlotContext&, std::ostream&) -> diag::Outcome<PnmInfo>`; `PnmInfo` holds the image size and the downsampling applied.

### EXPORT: `hefesto/display/export/`
`export_svg(const LayoutScene&, const ExportOptions&, std::ostream&) -> diag::Outcome<void>`; `export_png(const LayoutScene&, const ExportOptions&, std::ostream&) -> diag::Outcome<void>`. `ExportOptions` holds the pixel size, background, grid and box flags and the transparent-background flag.

### MZTECH: `hefesto/route/tech/`
`read_mzrouter_section(const RawSection&, const TypeTable&, TechDiagnostics&) -> MazeStyles`; `derive_after_load(MazeStyles&, const drc::RuleSet&, const lef::LayerTable&, bool magic_compat)`; `find_style(const MazeStyles&, std::string_view) -> const MazeStyle*`.

### MZ: `hefesto/route/maze/`
`route(const RouteRequest&) -> RouteResult` (D4). `RouteRequest`, `RouteResult`, `ResultCode`, `RunControls`, `SearchStats` in `route/maze/types.hpp`.

### IROUTE: `hefesto/route/iroute/`
`register_iroute_commands(cmd::Registry&)`; `IrouteSession` with `reset_from(const MazeStyles&)`; `save_parameters(const IrouteSession&, const TypeTable&, bool magic_compat, std::ostream&)`.

### MAZETOOL: `hefesto/tools/`
`register_tool_commands` gains `mazeroute`; `route_net_file(std::istream&, CommandRunner&) -> MazeRouteSummary`.

## Risks / Trade-offs

- **R1. The maze engine is large and order-sensitive.** About 10 000 lines must reproduce Magic's visit order. Mitigation: goldens first (G1-ROUTE); LEG-MZ-13 (`dumpEstimates`) and the `iroute verbosity 2` statistics pin intermediate state, not only final paint; the port follows Magic's file structure so reviewers can compare function by function.
- **R2. Compat branches inside the search.** Four OFF behaviours (Q-LEGACY-33, -85, -87, -89) double some test effort. Mitigation: one branch point each (D5); if the user prefers replicate (J5 (b)), they disappear without other changes.
- **R3. PNM bytes across platforms.** Mitigation: the committed Lanczos table, `-ffp-contract=off` and the three-platform golden runs (Q-MISC-54). If a platform still differs, the case gets an approved deviation, never a silent tolerance.
- **R4. The derived style routes poorly on some PDK.** Its costs come from scmos. Mitigation: it is OFF-only and overridable by any `irouter` style in the technology or a user's technology overlay; the `route-bench` sky130A workload and M11's benchmark measure it; costs can be tuned without a spec change because the requirement fixes the rule, and a change of values would be a spec delta in M11.
- **R5. The Cairo variant is brittle.** Magic under Xvfb with Cairo may crash at random, as the OpenGL variant did with `.magic3d`. Mitigation: only two cases use it; if it cannot be stabilised, MISC-PLOT-12 and -13 become Hefesto-only cases with a recorded decision (Q-MISC-52 (b)).
- **R6. `-timeout` in SCRIPT-AUX-02.** A slow runner could time out where Magic did not. Mitigation: the case's routes take milliseconds; `router.replay-bloom-budget` exists if needed.

## Migration Plan

No user data changes. Technologies with `plot` or `mzrouter` sections, which loaded before as raw sections, are now interpreted; a malformed section that Magic accepted with a crash risk now reports the line (Q-MISC-50, Q-LEGACY-39). Sessions gain the plot parameters and the `iroute` session state; nothing is persisted in projects.

## Open Questions

None is decided. Each one's recommended option is (a).

1. **How spec changes are recorded.**
   - (a) *Recommended:* as this change's `specs/` deltas, synced into the living specs at G0, then `skip_specs: true` (M9's approach).
   - (b) Edit the living specs directly after approval (the M7 and M8 approach).
2. **When this change starts relative to the 3D view.**
   - (a) *Recommended:* R0 now; R1 as soon as G0 passes, in parallel with the 3D view's P2 (no shared files, D1).
   - (b) After the 3D view's P2 exit.
3. **Desktop scope.**
   - (a) *Recommended:* the export dialog and the maze helper panel, with DESIGN boards first (D17).
   - (b) Console only in M10; both UIs in M13 with the Desktop 1.0 polish.

## Decisions this design depends on

| Decision (all pending) | Recommended option | Design sections it shapes |
|---|---|---|
| J1: D-37, D-38 view export ownership | `hefesto-rendering` owns SVG and PNG export; delivered here | D3, D4, D10, D17 |
| J2: Q-LEGACY-37 default `irouter` style | derived from the `lef` section, `magic-compat` OFF only | D5, D13, D23 |
| J3: Q-MISC-52 SVG reference | native SVG, `magic-cairo` capture variant for name and size | D2, D10, D19 |
| J4: router engine interface for M11 | session-scoped, structured result, bloom budget | D4, D11, D16, D23 |
| J5: route- and plot-changing Magic bugs | compat (Q-MISC-41, -43; Q-LEGACY-33, -85, -87, -89) | D5, D8, D11 |
| J6: maze helper window | native desktop panel | D15, D17 |
| Q-MISC-51 plot devices | default Magic build plus `svg` | D6, D7 |
| Q-MISC-42, -48, -49, -50, -53 | fix in both modes | D7, D8, D9, D18 |
| Q-MISC-44 platform | embed the prologue, bundle the mraster files, no spooling | D2, D6, D8 |
| Q-MISC-54 cross-platform bytes | committed Lanczos table, `-ffp-contract=off` | D8, D9, D20 |
| Q-MISC-55, Q-LEGACY-42, -43, -45 | compat | D5, D9, D12 |
| Q-LEGACY-38, -39, -40, -41, -47, -48 | fix (Q-LEGACY-40 verify) | D11, D12, D14, D18 |
| Q-LEGACY-44 | verify | D5, D19 |
| Q-LEGACY-6, -32, -86, -88, -91, -92, -94, -95, -96, -46; Q-MISC-47 | replicate | D9, D11, D12 |
| Q-LEGACY-31, -93; Q-MISC-45 | fix; remove; n/a | D6, D11 |
| Open Questions 1–3 | (a) | D1, D17 |
