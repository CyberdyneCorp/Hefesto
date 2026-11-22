# M10 3D UI conformance

How the M10 3D view on the desktop (the editor-view tab "3D", its toolbar, the layer stack panel, the camera presets, inspection and the later-phase controls) matches the forge design system (`openspec/specs/hefesto-design-system/spec.md`, pattern "3D inspection") and its reference board `docs/design/mockups/View3D.dc.html` (`openspec/changes/m10-3d-view/design.md` D12). Same format and rules as [m5-ui-conformance.md](m5-ui-conformance.md) and [m4-ui-conformance.md](m4-ui-conformance.md), which stay the records for their screens.

Every row names one element, how the shell implements it, and any gap with its reason. Where Raven cannot match the board, the shell does the closest faithful thing and the gap is recorded here; a gap that needs a Raven change is also reported as an open item (proposed upstream, never patched locally). If SHELL finds an element missing from the board, DESIGN draws it first (M2–M5 rule) and the row says so. Layer colors come from the display styles, never from theme tokens.

Owner: SHELL (seeded by the M10 scaffold, task 2.4; completed in tasks 9.1, 9.2, 9.6 and, for P2, 11.5). DESIGN records the board review (task 7.1); RENDER3D records the renderer spike (task 5.1); the "Performance" section belongs to PERF (tasks.md section 13). Status values:

- **done**: seen in a capture and matching the board within the noted tolerance.
- **partial**: built, with a gap recorded.
- **later**: present but disabled; the function arrives in the phase named (P2: mask layers, cross-sections, export; P3: overlays; M14: touch).
- **blocked**: built in the shell, but the core function it needs is still a scaffold stub.
- **todo**: not built yet.

Nothing is claimed from code reading alone: a row is **done** only after a capture of the product app (`hefesto ... --capture`) was compared with the board, on macOS and Linux, as in M1–M5.

## How this was checked

Not checked yet. The scaffold (branch `m10-r1-scaffold`) adds no screen: the 3D tab and Window > 3D Display stay disabled as later-milestone items until SHELL enables them (task 9.1), and `specialopen wind3d` opens the 3D window since WIND3D (tasks 8.1-8.4).

Round 3 (SHELL, branch `m10-3d-round3-shell`, 2026-11-22): the 3D tab, the layer stack panel and the controls are built (`src/shell/desktop/src/view3d_view.cpp`, view models in `hefesto/shell/view3d_models.hpp`), every control is covered by the headless view-model tests in `tests/unit/shell/view3d_models_test.cpp`, and the 3D window check `hefesto-render3d-spike` now draws a mesh built by MESH with `VulkanMesh3dRenderer`. Rows below say what was seen in a capture; picking and net highlighting stay **blocked** on INSPECT's `view3d/pick` functions, which are stubs on main.

macOS capture (2026-11-22, Apple M3 Pro, MoltenVK 1.4.2, debug build): the product app with `sky130_fd_sc_hd__dfxtp_1` open, `render3d` run from the command bar, window 1600 × 989 logical: [view3d-macos.png](../images/ui/view3d-macos.png). The 3D tab is active in the editor-view control, the header reads "sky130_fd_sc_hd__dfxtp_1 · 3D / layout layers · level 1 · render3d opened this tab", the mesh is drawn in Magic's default 3D view (875 rectangles, status "Mesh ready · 875 rectangles"), and the layer stack panel lists the layers top first with their µm ranges and the Hide / Solo / 100 % row buttons. The 3D window check `hefesto-render3d-spike` passes on the same machine (the mesh's white top face at the centre, black corners, pick instance 1 at the centre, before and after the resize): [640 × 400](../images/ui/view3d-check-macos-640x400.png), [900 × 300](../images/ui/view3d-check-macos-900x300.png). Linux captures come from the CI desktop legs (open).

## Board review (DESIGN, task 7.1)

Reviewed on 2026-11-21 (M10 round 2, branch `m10-r2-decide`): `docs/design/mockups/View3D.dc.html` was updated where the plan changes it (design D12) and rendered at 1440 × 900 with Chromium's headless shell to check the layout (no overlaps, every control visible). The board and `hefesto-design-system` "3D inspection" (layer stack, isometric view with overlay legend, cross-section, camera presets) agree: all four parts are on the board; the overlay legend and the cross-section are shown disabled with their phase, as the M2–M5 boards show later-milestone controls. The render in `docs/images/ui/view3d.png` (used by the top-level README with a caption about the overlay) still shows the M1 drawing; it is re-rendered together with the README caption (open item for INTEGRATION).

