# Design: M10 3D view (view-3d, hefesto-3d-visualization)

> Drafted 2026-11-13 and revised after review the same day, against the recommended answers of `docs/decisions/m10-3d-decision-table.md`. **The user approved them on 2026-11-13 as written**, with option (a) for J1, J2 and Open Questions 3 and 4, so no design section changed. The table "Decisions this design depends on" at the end lists, for each decision, the D-numbers it shapes.

## Context

See proposal.md for motivation and what exists today. The conventions of M0–M5 hold:
- SPDX headers;
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`;
- per-area diagnostic code tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`;
- cognitive complexity at most 15 per function in the core and 8–12 in the shell view models (flag irreducible ones);
- no UI, GPU, Raven or process headers in the core (`scripts/check_core_includes.py`);
- golden expected outputs captured only in CI;
- Magic 8.3.694 at fc5daa9e as the behavioral reference;
- golden cases run with magic-compat ON, and OFF behaviors are unit-tested.

Magic's 3D window is `graphics/W3Dmain.c` (about 1,900 lines, OpenGL 1.x immediate mode, Tk/GLX). Its observable contract is small: the `render` statement, the command outputs (`view`, `scroll`, `zoom`, `render name`, `cutbox`, errors), the default view values and the geometry it draws. Pixels are informative (view-3d "Viewing transform and shading"). Hefesto's own requirements (`hefesto-3d-visualization`) are native and much larger: inspection, export, overlays.

## Goals / Non-Goals

**Goals:**
- Every requirement of `view-3d` (8) and `hefesto-3d-visualization` (5), in three phases (D1).
- One mesh model, one view state and one pick mapping, shared by the Magic commands, the desktop screen, export and (later) the iPadOS app.
- Every compat decision (Q-MISC-72, 78) has one branch point, testable in both modes.
- The performance requirement: at least 30 frames per second for a meshed region of up to 1 million rectangles (`hefesto-performance`).

**Non-Goals:** as in proposal.md "Non-goals": the rest of M10, the Metal backend and iPadOS app (M14), pixel identity with Magic, extraction semantics beyond the layer stack, and editing in 3D.

## Decisions

### D1. Phases and gates

| Phase | Starts when | Delivers | Exit |
|---|---|---|---|
| P0 (DECIDE) | now | the user's approval of the decision table; `Resolution:` lines; the spec text | `check_milestone_readiness.py --milestone M10` lists no 3D entry; `openspec validate --all --strict` |
| P1 slice | M5 GEN merged (user decision 2026-11-13) and P0 approved | layout-layer source, `wind3d` and every command, camera and presets, renderer, desktop screen, visibility, picking, net highlighting | D19 "P1 exit" |
| P2 | P1 exit | mask-layer source, cross-sections, glTF/STL export | D19 "P2 exit" |
| P3 | M7 extraction merged (and M9 simulation results for current density) | parasitic and field overlays | D19 "P3 exit": the 3D part of the M10 exit |

P1 needs nothing from GEN except the start gate the user set; it does need M5 STYLES (the parsed `render` statement, the current output style and its scalefactor) for `render name`, `cutbox` and the Q-MISC-73 rules. P2's mask-layer source is the one part that calls GEN. Phasing keeps the first slice useful (layout layers are what users see in 2D) and lets the mask path start from a working mesh, camera and renderer.

P1 implements Magic's mode state exactly: the window opens in mask-layer mode when a current output style exists (otherwise layout-layer mode, Q-MISC-73 amendment), `cif` toggles and rescales by f (Q-MISC-78), and every command returns Magic's values in both modes. Only the mask-layer drawing waits for P2: in mask-layer mode P1 draws the layout-layer mesh with xy scaled by f (the z factors of the f rescale cancel, so its heights are unchanged) and the screen shows a "mask layers arrive in phase 2" notice. Opening directly in layout-layer mode was rejected: Magic's default view carries the f factor (W3Dmain.c:1374-1376) and `cif` divides it out in `float` (:890), so `scroll ... rel` (MISC-3D-03) and the `cif` round trip (MISC-3D-02) would not match. No `CORE-NOTIMPL` code and no MISC-3D deviation are needed for the mode.

### D2. Layout of the 3D code

| Module | Path | Backend-free | Owner |
|---|---|---|---|
| Layer stack | `src/core/{include/hefesto,src}/view3d/stack/` | yes | TECH |
| Extract subset | `src/core/{include/hefesto,src}/extract/tech/` | yes | TECH (M7 extends) |
| Mesh model and builders | `view3d/mesh/` | yes | MESH |
| View state, camera, presets, gestures | `view3d/camera/` | yes | CAMERA |
| `wind3d` client and commands | `view3d/client/` | yes | WIND3D |
| Pick mapping, net highlight sets | `view3d/pick/` | yes | INSPECT |
| Cross-sections | `view3d/section/` | yes | INSPECT (P2) |
| glTF and STL writers | `view3d/export/` | yes | EXPORT (P2) |
| Overlay datasets and color maps | `view3d/overlay/` | yes | OVERLAY (P3) |
| Vulkan mesh renderer | `src/render/vulkan/src/mesh3d_*.cpp`, `include/hefesto/render/mesh3d_renderer.hpp` | no | RENDER3D |
| Desktop screen | `src/shell/desktop/src/view3d_*.cpp` | no | SHELL |

