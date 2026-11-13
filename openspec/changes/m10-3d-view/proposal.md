# Proposal: M10 3D view (view-3d, hefesto-3d-visualization)

> **Status: plan drafted 2026-11-13; its decisions approved by the user the same day.** The open questions Q-MISC-70, 71, 72 and 74, the amendment of Q-MISC-73, the new entries Q-MISC-76, 77 and 78, the `render3d` cut-box spec correction and the judgement items J1 and J2 (`docs/decisions/m10-3d-decision-table.md`) were approved as drafted, and design Open Questions 3 and 4 were decided (a). Every place in this change that depends on one of them names it; the table at the end of design.md lists the D-numbers each one shapes.

## Why

Hefesto has no 3D view. The user decided on 2026-11-13 to resolve the 3D open questions and plan the 3D work now, so that code can start as soon as M5's mask-generation workstream (GEN) lands, with a first slice from layout (magic) layers. The parasitic and field overlays need M7 extraction and wait for it.

What exists today (2026-11-13, main at 3fff5567):
- **Core.** `ui::ClientId::wind3d` exists (`ui/client.hpp:27`), and the `wind3d` key macros of `magic/proto.magicrc` are in `ui/bindings.cpp`. `specialopen wind3d` is not wired: `view/window_ops.cpp` opens `color` but not `wind3d`, and answers `UI-CLIENT-002` ("`wind3d` until M10", `docs/diagnostics/codes/ui.md`).
- **Technology.** The `cifoutput` and `extract` sections are kept as raw lines (`tech/loader.cpp`). The M3 `drc::CifCatalog` skips `render`. Extract `height`, `planeorder` and `units` are not parsed: that reader (`extract-tech-section`) belongs to M7.
- **Renderer.** `src/render/vulkan/` (about 3.1k lines) is 2D only: `CanvasRenderer` draws instanced rectangles and polygons through a `ViewTransform`, records into Raven's render pass through the viewport draw callback, which has one color attachment and no depth (Raven v0.2.2 `raven_viewport.h:53`), and has no 3D matrices. `headless.cpp` renders offscreen, which image tests can reuse.
- **Desktop.** "3D" is a disabled editor-view tab ("3D inspection: arrives in M10", `view_models.cpp:410`); Window > "3D Display" (`magic::render3d`) is disabled as a later-milestone item (`menu_bar_model.cpp:187`, `later_milestone()`). The reference board is `docs/design/mockups/View3D.dc.html` (marked M10), matching the design system's "3D inspection" pattern.
- **Connectivity and selection.** `connect/search.hpp` returns the tiles and labels of a net, flat or hierarchical, on demand. `select/selection.hpp` holds the 2D selection with provenance.
- **Golden.** `tests/golden/MISC/CASES.md` has MISC-3D-01..04 (text outputs, not authored). The capture image builds Magic with `--with-opengl=no` (`tools/golden/container/Containerfile:86`), so `wind3d` does not exist there.
- **M5 (planned on main, `openspec/changes/m5-mask-gds-cif`, merged by PR #44; GEN not yet implemented).** STYLES parses the `render` statement (task 3.1); the `mask-output-styles` spec calls it "3-D display attributes only", and keeping the values on `mask::OutputStyle` is not stated. GEN provides `generate_flat(EditorState&, const CellDef&, Rect, const OutputStyle&, GenerationContext&) -> LayerPlanes` with the halo, progress and cancellation: exactly what Magic's mask-layer mode needs.

## What Changes

### P1: the slice from layout layers (starts after M5 GEN lands)

- **Decisions recorded (DECIDE, docs only, done 2026-11-13).** After the user's approval: `Resolution:` lines for Q-MISC-70..78 and the spec text of the decision table's "Spec changes after approval", including J1 and J2 in `hefesto-3d-visualization` and the `render3d` cut-box correction in `view-3d`.
- **Layer stack data (TECH).** One drawable entry per (tile type, display style), since Magic draws a type once per style (Q-MISC-77). The extract-style subset that the layout-layer view needs (style and variant selection, scale, `units`, `planeorder`, `noplaneordering`, `height`, the 0.05 / 0.1·order defaults and the micron conversion), written once in the home of `extract-tech-section`, which M7 extends. The `render` values (style index, height, thickness) kept per mask layer on `mask::OutputStyle`, with the style resolved against the display styles; M5 STYLES is asked to store them, otherwise TECH adds them.
- **Mesh model (MESH).** A headless, backend-free core module that builds per-layer prism meshes from flattened layout tiles: top and bottom faces, side faces on tile boundaries, triangles for split tiles, closed faces at the cut box, clipping in root coordinates (Q-MISC-70). Built in the background, by spatial chunk, with progress and cancellation; deterministic order.
- **View state and camera (CAMERA).** Magic's view state and math exactly as "Default view" and "3D view commands" specify (both modes, Q-MISC-72 OFF conversion), the viewing transform for the current window size (Q-MISC-74), and Hefesto's orbit, pan, zoom and presets (top, isometric, front, side) as operations on the same state. Orthographic by default, perspective as a view option.
- **3D renderer (RENDER3D).** A depth-tested (Q-MISC-77) mesh pipeline with Magic's lighting, rendering offscreen into color, depth and pick-id images on Raven's Vulkan device, shown through the viewport's texture mode. Headless image tests.
- **Window and commands (WIND3D).** `specialopen wind3d`, `render3d` (cut box copied once, as Magic), every command of "3D view commands" with per-window mode (Q-MISC-76), per-mode visibility sets (Q-MISC-71), the no-output-style rules (Q-MISC-73) and the output-style rescale rule (Q-MISC-78), and the `wind3d` key macros. Magic's mode state is exact from P1: the window opens in mask-layer mode when an output style exists and `cif` toggles both ways, so every command value matches Magic; until P2, mask-layer mode draws the layout-layer mesh scaled by f with a notice.
- **Desktop (DESIGN, SHELL).** The 3D tab and Window > 3D Display enabled; the 3D inspection screen per `View3D.dc.html`: layer stack with µm ranges, visibility, isolate and transparency, camera presets, "Show contacts and vias" (J1), gestures; parts of later phases shown disabled with their phase.
- **Inspection (INSPECT, P1 part).** Picking through the id image, linked both ways with the 2D selection; net highlighting through `layout-connectivity`; per-layer show, hide, isolate and transparency.

### P2: mask-layer source, cross-sections and export

- **Mask-layer source (MASKSRC).** Mask-layer mode draws the meshes of `mask::generate_flat` over the cell bbox or cut box grown by the style's radius, with the `render` values (Q-MISC-72 compat; layers without `render` have no physical height and are reported in µm-based features), the mask-layer visibility set and the mesh builder's plane input, replacing the P1 placeholder.
- **Cross-sections (INSPECT, P2 part).** An axis-aligned cut line or plane: a 2D section panel of the stack along the line, in µm, and a clipping plane in the 3D view.
- **Export (EXPORT).** `export3d` to glTF 2.0 (`.gltf` + `.bin`, `.glb`) through cgltf, added to the build with its notice, and binary STL (per layer or merged) by Hefesto's own writer; physical µm (J2); byte-identical on every platform.

### P3: parasitic and field overlays (after M7)

- **Overlays (OVERLAY).** Node capacitance color map, coupling markers with a threshold, resistance networks from `extresist-*`, current density from imported simulation results, with legends, units and ranges, values read from the extraction and simulation outputs and never recomputed; IR-drop and EM datasets come from `reliability-visualization`.

### Across phases

- **Golden and verification (GOLDEN).** A second capture image with an OpenGL Magic (Mesa under Xvfb) for MISC-3D-01..04 and new 3D cases; an informative face-set comparison against Magic's emitted geometry; Hefesto-only image tests.
- **Performance (PERF).** A `view3d-bench` for build time, memory and frame rate against `hefesto-performance` ("at least 30 frames per second for a meshed region of up to 1 million rectangles").
- **Integration and exit (INTEGRATION).** Per-phase exit checks, docs, roadmap and CHANGELOG.

### Non-goals

- The rest of M10: `plot-output`, `hefesto-collaboration-and-diff`, the routers, the reliability capabilities (except as the data source of P3 overlays) and the digital flow.
- The iPadOS app and the Metal backend (M14). M10 keeps the 3D core backend-free and the gestures abstract so M14 only adds the Metal backend and the SwiftUI screen (design D11).
- Pixel identity with Magic. The viewing transform is informative; parity is checked on command outputs and on geometry (design D13).
- New extraction semantics. TECH reads only the extract keywords the 3D view needs; the rest of `extract-tech-section` stays in M7.
- Editing in the 3D view. Picking selects; it does not paint or move.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

None in this change. This change implements the existing requirements of `view-3d` (8 requirements) and `hefesto-3d-visualization` (5 requirements). The spec edits listed in `docs/decisions/m10-3d-decision-table.md` "Spec changes after approval" were made by task 1.2 directly in the living specs after the user approved them on 2026-11-13, so the change sets `skip_specs: true`, following the M0–M5 precedent (m0-foundations design D10). TECH also implements, ahead of M7, the requirements of `extract-tech-section` that the layer stack needs (design D5); they are tested under that capability's tags and M7 extends the same code.

## Impact

- **New code (core, backend-free):** `src/core/{include/hefesto,src}/view3d/{stack,mesh,camera,client,pick,section,export,overlay}/`, `src/core/{include/hefesto,src}/extract/tech/` (the subset), `tests/unit/view3d/`, `tests/unit/extract/tech/`.
- **New code (renderer and shell):** `src/render/vulkan/src/mesh3d_*.cpp` with `hefesto/render/mesh3d_renderer.hpp`; `src/shell/desktop/src/view3d_*.cpp`.
- **Shared changes:** `view/window_ops.cpp` opens `wind3d`; `mask::OutputStyle` gains the render attributes (if M5 does not add them); `tech/loader.cpp` hands the `extract` raw lines to the subset reader; the desktop view tabs and menu bar enable the 3D items.
- **Dependencies:** cgltf (MIT, header-only, approved in `hefesto-licensing`), added in P2 with its entry in `THIRD_PARTY_NOTICES.md`.
- **Golden:** MISC-3D-01..04 and new MISC-3D cases; a `magic-gl` capture image variant.
- **Docs:** `docs/diagnostics/codes/view3d.md`, `docs/design/m10-3d-ui-conformance.md`, `docs/contributing/` (areas, the GL capture image), roadmap status and CHANGELOG.

## Dependencies

| Phase | Needs | From |
|---|---|---|
| P1 | user approval of the decision table (Q-MISC-70..78, J1, J2): done 2026-11-13 | DECIDE (task 1.1) |
| P1 | `mask::OutputStyle` and the `render` statement parsed; current output style selection and scalefactor | M5 STYLES (m5 tasks 3.1, 3.4) |
| P1 start | M5 GEN merged (`generate_flat`, `GenerationContext`; `Progress` and cancellation) | M5 GEN (m5 tasks 6.1 and 6.6), per the user's decision |
| P1 | the extract-style subset | TECH (this change), extended by M7 `extract-tech-section` |
| P1 | flat and hierarchical net search | `layout-connectivity` (M2, exists) |
| P2 | `generate_flat` over an area, halo radius | M5 GEN, STYLES |
| P3 | `.ext` capacitances, extresist networks | M7 (`extract-*`, `extresist-*`) |
| P3 | simulation currents and voltages | M9 (`simulation-analyses-and-results`) |
| P3 | IR-drop and EM datasets | M10 reliability (`reliability-visualization`) |