| Item | Board change | Status |
|---|---|---|
| Parity controls: `level`, mask/layout mode toggle | A view toolbar along the bottom of the 3D view: the segmented "Mask layers / Layout layers" (`cif`), a Level stepper (`level up` / `level down`, value shown), the Cut box toggle (`cutbox box` / `cutbox none`, the wrapper's checkbutton, D9) and a mesh status ("Mesh ready · n rectangles"; progress while building, D6). A note states that mask-layer mode draws a placeholder with "Mask layers arrive in phase 2" in P1 while `view`, `scroll` and `zoom` keep Magic's values (D1). | done |
| J1 option "Span connected layers" (the "Show contacts and vias" area) | "Show contacts and vias" became "Span connected layers", off by default, with its help text: off, contacts and vias use their tech height and thickness as in Magic; on, each spans from the lower layer's top to the upper layer's bottom (`hefesto-3d-visualization` "Mesh generation from layout"). It moved to an "Inspect" group with "Isolate selected net" (both P1). | done |
| Perspective toggle | An "Orthographic / Perspective" segmented control above the camera presets (design D7; orthographic is Magic's projection and the default). | done |
| P2/P3 parts disabled with their phase | Cross-section and Export glTF… in the top bar, and the A–A′ section panel, are disabled with a "P2" chip and a tooltip; the Overlay dropdown is disabled with "P3" and the note "after M7 extraction", where the legend will show. The 3D view no longer shows simulation data: the rails are drawn in metal1's display color, and the callout shows a pick ("metal1 · VPWR, picked · selected in 2D", D10) instead of a current-density value. | done |
| "Open in separate window" tab menu item | The 3D tab has a menu caret; the open menu shows "Open in separate window" and "Close 3D view" (`closewindow`). The header subtitle says the tab was opened by `render3d`. | done |
| Layer row controls | The hovered metal1 row shows hide (`see no`), isolate and transparency buttons with labels (D10, task 9.2); the board had none. | done |

## Renderer spike (RENDER3D, task 5.1)

Checked on 2026-11-20 with `hefesto-render3d-spike` (`src/shell/desktop/src/render3d_spike.cpp`): it draws the spike's test mesh (`hefesto/render/mesh3d_spike.hpp`: a nearer red quad drawn first, a farther green quad behind it, a blue quad coplanar with the red one drawn later, and a back-facing quad over everything) into a `render::VulkanOffscreenTarget` on Raven's Vulkan device, shows it in a Raven viewport in texture mode, resizes the window from 640 × 400 to 900 × 300 (logical), and compares five window pixels with the expected colors after each step (exit 1 on a mismatch). The same mesh is read back image by image (color, depth, pick id) by `tests/unit/render/mesh3d_target_test.cpp`.

| Item | Result | Status |
|---|---|---|
| Offscreen depth-tested mesh shown through Raven's viewport texture mode on macOS (MoltenVK) | Apple M3 Pro, Homebrew MoltenVK 1.4.2 (the same `libMoltenVK.dylib` image for Raven and Hefesto), Raven v0.2.3: both steps pass (window 1280 × 800 and 1800 × 600 framebuffer pixels); captures [640 × 400](m10-3d-spike/macos-moltenvk-640x400.png), [900 × 300](m10-3d-spike/macos-moltenvk-900x300.png). The `[cap:view-3d]` readback tests pass on the same device. | done |
| The same on Linux (lavapipe) | Passes in CI run 44869237110 (`m10-3d-round1`, PR #77): the three `[cap:view-3d]` offscreen-target tests pass in `gpu-readback-lavapipe` (llvmpipe), and the ctest "3D offscreen frame shown through viewport texture mode" passes under X11 (Xvfb) and Wayland (headless Weston) on the linux-x64 desktop legs. | done |
| Resize, synchronisation and layout transitions | Resize: the viewport's resize callback calls `OffscreenTarget::resize`, which only records the size; a buffer is recreated at the new size the next time it becomes the back buffer, so the shown image stays valid until the viewport is re-pointed (`Rvulkan_texture_set_image_view`, which returns once Raven no longer samples the old view). Synchronisation: the frame is submitted on Raven's graphics queue under the shell's queue lock (the mutex Raven also takes through `Rvulkan_set_queue_lock_callbacks`) and waited on with a fence (redraw on demand, D8); the render pass's external dependencies order Raven's earlier fragment-shader reads and the readback copies before the clears, and the attachment writes before Raven's sampling and the copies. Layouts: every attachment starts UNDEFINED (each frame clears it); color ends in `SHADER_READ_ONLY_OPTIMAL`, the layout Raven requires; pick and depth end in `TRANSFER_SRC_OPTIMAL` for the readbacks; a color readback goes to `TRANSFER_SRC_OPTIMAL` and back (tests only, never while a viewport shows the target). | done (macOS) |
| Color image format and views (sRGB image, UNORM view) | `R8G8B8A8_SRGB` created with `MUTABLE_FORMAT`, drawn through an `_SRGB` view and shown through an `R8G8B8A8_UNORM` view; linear 0.2158605 is stored as 128 and reaches the window as 128 (both checked). Depth `D32_SFLOAT`, pick id `R32_UINT` (0 = background); all three checked as optimal-tiling formats at creation (VulkanError otherwise). No fallback was needed. | done |

## Tab and window (D12; tasks 9.1, 8.4)

| Element | Implemented as | Status / gap |
|---|---|---|
| Editor-view tab "3D" next to Layout and Schematic | The segmented control's "3D" is live: a click shows the 3D view (a second Raven viewport in texture mode beside the layer stack panel), or runs `magic::render3d` when no window is open; "Layout" shows the canvas again. Enter or Space on the focused control toggles the two. | done (macOS capture); Linux capture open (CI desktop legs) |
| Window > 3D Display and `render3d` open the tab | Options > 3D Display (Magic's wrapper puts it under Options) is enabled; any open of the 3D window (`render3d`, `specialopen wind3d`) shows the tab, `closewindow` returns to Layout. Test "Options > 3D Display is enabled". | done (macOS: opened by `render3d` in the capture); Linux open |
| "Open in separate window" tab menu item | Not built: a Raven window per tab needs a second texture-mode viewport host. | todo (open item, P2) |
| Progress while a mesh builds; the previous model stays visible | One `MeshWorker` per window; status line "Building mesh · n %", then "Mesh ready · n rectangles"; the previous model stays drawn until the new one is done. | partial: "Mesh ready" seen in the macOS capture; the progress state is too short on a cell to capture |
| Focus-based command routing (`.magic3d <cmd>` and the command bar) | While the 3D tab is shown, a typed line whose first word is a `wind3d` command runs as `<path> <line>`; keys in the 3D view run the `wind3d` macro table on the path. Tests "lines typed while the 3D view has the focus go to the window's path", "keys in the 3D view run the wind3d bindings on the window's path". | done (headless) |
| "Mask layers arrive in phase 2" notice in mask-layer mode (D1) | A warning banner at the top of the view in mask-layer mode; the layout mesh is drawn with xy scaled by f through `DrawParams::xy_scale` once RENDER's field is on main (unscaled until then). | partial: the scale waits for RENDER |
| Frame-time readout (task 13.2, viewport part) | The "Frame time" toggle shows the rolling mean of the last 30 renderer draws and its frame rate in the status line; `hefesto-render3d-spike --bench-rects N` draws bench/view3d's synthetic region in a window and prints draw and whole-frame times. | done: `--bench-rects 1000000` on the M3 Pro (debug build, 1280 × 800 target, 1,004,944 rectangles in 19 chunks, 120 orbiting frames): draw 33.40 ms (29.9 fps), whole frame 33.73 ms (29.6 fps). The P1 gate (≥ 30 fps, Release) is PERF's measurement in "Performance" |

## Layer stack panel and camera (D12; task 9.2)

| Element | Implemented as | Status / gap |
|---|---|---|
| Layers bottom to top with µm ranges (`physical_height_um`) | One row per drawn layer, sorted by draw height and listed top first (as the board), swatch in the display style's color, range in µm, blank when the layer has no height source; non-solid, non-stippled styles are left out (N3). | done (headless) |
| Show / hide / isolate, kept in sync with `see` (Q-MISC-71) | Row buttons Hide/Show, Solo and "Show all layers" run `<path> see [no] <layer>`, `see no *` + `see <layer>`, `see *`; the rows are read back from the window's set after every command, so `see` from the console updates the panel. Disabled in mask-layer mode (the mask set applies to P2's layers). Test "panel visibility and see stay in sync both ways". | done (headless) |
| Per-layer transparency | The row's percentage button cycles 100 / 50 / 20 % (`DrawParams::alpha`), outside Magic's state. | done (headless) |
| Presets Isometric, Top, Front, Side | Buttons running `<path> view ax ay az` with CAMERA's preset angles. | done (headless) |
| Perspective toggle | Orthographic / Perspective pair (`DrawParams::projection`), not part of Magic's view values. | done (headless) |
| Mouse, trackpad and key gestures (orbit, pan, zoom, reset) | Drag orbits; Option/Shift/right/middle drag pans; wheel and pinch zoom (Shift scales z); two-finger scroll pans; keys through the `wind3d` bindings (space is `defaults`, Magic's reset). | done (headless) |
| Cutbox toggle (`cutbox box` / `cutbox none`) | A toggle button, pressed while the window has a cut box. | done (headless) |
| `level` and the mask/layout mode toggle (`cif`) | Level − / value / + (`level down` / `level up`) and the Mask layers / Layout layers pair (`cif` when the mode differs). | done (headless) |

## Inspection (D10; tasks 9.3–9.5)

| Element | Implemented as | Status / gap |
|---|---|---|
| Click picks a target; 3D → 2D selection and scroll | A left click (under 3 px of motion) reads the pick image, then `view3d::resolve` and `view3d::to_selection`; the caption "layer · picked · selected in 2D" shows bottom right. Tested with a fake resolver. | blocked: INSPECT's `resolve` / `to_selection` are stubs on main |
| 2D selection highlighted in 3D | `view3d::from_selection` after every command, passed as `DrawParams::highlight`. | blocked: INSPECT stub |
| "Isolate selected net" | Toggle button: `view3d::net_highlight` of the picked target as the highlight. | blocked: INSPECT stub |
| "Show contacts and vias" (J1 "Span connected layers") | "Span connected layers" toggle: a full build with `span_connected_layers` and the technology. | done (headless) |

## Later phases

| Element | Phase | Status / gap |
|---|---|---|
| Cross-section tool and the A–A′ section panel | P2 (task 11.5) | later |
| Export glTF with the file picker | P2 (task 11.5) | later |
| Overlay dropdown and legend | P3 (task 15.1) | later |
| Touch orbit and pinch zoom | M14 (iPadOS) | later |

## Desktop review of P1 (SHELL, task 9.6)

Reviewed on 2026-11-22 against `docs/design/mockups/View3D.dc.html` with the macOS capture above.

| Board part | Seen in the product | Status / gap |
|---|---|---|
| Editor-view tab "3D" with its header (cell, mode, level, what opened the tab) | As on the board; the tab is the active segment and the header is a chip at the top left of the view. | done (macOS) |
| Layer stack, top layer first, swatch, name, µm range, row controls | As on the board; the row buttons are always shown (the board shows them on hover), so they are reachable by Tab and need no hover state. With a 349-layer technology (sky130A) the list is long and scrolls. | done (macOS); difference noted |
| View toolbar along the bottom of the 3D view (mode, level, cut box, mesh status) | The mesh status is at the bottom left of the view as on the board; the mode, level and cut box controls are in the side panel under the layer stack ("Magic 3D" section), below the presets, not in a bottom toolbar. With a long layer list they are reached by scrolling the panel or by Tab. | gap: move them to a bottom toolbar (follow-up) |
| Orthographic / Perspective and the presets | In the side panel's "View" section. | done (headless); not in the capture's visible area |
| Inspect group (Isolate selected net, Span connected layers) | In the side panel's "Inspect" section with Frame time and Close 3D view. | partial: picking blocked on INSPECT |
| Tab menu ("Open in separate window", "Close 3D view") | "Close 3D view" is a side-panel button; "Open in separate window" is not built. | todo (P2) |
| Pick callout ("metal1 · VPWR, picked · selected in 2D") | Caption at the bottom right after a click. | blocked: INSPECT |

## Accessibility (task 9.6)

| Element | Implemented as | Status / gap |
|---|---|---|
| Keyboard access to every control | Every control is a focusable painted button (Tab reaches it; Enter and Space click it), including the row buttons and the editor-view tabs; the 3D view takes the focus on a click or when its tab is shown, and its keys are Magic's `wind3d` bindings. | partial: reviewed in code, view-model tests and the macOS capture; a VoiceOver pass on the running app is pending |
| Screen-reader labels for every control | Names: "3D view", "3D view controls", "Layer stack, top layer first", each row "metal1, 1.38–1.73 µm, shown", "Hide metal1", "Isolate metal1", "Transparency of metal1, 50 percent", "Level down" / "Level up", the presets by name; toggles announce ", on" / ", off" (Perspective, Cut box, Span connected layers, Frame time, Isolate selected net). | partial: as above |

## Performance (PERF, section 13)

PERF records the frame rate at 1M rectangles on the M3 Pro (≥ 30 fps, the P1 exit gate, task 13.2) and the other D6 numbers here.

Mesh build (task 13.1, `bench/view3d/`, layout-layer mode, no cut box), measured 2026-11-21 on the M-series development Mac, Release build:

| Workload | Rectangles | Sides | Median build | Model (CPU) | GPU estimate | Peak RSS | D6 |
|---|---|---|---|---|---|---|---|
| synthetic (87 × 86 array of a 134-rectangle unit, 4 layers) | 1,004,944 | 3,760,676 | 1.12 s (runs 0.94–1.62 s) | 155.5 B/rect | 94.8 B/rect | 357 MiB | build met (≤ 1.5 s); CPU memory missed (64 B/rect); GPU met (≤ 96 B/rect) |
| synthetic, CI (ubuntu-24.04, perf run 44969020740) | 1,004,944 | 3,760,676 | 1.13 s (runs 1.10–1.21 s) | 155.5 B/rect | 94.8 B/rect | 390 MiB | build met; CPU memory missed; GPU met |
| array (100 × 100 sky130 dfxtp_1, 349 layers), CI | 8,827,796 | 30,701,344 | 24.0 s (runs 24.0–25.1 s) | 149.0 B/rect | 89.6 B/rect | 2.9 GiB | about 2.7 µs per rectangle against 1.1 µs for synthetic: the 349 (type, style) layers cost per-layer passes; CPU memory missed |
| block (446 sky130_fd_sc_hd cells, 349 layers), CI | 289,535 | 1,015,220 | 0.167 s (runs 0.153–0.187 s) | 172.9 B/rect | 90.1 B/rect | 171 MiB | CPU memory missed |

Gates (`bench/view3d/gates.json`, checked by `view3d_gate.py` in the `perf.yml` `view3d-bench` job), set from the first CI run (M10 round 2): synthetic median build ≤ 1.75 s and model ≤ 170 B/rect; array ≤ 36 s and ≤ 170 B/rect; block ≤ 0.5 s and ≤ 190 B/rect (block's 172.9 B/rect is above the synthetic gate). The memory gate is looser than D6's 64 B/rect while that target is missed (open item for MESH: about 3.7 side instances per rectangle at 28 bytes each, since `SideInstance` carries its own height and thickness, plus the vectors' growth slack; a 20-byte side and `shrink_to_fit` after the build would bring it to about 95 B/rect). Frame time (task 13.2) is measured by the frame workload of the same benchmark (`bench/view3d/frame.cpp`, `--frames N`, default 30) when the Vulkan renderer is built (`HEFESTO_BUILD_DESKTOP=ON`); the `perf.yml` `view3d-bench` job builds without the desktop and reports it as not measured, so it stays recorded, not gated. llvmpipe frame times would be information only (D17).

Frame rate (task 13.2), measured 2026-11-22 (M10 round 3) on the development Mac: Apple M3 Pro (Mac15,7), MoltenVK, Release build, `hefesto-bench-view3d --workload synthetic --repeat 1 --frames 60`. The synthetic 1M-rectangle mesh (19 chunks) is uploaded to `render::VulkanMesh3dRenderer` on the headless device and whole frames (record, submit and wait for `end_frame`) are timed at 1280 × 800 in the isometric view with the whole region visible, after one warm-up frame per level; three runs, with other workflows building on the machine (load average about 5):

| Level | Median frame | fps (median) | ≥ 30 fps gate (D19) |
|---|---|---|---|
| 0 (single-sampled) | 17.3–17.6 ms | 56.7–57.7 | met |
| 1 (MSAA 4x: the geometry is drawn twice, design "As-landed: Mesh3dRenderer"; the default level) | 33.8–34.6 ms | 28.9–29.6 | missed by about 1 fps |

Upload of every chunk: 0.055–0.116 s; upload plus the first frame: 86–200 ms (D6 "first chunks within 200 ms": met on the warm runs, at the limit on the first). The level-1 miss is an open item: the renderer has no frustum culling and draws from host-visible buffers (no device-local staging), and level ≥ 1 draws the geometry once for pick and depth and once more for the 4x color pass; each of these is a candidate. The M1 (60 fps) and reference-hardware runs are not done yet.