Each core subdirectory has its `sources.cmake` and `codes.inc`; the area `view3d` has one code document, `docs/diagnostics/codes/view3d.md` (prefix `VIEW3D-`). Everything under `src/core` stays free of GPU and UI headers, so the iPadOS app (M14) links the same mesh, camera, pick and export code and adds only a Metal backend (D11).

### D3. Workstreams and ownership

| Workstream (tasks.md section) | Phase | Capabilities | Owns (may edit) |
|---|---|---|---|
| DECIDE | P0 | — | `docs/decisions/open-questions/MISC.md` (3D entries), `docs/decisions/m10-3d-decision-table.md`, `openspec/specs/{view-3d,hefesto-3d-visualization}/spec.md` |
| SCAFFOLD | P1 | — | the module skeletons of D2, `docs/diagnostics/codes/view3d.md` |
| TECH | P1 | `view-3d` "Render statement", "Layer height and thickness"; the `extract-tech-section` subset | `extract/tech/`, `view3d/stack/`, `tech/loader.cpp` (hand-off of the `extract` raw lines), `mask/style/` render attributes only if M5 did not add them (D5) |
| MESH | P1 (P2 plane input) | `view-3d` "Rendered geometry"; `hefesto-3d-visualization` "Mesh generation from layout" | `view3d/mesh/` |
| CAMERA | P1 | `view-3d` "Default view", "Viewing transform and shading"; the camera part of "Interactive 3D inspection" | `view3d/camera/` |
| RENDER3D | P1 | `view-3d` "Viewing transform and shading", "Platform mechanism"; `hefesto-rendering` (3D use) | `src/render/vulkan/` mesh3d files, the render interface additions (depth, offscreen target, id image) |
| WIND3D | P1 | `view-3d` "Opening the 3D window", "3D view commands" | `view3d/client/`, `view/window_ops.cpp` (`wind3d` case), the `UI-CLIENT-002` `wind3d` path and its code-table row, the `render3d` wrapper procedure |
| DESIGN | P1 | `hefesto-design-system` "3D inspection" | `docs/design/mockups/View3D.dc.html`, `docs/design/m10-3d-ui-conformance.md` |
| SHELL | P1 (P2/P3 parts) | `hefesto-3d-visualization` (desktop) | `src/shell/desktop/src/view3d_*.cpp`, the 3D tab in `view_models.cpp`, the `render3d` entry in `menu_bar_model.cpp` |
| INSPECT | P1 (cross-sections P2) | `hefesto-3d-visualization` "Interactive 3D inspection" | `view3d/pick/`, `view3d/section/` |
| MASKSRC | P2 | `view-3d` "Rendered geometry" (mask-layer mode), `hefesto-3d-visualization` "Mesh generation from layout" (mask source) | `view3d/mesh/` plane input, `view3d/client/` mode switch |
| EXPORT | P2 | `hefesto-3d-visualization` "3D export" | `view3d/export/`, `cmake/Dependencies.cmake` (cgltf), `THIRD_PARTY_NOTICES.md` |
| OVERLAY | P3 | `hefesto-3d-visualization` "Parasitic and field overlays" | `view3d/overlay/`, the overlay parts of the shell |
| GOLDEN | P1–P3 | the MISC-3D cases | `tests/golden/MISC/`, `tools/golden/container/` (the `magic-gl` variant), `tools/golden/gl_capture/` |
| PERF | P1 (P2 re-run) | `hefesto-performance` 3D frame rate | `bench/view3d/`, `perf.yml` `view3d-bench` |
| INTEGRATION | each phase | exit checks | roadmap, CHANGELOG, `docs/contributing/` |

Order within P1: SCAFFOLD → TECH, CAMERA and the RENDER3D spike (task 5.1) in parallel → MESH → WIND3D and RENDER3D → DESIGN review → SHELL → INSPECT → PERF → INTEGRATION.

### D4. Session state and shared interfaces

- `view3d::Session` in `EditorState`: the at-most-one 3D window (view-3d "Only one 3D window allowed."), holding a `view3d::WindowState` per open window: rendered cell, mode (layout-layer or mask-layer; per window, Q-MISC-76), the f applied on entering mask-layer mode (Q-MISC-78), level, `ViewState` (D7), cut box, the two visibility sets (Q-MISC-71), and the current `MeshModel` handle.
- `view3d::LayerStack`: for each drawable layer (tile type or mask layer) its display styles (a tile type may belong to several, D5), h and t in internal units for drawing, h and t in µm for Hefesto's extensions, and the source of each value (extract, render, default). Built from `extract::tech::ExtractStyle` (layout-layer mode) or `mask::OutputStyle` (mask-layer mode) and the magic-compat setting (Q-MISC-72).
- `view3d::MeshModel`: immutable, shared (`std::shared_ptr<const ...>`) per build; chunks (D6) with a version so the renderer uploads only changed chunks.
- `view3d::PickId` ↔ `view3d::PickTarget` (D10).
- Change notification: the mesh builder subscribes to the database's existing change events for the rendered cell's subtree and rebuilds only the chunks whose area changed (D6). The desktop's 2D canvas uses the same events.
- Threading: builds run on a worker with a snapshot of the flattened tiles taken under the shared lock, as DRC does; the UI thread never waits on a build.

### D5. Layer stack data (TECH; Q-MISC-72, 77)

