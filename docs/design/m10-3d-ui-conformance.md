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

Not checked yet. The scaffold (branch `m10-r1-scaffold`) adds no screen: the 3D tab and Window > 3D Display stay disabled as later-milestone items until SHELL enables them (task 9.1), and `specialopen wind3d` still answers `UI-CLIENT-002` until WIND3D lands (task 8.1).

## Board review (DESIGN, task 7.1)

| Item | Board change | Status |
|---|---|---|
| Parity controls: `level`, mask/layout mode toggle | to be drawn | todo |
| J1 option "Span connected layers" (the "Show contacts and vias" area) | to be drawn | todo |
| Perspective toggle | to be drawn | todo |
| P2/P3 parts disabled with their phase | to be drawn | todo |
| "Open in separate window" tab menu item | to be drawn | todo |

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
| Editor-view tab "3D" next to Layout and Schematic | — | todo |
| Window > 3D Display and `render3d` open the tab | — | todo |
| "Open in separate window" tab menu item | — | todo |
| Progress while a mesh builds; the previous model stays visible | — | todo |
| Focus-based command routing (`.magic3d <cmd>` and the command bar) | — | todo |
| "Mask layers arrive in phase 2" notice in mask-layer mode (D1) | — | todo |

## Layer stack panel and camera (D12; task 9.2)

| Element | Implemented as | Status / gap |
|---|---|---|
| Layers bottom to top with µm ranges (`physical_height_um`) | — | todo |
| Show / hide / isolate, kept in sync with `see` (Q-MISC-71) | — | todo |
| Per-layer transparency | — | todo |
| Presets Isometric, Top, Front, Side | — | todo |
| Perspective toggle | — | todo |
| Mouse, trackpad and key gestures (orbit, pan, zoom, reset) | — | todo |
| Cutbox toggle (`cutbox box` / `cutbox none`) | — | todo |
| `level` and the mask/layout mode toggle (`cif`) | — | todo |

## Inspection (D10; tasks 9.3–9.5)

| Element | Implemented as | Status / gap |
|---|---|---|
| Click picks a target; 3D → 2D selection and scroll | — | todo |
| 2D selection highlighted in 3D | — | todo |
| "Isolate selected net" | — | todo |
| "Show contacts and vias" (J1 "Span connected layers") | — | todo |

## Later phases

| Element | Phase | Status / gap |
|---|---|---|
| Cross-section tool and the A–A′ section panel | P2 (task 11.5) | later |
| Export glTF with the file picker | P2 (task 11.5) | later |
| Overlay dropdown and legend | P3 (task 15.1) | later |
| Touch orbit and pinch zoom | M14 (iPadOS) | later |

## Accessibility (task 9.6)

| Element | Implemented as | Status / gap |
|---|---|---|
| Keyboard access to every control | — | todo |
| Screen-reader labels for every control | — | todo |

## Performance (PERF, section 13)

No numbers yet. PERF records the frame rate at 1M rectangles on the M3 Pro (≥ 30 fps, the P1 exit gate) and the other D6 numbers here.