**Extract subset.** The layout-layer view needs h and t per tile type from the current extract style (view-3d "Layer height and thickness for the layout-layer view"). TECH writes the part of the `extract-tech-section` reader that produces them, in that capability's home `extract/tech/`, with its requirement tags: `style` and variants, `scale`/`lambda`, `units`, `planeorder`, `noplaneordering`, `height`, the end-of-style plane-order check and its warnings, the defaults (t = 0.05, h = 0.1 · order) and the conversion by dscale with `units microns` (ExtTech.c:3962-3966, rescale on grid change :4090-4093). Every other keyword is accepted and skipped silently, so a full PDK extract section loads without diagnostics that M7's reader would not also give. M7 replaces the skip with its parsers in the same files; there is one extract reader.

**Render attributes.** The `render` statement is parsed by M5 STYLES (m5 task 3.1). The 3D view needs, per mask layer of each output style: the render style as a position in the technology style table, height and thickness as `float` stored as given (view-3d "Render statement in the cifoutput section"), and the default pale-highlight style with h = t = 0. Task 1.4 asks M5 to keep these on `mask::OutputStyle` (`RenderAttrs`); if M5 does not, TECH adds them in `mask/style/` with STYLES' agreement (D18).

**Units (Q-MISC-72, compat).** The branch point is one function, `LayerStack::draw_height(layer)`:
- layout-layer mode: extract h, t in internal units, both modes;
- mask-layer mode, magic-compat ON: render h, t as stored;
- mask-layer mode, magic-compat OFF: render h, t divided by µm per internal unit = `cs_scaleFactor / (100 · cs_expander)` of the current output style.

The physical µm values used by cross-sections, export, the layer-stack panel and overlays come from a second function that never depends on the mode or the setting. `zoom` reports s_z unchanged in both settings.

**No `render` line (Q-MISC-72).** Reading `render` values as µm rests on PDK convention only (W3Dmain.c:1239-1241). A mask layer without a `render` line has no physical height: `physical_height_um` returns "none", it is drawn flat (h = t = 0) as Magic does, and cross-sections and export from mask-layer mode leave it out with the warning `VIEW3D-STACK-001` naming the layers (gf180mcuD has no `render` lines at all, so its mask-layer export is empty and the warning suggests layout-layer mode). Hefesto does not guess heights from the layout-layer stack: mask layers do not map one-to-one onto tile types.

**Several styles per type (Q-MISC-77).** Magic draws a tile once for each display style its type belongs to (W3Dmain.c:1692-1702; sky130A contacts such as `pc` and `mcon` are in two styles). The stack's drawable unit is therefore the pair (type, style), in style-table order; each pair is its own prism, and D8's less-or-equal depth test lets the last style drawn win on the coplanar faces, as Magic's draw order does. Picking maps a pair back to its type.

### D6. Mesh building, memory and frame rate (MESH; Q-MISC-70)

**Input.** Layout-layer mode flattens the rendered cell's subtree through instance transforms (the same flat tile search DRC and `connect` use), clipped in root coordinates to the cut box, or to the window area grown by 1 (Q-MISC-70 fix, both modes). Mask-layer mode (P2) takes `LayerPlanes` from GEN, already flat in root coordinates.

**Halo** (`hefesto-3d-visualization` "Region model"). The configured halo is 0 for the layout-layer source, which is clipped exactly to the cut box as Magic does, and the output style's interaction radius for the mask-layer source (W3Dmain.c:1745), so mask operators see their neighbourhood; the mesh is still clipped to the cut box.

**Geometry.** For each visible, drawable (solid or stippled) (type, style) pair (D5):
- top and bottom faces from tiles, as axis-aligned rectangles;
- side faces only on boundary segments where the neighbour is a different layer or the clip edge, from a boundary walk of the layer's tiles; faces cut by the cut box are closed;
- split tiles as triangles with a diagonal side face;
- z_top = −h·s_z and z_bot = z_top − t·s_z are applied in the vertex shader from the layer's h, t and the view's s_z, so `zoom` in z never rebuilds the mesh.

Prisms with h and t both zero (gf180mcuD in mask-layer mode) still draw top faces, as Magic does.

**Encoding.** Rectangles and side segments are instanced, not expanded into triangles: a top/bottom instance is four `float32` corner coordinates relative to its chunk origin plus a 16-bit layer index (20 bytes); a side instance is a segment (16 bytes) plus layer and outward normal flags. Chunk origins are `double` on the CPU; GPU coordinates are chunk-relative `float32`, so a 2³¹-unit chip keeps sub-unit precision. Split-tile triangles go to a small per-chunk triangle buffer.

**Chunks.** The region is divided into square chunks sized so that each holds at most about 65,536 tiles (split adaptively, as a quadtree, on the first build). Chunks are the unit of rebuild after an edit, of frustum culling and of progressive display: the first build shows chunks as they finish, nearest the view centre first, and reports progress; cancellation drops unfinished chunks and leaves the previous model visible.

**Determinism.** Instances are emitted in a fixed order: layer order (style-table order in layout-layer mode, mask-layer order in mask-layer mode), then chunk order (Morton order of chunk index), then tile order of the plane's own enumeration. The mesh is therefore identical across runs and platforms, which export (D14) relies on.

**Targets** (measured by PERF on the `hefesto-performance` reference hardware; the M3 Pro development machine records early numbers):

| Metric | Target |
|---|---|
| Frame rate, 1M meshed rectangles, isometric view, whole region visible | ≥ 30 fps on every reference machine (spec); goal 60 fps on the M1 Mac |
| GPU memory | ≤ 96 bytes per meshed rectangle on average (≤ 96 MB per million); an isolated rectangle costs one 20-byte rectangle instance plus four side instances of about 18 bytes. Deriving the sides of fully isolated tiles in the shader is an optimisation PERF may take, not a target |
| CPU memory of the model | ≤ 64 bytes per meshed rectangle, excluding the database |
| Build time | ≤ 1.5 s for 1M rectangles off the UI thread on the M1 Mac; first chunks on screen within 200 ms |
| Incremental rebuild after a local edit | ≤ 50 ms for the affected chunks |
| Memory pressure (iPadOS, M14) | meshes are droppable caches, rebuilt on demand (`hefesto-performance` memory warning) |

### D7. View state, camera and presets (CAMERA; Q-MISC-74, 78)

**Magic's state is the state.** `ViewState` holds exactly Magic's values: angles (ax, ay, az), translation (tx, ty, tz), s_xy, s_z, p_z, with the default view and the f rescale of "Default view" (integer halving by arithmetic shift, `float` arithmetic as Magic's `float` fields, so `view`, `scroll` and `zoom` print the same values). Every Hefesto camera operation is an operation on this state, so the commands always report the camera the user sees:
- orbit: changes ax and az (pointer drag, one-finger drag);
- pan: changes tx, ty in screen units, as `scroll ... rel`;
- zoom: multiplies s_xy (wheel, pinch), and s_z with a modifier;
- presets: top (0, 0, 0), isometric (−54.7356, 0, 45), front (−90, 0, 0), side (−90, 0, −90) as absolute `view` angles; DESIGN confirms them on the board.

**Output style changes (Q-MISC-78, compat).** `rescale` is called with the current output style's f (magic-compat ON, as Magic), or with the f the window recorded on entering mask-layer mode (OFF); with OFF, a change of output style while in mask-layer mode rescales the view by f_new / f_old. One branch point: `WindowState::exit_mask_factor()`.

**Transform.** v' = A · S · Rx · Ry · Rz · T · v as the spec states, with A = diag(H_w / W_w, 1, 1) from the drawable's current size (Q-MISC-74 fix; the initial size request stays 500 × 500).

**Projection.** Orthographic by default, as Magic. Perspective is a Hefesto view option (a field of view setting on the screen; not a Magic command and not part of `ViewState`), applied after the same transform; picking and cross-sections work in both.

**Gestures.** `view3d::camera::Gesture` is an abstract event (orbit, pan, zoom, preset, reset) with deltas in view units. The desktop maps mouse, trackpad and keys to it; the iPadOS app (M14) maps one-finger drag to orbit and pinch to zoom (spec scenario "Touch orbit"). The key macros of `proto.magicrc` stay commands.

### D8. Renderer integration (RENDER3D; Q-MISC-77)

**Raven texture mode.** The 2D canvas records into Raven's render pass through the viewport draw callback, which has one color attachment and no depth (Raven v0.2.2 `include/raven/raven_viewport.h:53`). The 3D view uses the viewport's texture mode instead (`raven_viewport.h:8-16`): it renders offscreen into its own images on Raven's Vulkan device (`Rvulkan_get_device`, already used by `raven_ui.cpp:311`), and shows the color image with `Rvulkan_texture_create_from_image_view` / `Rvulkan_texture_set_image_view` (`raven_vulkan.h:111-117`, zero copy) and `Rviewport_set_texture`. Submission uses the graphics queue under the existing queue-lock callbacks (`raven_ui.cpp:275`). `hefesto-rendering` already allows "into a texture that the viewport presents". This needs no Raven change, and Raven can draw UI on top of the view (legend, presets). Task 5.1 is a spike that confirms it on macOS (MoltenVK) and Linux (lavapipe) before the pipeline is written.

**Images.** Color: an sRGB-encoded image (`R8G8B8A8_SRGB` with `MUTABLE_FORMAT`), rendered through an `_SRGB` view and shown through a `_UNORM` view, because `Rvulkan_texture_create_from_image_view` expects a UNORM view of an sRGB-encoded image (the doc comment before `raven_vulkan.h:111`); task 5.1 records the format it chose. Depth (`D32_SFLOAT`), and a pick-id image (`R32_UINT`), sized to the viewport's pixel size from the resize callback; double-buffered so the texture shown is never the one being drawn.

**Pipeline.** Instanced rectangle and side-segment pipelines plus a triangle pipeline, depth-tested with `LESS_OR_EQUAL` in style-table order so the last style wins on coplanar faces (Q-MISC-77 fix, both modes; D5), back faces culled, lighting as the spec: ambient 0.4, one white diffuse light at (50, 50, 50), on the display style's color; anti-aliasing at level ≥ 1 by MSAA 4× (resolved before display). Only solid and stippled styles are drawn (W3Dmain.c:406, :439); Magic's OpenGL style setup applies the stipple as a GL polygon stipple (grTOGL1.c:236-240), while Hefesto draws stippled styles solid with the style color, since pixels are informative and a screen-space stipple on 3D faces does not read as depth. Transparency (INSPECT) uses a second, sorted-by-chunk pass without depth writes.

**Redraw.** On demand only (state change, build progress, resize), not realtime, matching the 2D canvas.

**Headless.** The same pipeline runs in `headless.cpp`'s offscreen device for image tests; tests compare against Hefesto's own reference images with a tolerance, never against Magic's pixels.

### D9. `wind3d` client and commands (WIND3D; Q-MISC-71, 73, 76, 78)

- `specialopen wind3d [cell [path]]` follows "Opening the 3D window": the one-window rule, cell resolution and errors, the default toolkit path `.magic3d`, level 1, no mouse actions. The Magic display-type check is not applied (Q-MISC-75, platform). Mode: mask-layer if a current output style exists, otherwise layout-layer (Q-MISC-73 amendment), from P1 on (D1). It replaces the `UI-CLIENT-002` answer that `specialopen wind3d` gives today (`docs/diagnostics/codes/ui.md`, "`wind3d` until M10"); the code-table row drops `wind3d` (D18).
- `render3d [cell]` is a `magic::` procedure, as the wrapper's: it opens the window with toolkit path `.render.magic`, runs `cutbox box` once (which copies the layout box only when it is in the rendered cell, W3Dmain.c:719-738) and sets `Opts(cutbox)` to 1; closing sets `Opts(wind3d)` to 0. The desktop shows it as the 3D tab (or a separate window, D12), and the screen's Cutbox toggle runs `cutbox box` or `cutbox none`, as the wrapper's checkbutton. The cut box does not follow later box moves (the spec's "following the layout box" is corrected in DECIDE); following the box would be a native desktop option, not planned.
- Commands: every row of "3D view commands" with Magic's argument parsing, usage messages and the `float` arithmetic of D7. `cif` rescales by f as specified (Q-MISC-78) and keeps the mode per window (Q-MISC-76). `see` acts on the current mode's visibility set; `defaults` resets both; a change of current output style resets the mask set (Q-MISC-71). `render` with no output style reports "No CIF output style set!" (Q-MISC-73 amendment). The commands are reachable as `.magic3d <cmd>` (the toolkit-path command of M4's binding) and from the command bar when the 3D view has focus.
- Key macros: the existing `wind3d` table of `ui/bindings.cpp`.

### D10. Inspection: visibility, picking, net highlighting, cross-sections (INSPECT; J1)

**Visibility.** The screen's per-layer show/hide edits the same per-mode set as `see` (Q-MISC-71); isolate is "hide all others"; transparency is a per-layer alpha kept outside Magic's state.

**Picking.** Each instance carries a 32-bit pick id: chunk-local index plus chunk id. `view3d::pick` maps an id back to a `PickTarget`: the layer, the root-coordinate rectangle or triangle, and, in layout-layer mode, the instance path and the tile's cell-local rectangle (kept in a per-chunk side table built with the mesh; not uploaded). A click reads one pixel of the id image.
- 3D → 2D: selecting a target selects that material in the 2D view, as `select area <type>` at the target's point would in the cell that holds it: in the edit cell directly; under a subcell, the subcell use (Magic's selection semantics for unexpanded material), with the target's path shown. The 2D view scrolls to it if it is outside the window.
- 2D → 3D: the 2D selection's paint (flattened, root coordinates, as `Selection` already provides) is drawn as a highlight in 3D by overlapping it with the mesh's instances per layer.

**Net highlighting.** From a picked target or the 2D selection, `connect::search_flat` (or the hierarchical search) gives the net's tiles; the 3D view draws them highlighted and dims the rest ("Isolate selected net" on the board). In mask-layer mode (P2) the layout-layer net tiles are drawn as an overlay over the mask geometry; mapping nets onto generated mask layers is not attempted.

**Contacts and vias (J1, decided (a) 2026-11-13).** By default contacts use their technology height and thickness, as Magic. The view option "Span connected layers" (the board's "Show contacts and vias" area) draws each contact type from the top of the lowest connected layer to the bottom of the highest, using its residues from `tech-contacts-and-stacking` and the stack's h and t.

**Cross-sections (P2).** A cut line drawn in the 2D or 3D view (axis-aligned; the board's A–A′) gives a 2D section panel: for each layer, the intervals of the line it covers, drawn from h to h + t in µm (D5), with the same colors; and an optional clipping plane in the 3D view. Built from the mesh model, so it costs no re-flattening.

### D11. iPadOS scope

The 3D view with overlays is in the first iPadOS scope (`hefesto-ipad-feature-scope`). M10 does not build for iPadOS. It guarantees that M14 needs only a Metal backend and SwiftUI views: the stack, mesh, camera, gestures, pick mapping, sections, export and overlay data are in `src/core` with no GPU or UI headers (checked by `check_core_includes.py`); the renderer is reached only through `hefesto/render/mesh3d_renderer.hpp`, whose interface names no Vulkan type; gestures are abstract (D7); meshes are droppable caches (D6). Export on iPadOS writes through the platform file picker, as other exports.

### D12. Desktop screen (DESIGN, SHELL)

The screen follows the design system's "3D inspection" pattern and its reference board `docs/design/mockups/View3D.dc.html`: the editor-view tab "3D" next to Layout and Schematic; a toolbar with Cross-section and Export glTF; a layer stack panel listing layers bottom to top with their µm ranges (D5) and visibility, isolate and transparency controls; the presets Isometric, Top, Front, Side; "Isolate selected net"; "Show contacts and vias" (J1); an overlay dropdown and legend; the A–A′ section panel. Layer colors come from the display styles, never from theme tokens. DESIGN (task 7.1) updates the board first where the plan changed it: the parity controls (level, mask/layout mode toggle), disabled P2/P3 parts with their phase, and the J1 option. SHELL builds it as a second Raven viewport, like `LayoutCanvas`. `render3d` opens the tab; "Open in separate window" is a tab menu item. The window's top-bar hiding order is unchanged (`hefesto-design-system`): the 3D tab is a later-milestone item only until P1 lands.

### D13. Golden and verification strategy for Magic parity

What is compared with Magic, and how:

1. **Command outputs (exact).** MISC-3D-01..04 and new cases (task 12.3: `cutbox` forms, `level` clamping, `render name h t style`, `see`/`defaults`, `cif` round trip values, Q-MISC-73 no-style errors as a deviation). They need a Magic with the 3D window. GOLDEN adds a `magic-gl` variant of the capture image: the same Magic commit built with `--with-opengl` against Mesa (llvmpipe, GLX) and run under Xvfb, used only by `MISC-3D-*` cases (Open Question 3, decided (a) 2026-11-13).
2. **Geometry (informative, strong).** A small GL interception shim (`tools/golden/gl_capture/`, LD_PRELOAD on `glBegin`/`glVertex3f`/`glEnd`/`glColor`) records Magic's emitted polygons for a case, normalised per (style, z) to the union of top and bottom faces and the union of covered side edges, not to a face multiset: Magic emits faces per tile and per cell (GrBoxOutline works in each cell's own plane, so tiles abutting across cells get inner side faces, and multi-style types are emitted once per style), while Hefesto's mesh walks the flattened boundaries. Hefesto's mesh is reduced to the same form. Cases avoid the fixed bugs (no transformed subcell under a clip area, Q-MISC-70; resizes, Q-MISC-74), or the comparator applies the documented correction. This checks layer heights, z formulas, side faces, split tiles and cut faces without comparing pixels.
3. **Pixels (Hefesto only).** Headless image tests against Hefesto's reference images, tolerance-based, per backend.
4. **OFF behaviors and native features.** Unit tests from the spec scenarios: one per mode for Q-MISC-72, and tests for picking round trips, net highlight sets, cross-section intervals and export bytes.

Golden cases run with magic-compat ON. The fix rows (Q-MISC-70, 71, 73 amendment, 74, 76, 77) have no command-output effect except the Q-MISC-73 errors, which get a deviation entry. Q-MISC-78 is compat: golden cases see Magic's drift (ON); OFF is unit-tested.

### D14. Export and determinism of exported files (EXPORT; J2)

- `export3d <file> [-format gltf|glb|stl] [-merged] [-zscale k] [-visible|-all]`: a Hefesto command (not Magic). The format follows the extension by default.
- Content: the current mesh model's geometry (layers, visibility, cut box) in physical µm (mask layers without a `render` height left out with `VIEW3D-STACK-001`, D5), without the view's z exaggeration unless `-zscale` is given (J2, decided (a) 2026-11-13); k is recorded in `asset.extras.hefesto.zscale`.
- glTF: one mesh node per visible layer in layer order (D6), named by layer; one material per layer named by the display style's long name with its color as base color; positions as `float32` µm relative to the model origin (the region's lower-left), with the origin in the root node's translation as `double`-derived `float`; indices `uint32`; no timestamps; `asset.generator` without build-specific text. Written by cgltf (`cgltf_write`) as the spec requires, under the C locale.
- STL: binary, an 80-byte header of fixed text naming the layer (no time), triangles in mesh order with normals computed from the integer geometry.
- Determinism: the same model writes the same bytes on Linux x86_64, Linux aarch64 and macOS arm64 (`hefesto-testing-strategy` "Cross-platform determinism checks"). Coordinates are computed from integers in `double` and rounded once to `float`; triangulation of faces is fixed (two triangles per rectangle, fixed diagonal). A test writes reference files on each CI leg and compares digests. If cgltf's number formatting turns out to depend on the C library, EXPORT pins it with cgltf's `CGLTF_SPRINTF`-style override hook or a post-pass on the JSON chunk; task 11.4 decides from the CI evidence.

### D15. Mask-layer source (MASKSRC, P2)

Mask-layer mode calls `mask::generate_flat` on the rendered cell over its bbox, or the cut box, grown by the output style's interaction radius (W3Dmain.c:1745), with the `GenerationContext` progress and cancellation, on the build worker. The resulting `LayerPlanes` feed the same mesh builder (D6) with mask-layer order and the `render` attributes (D5). Planes of layers with no `render` statement get h = t = 0 and the pale-highlight style, as Magic, and no physical height (D5). The P1 placeholder (D1) is removed. A change of output style, of `cif ostyle`, or an edit in the region invalidates the planes of the affected chunks; regeneration is per chunk with the halo.

### D16. Overlays (OVERLAY, P3)

Datasets are read, never recomputed: node and coupling capacitance from the `.ext` data of M7 (`extract-file-format`), resistance networks from `extresist-*` output, currents and voltages from M9 simulation results, IR-drop and EM from `reliability-visualization`. Each overlay is a `view3d::overlay::Dataset` (values keyed by net, node or element, with units and range) mapped onto pickable geometry through the net tiles of `connect` and the node positions of the `.ext`/`.res.ext` data. Color maps and legends are the design system's; thresholds filter coupling markers. Only the data binding needs M7; the overlay framework (legend, color map, dataset interface) can be built with synthetic data at the end of P2 if P3 is waiting.

### D17. Performance gates (PERF)

`bench/view3d/` builds models from the reference layouts of `hefesto-performance` and a synthetic 1M-rectangle region, and measures build time, memory and frame time headless (llvmpipe numbers are information only; the gates are on hardware runs recorded in `docs/decisions/` like the Raven checks). `perf.yml` gains a `view3d-bench` job with the build time and memory as gates (deterministic on CI) and frame time as information.

### D18. Changes to earlier code

| Change | Owner | Why |
|---|---|---|
| `view/window_ops.cpp`: the `wind3d` case of `specialopen`; the `UI-CLIENT-002` `wind3d` path retired and its code-table row updated | WIND3D | the client exists, the opening does not |
| `tech/loader.cpp`: the `extract` raw lines handed to `extract::tech` | TECH | D5 |
| `mask::OutputStyle`: `RenderAttrs` per layer, if M5 does not add them | TECH, with M5 STYLES' owner | D5 |
| `view_models.cpp`, `menu_bar_model.cpp`: the 3D tab and `render3d` enabled | SHELL | D12 |
| `src/render/vulkan`: depth image, offscreen target, id image in the render interface | RENDER3D | D8 |

### D19. Exit criteria

- **P1 exit:** tagged tests for every `view-3d` requirement except the mask-layer drawing of "Rendered geometry" (listed as deferred to P2 by `check_requirement_coverage.py`), and for the camera, visibility, picking and net-highlighting parts of "Interactive 3D inspection"; MISC-3D-01..04 pass with no deviation for the mode; the geometry comparison of D13 passes on the layout-layer cases; the 3D tab works on Linux and macOS; PERF meets the gated D6 targets (build time and memory on CI; ≥ 30 fps at 1M rectangles on the M3 Pro), and the other D6 numbers are recorded, a miss being an open item; CI green on every leg.
- **P2 exit:** every `view-3d` requirement covered; mask-layer geometry cases pass; cross-sections; glTF and STL export byte-identical across platforms; the cgltf notice present; PERF re-run with mask-layer mode.
- **P3 exit (the 3D part of the M10 exit):** the common exit criteria of docs/roadmap.md for `view-3d` and `hefesto-3d-visualization`: tagged unit tests for every requirement, CI green, byte-identical exported files across platforms; the overlay scenario "Capacitance heat map" passes on an extracted cell. "Touch orbit" is deferred to M14 (the iPadOS app; D11) and listed as such in the coverage report; the gesture mapping it needs is unit-tested in P1 (task 4.3).

## Area contracts

### TECH: `hefesto/extract/tech/`, `hefesto/view3d/stack/`
- `extract::tech::ExtractStyleSubset` (styles, current style, plane order, h, t per type, `units`, dscale), `parse_extract_subset(RawSection, Technology&) -> ...`
- `view3d::LayerStack`, `build_layout_stack(...)`, `build_mask_stack(const mask::OutputStyle&, bool magic_compat)`, `draw_height`, `physical_height_um`

### MESH: `hefesto/view3d/mesh/`
- `MeshModel`, `Chunk`, `RectInstance`, `SideInstance`, `Triangle`
- `build_from_layout(const LayerStack&, const CellDef&, ClipArea, BuildContext&) -> MeshModel`
- `build_from_planes(const LayerStack&, const mask::LayerPlanes&, ClipArea, BuildContext&) -> MeshModel` (P2)
- `rebuild_chunks(const MeshModel&, Region dirty, ...)`

### CAMERA: `hefesto/view3d/camera/`
- `ViewState`, `default_view(Rect bbox, Mode, float f)`, `rescale(ViewState&, float f)`, `matrix(const ViewState&, int width, int height, Projection)`, `apply(ViewState&, Gesture)`, `preset(Preset)`

### WIND3D: `hefesto/view3d/client/`
- `view3d::Session`, `WindowState`, the command table and handlers; `open_wind3d(...)`

### RENDER3D: `hefesto/render/mesh3d_renderer.hpp`
- `Mesh3dRenderer` (`upload(const MeshModel&)`, `draw(const ViewState&, const DrawParams&, OffscreenTarget&)`, `read_pick(int x, int y) -> PickId`), no Vulkan types in the interface

### INSPECT: `hefesto/view3d/pick/`, `hefesto/view3d/section/`
- `PickTarget`, `resolve(const MeshModel&, PickId)`, `to_selection(...)`, `from_selection(...)`, `net_highlight(...)`; `CrossSection`, `section(const MeshModel&, Segment)` (P2)

### EXPORT: `hefesto/view3d/export/`
- `write_gltf(const MeshModel&, const LayerStack&, ExportOptions, ByteSink&)`, `write_stl(...)`

## Risks / Trade-offs

- **R1. Raven texture mode on MoltenVK and lavapipe.** Zero-copy image views shared between Hefesto's submission and Raven's frame need correct layout transitions and synchronisation. → Spike first (task 5.1). Fallback: Raven's CPU `Rtexture_update` path for the first slice (slower, correct), then fix forward; the draw-callback path with a depth attachment would need a Raven change and is the last resort.
- **R2. Magic's 3D window in the capture image.** An OpenGL Magic under Xvfb and Mesa adds a second image and may be flaky. → Only the `MISC-3D-*` cases use it; command outputs do not depend on the GL implementation; the image is built once and cached (Open Question 3).
- **R3. The extract subset ahead of M7.** Writing part of M7's reader early risks rework. → It is written in M7's home with M7's tags, as the first slice of `extract-tech-section`; M7's plan takes it as existing code (Open Question 4).
- **R4. Memory and frame rate at 1M rectangles.** → Instancing, chunk culling, shader-side z, measured early on the M3 Pro and gated in `view3d-bench` (D6, D17).
- **R5. Float precision on large chips.** → Chunk-relative `float32` positions with `double` origins (D6).
- **R6. Decisions changing later.** The plan follows the answers approved on 2026-11-13. → The table at the end lists the D-numbers each answer shapes, so a later change of decision names the sections to revise.
- **R7. Determinism of glTF text.** cgltf formats numbers with the C library. → C locale, cross-platform digest test, a pinned formatter if needed (D14).
- **R8. Scope creep from the board.** The mockup shows overlays and simulation data before they exist. → Later-phase controls are shown disabled with their phase (D12), as the M2–M5 screens do.

## Migration Plan

No user data changes. The technology loader reads more of the `extract` section; techs that load today load the same way, and PDK extract sections give no new diagnostics (D5). The 3D tab and Window > 3D Display become enabled. With magic-compat OFF, the mask-layer view uses physical heights (Q-MISC-72), which only changes pixels.

## Open Questions

**Decided 2026-11-13: the user chose the recommended option (a) for all four.** 1 and 3 needed the user's decision; 2 and 4 were settled by review and confirmed by the user. The options are kept for the record.

1. **J1: contacts and vias in the 3D stack** (decision table, judgement item 1). Decision: (a). (a) *Recommended:* tech values by default, spanning as a view option; (b) spanning in the Hefesto view only; (c) tech values only.
2. **J2: units and exaggeration of exported models** (judgement item 2; settled by review: s_z is a view parameter, so "displayed model" means its geometry). Decision: (a). (a) physical µm without exaggeration, optional `-zscale`; (b) export the displayed exaggeration; (c) ask on every export.
3. **Golden capture of the 3D commands.** Decision: (a), with the geometry comparator of D13 (per-(style, z) unions). (a) *Recommended:* a `magic-gl` capture image variant (Mesa llvmpipe + Xvfb) used only by `MISC-3D-*`, plus the GL interception shim for geometry; (b) the `magic-gl` image without the shim (command outputs only); (c) no OpenGL Magic: author the expected outputs from Magic's source and mark the cases hand-derived.
4. **The extract-style subset before M7** (settled by review: (a); the user's "first slice from layout layers" rules out (c), and (b) means two readers). Decision: (a); the M7 entry of `docs/roadmap.md` records it. (a) TECH writes it in P1 in `extract-tech-section`'s home with that capability's tags, and M7 extends it; (b) a private 3D-only reader of `height`/`planeorder`, replaced by M7's (two readers for a while); (c) no layout-layer heights until M7: P1 draws layout layers with the defaults only (h = 0.1 · plane index, t = 0.05).

## Resolved Questions

All 3D open questions were approved as drafted on 2026-11-13: Q-MISC-70, 71, 74, 76 and 77 (fix), Q-MISC-72 and 78 (compat), and the amendment of Q-MISC-73 (fix); Q-MISC-75 is unchanged (platform). `docs/decisions/m10-3d-decision-table.md` holds the table, the evidence corrections and the spec changes; the entries' `Resolution:` lines are in `docs/decisions/open-questions/MISC.md`. The specs were aligned in the commit "Align the 3D specs with the approved decisions", including the `render3d` cut-box correction (the box is copied once), J1 and J2. `check_milestone_readiness.py --milestone M10` lists no entry cited by `view-3d` or `hefesto-3d-visualization`.

## Decisions this design depends on

| Decision (approved 2026-11-13) | Option | Design sections it shapes |
|---|---|---|
| Q-MISC-70 | fix (clip in root coordinates) | D6, D13 |
| Q-MISC-71 | fix (per-mode visibility sets) | D4, D9, D10 |
| Q-MISC-72 | compat (ON raw, OFF µm; no `render` line: no height) | D5, D14, D15 |
| Q-MISC-73 amendment | fix (layout mode without output style; `render` error) | D1, D9, D13 |
| `render3d` cut box (spec correction) | copied once, as Magic | D9 |
| Q-MISC-74 | fix (current size) | D7, D8 |
| Q-MISC-76 | fix (mode per window) | D4, D9 |
| Q-MISC-77 | fix (depth test; multi-style last wins) | D5, D6, D8 |
| Q-MISC-78 | compat (ON current f, OFF applied f) | D4, D7, D9 |
| J1 | (a) tech values, spanning as an option | D10, D12 |
| J2 | (a) physical µm, optional z scale | D14 |
| Open Question 3 | (a) `magic-gl` image and GL shim | D13 |
| Open Question 4 | (a) extract subset in `extract-tech-section`'s home | D5 |
