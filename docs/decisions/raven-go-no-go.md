# Raven go/no-go (M0)

- **Status:** open. Evaluation in progress, with a provisional recommendation (2026-11-07). The decision is taken at the M0 gate and then logged as a follow-up of P-11 in `docs/decisions/product.md`.
- **Raven under evaluation:** v0.2.2, commit `4a05d6f11c79e5a6eae77d7fe26f702f5fea4331`, the pin in `cmake/Dependencies.cmake`.
- **Criteria:** the five in `docs/roadmap.md` (M0 exit criteria), with the pass conditions of `openspec/changes/m0-foundations/design.md` D9. All five must hold for "go".
- **Scope of a no-go:** only the desktop shell (`src/shell/desktop/`) would be replaced. The core, the CLI and the Vulkan renderer (`src/render/vulkan/`) do not depend on Raven.
- **Evidence:** measurements in `docs/spikes/canvas-spike.md` and the raw JSON in `docs/spikes/results/`. Code: `src/shell/desktop/` (the Raven wrapper is `src/raven_ui.cpp`), `src/render/vulkan/` and `bench/canvas/`.

This record separates three kinds of statement:

- **Measured / verified:** something that was run, with its output.
- **Implemented, not verified:** code that exists but was not exercised in a way that proves the criterion.
- **Pending:** not done.

## Summary

| # | Criterion | Pass condition (D9) | Status |
|---|---|---|---|
| 1 | Canvas frame rate | median ≤ 16.7 ms during the pan, on both platforms, with Raven's UI drawn in the same frame | **Partial.** Met on macOS on an Apple M3 Pro (13.3 ms median). Reference M1 Mac pending. Linux frame rate pending: CI runs on the lavapipe software rasterizer, which says nothing about GPU frame rate. |
| 2 | Viewport embedding | the canvas resizes, receives mouse, scroll and keyboard input, and coexists with docked panels without flicker or extra frame latency | **Partial.** Embedding, docking and resize verified on macOS. On Linux, CI (PR #10) starts the real app and renders 30 frames with 10k rectangles inside the Raven viewport under X11 (Xvfb) and Wayland (headless Weston), with GCC and Clang, through lavapipe; this proves embedding works on both window systems, not frame rate. Input implemented and unit-tested at the mapping level, not exercised in a live window. Flicker and latency not assessed. |
| 3 | Text and theming | command bar and panels render Inter / JetBrains Mono and follow the design-system tokens (light and dark) | **Partial.** Tokens in dark and light verified. Raven's bundled Roboto / Roboto Mono render. Inter / JetBrains Mono loading implemented, not tested: the fonts are not in the repo yet. Raven compiles Roboto into its code, which conflicts with `hefesto-licensing` (fonts as data files); see section 3. |
| 4 | Accessibility hooks | Raven exposes the platform accessibility tree for standard controls on macOS and Linux | **Pending.** Raven documents the support and the shell names its controls. Nothing was verified with VoiceOver or Orca. |
| 5 | API stability risk | every Raven call isolated; symbol count and v0.2.0 → v0.2.2 breaking changes recorded and judged acceptable | **Met.** 56 functions, all in one file. No removals and no signature changes among them since v0.2.0. |

**Provisional recommendation: GO, conditional.** Nothing measured so far argues against Raven:

- The viewport draw callback hosts Hefesto's renderer on Raven's own Vulkan device.
- The frame budget is dominated by the canvas, not by Raven.
- The API surface is small and contained.

The decision cannot be final until criteria 1 (M1 and Linux), 2 (live input), 3 (Inter / JetBrains Mono) and 4 (screen readers) are checked. The checklist at the end lists each check. If a pending check fails, the fallback order is:

1. fix upstream (P-11 mitigation);
2. use texture mode instead of the draw callback for the canvas, which `hefesto_render_vk` already supports through its offscreen path;
3. no-go for the shell only.

## 1. Canvas frame rate

**Measured** on an Apple M3 Pro (18-core GPU), macOS, MoltenVK 1.4.2, Release build. The full method and tables are in `docs/spikes/canvas-spike.md`.

- **Inside Raven** (`hefesto --bench-frames 600`): the real main window at 1440 × 900 (framebuffer 2880 × 1800) with dock, panels, command bar and status bar, and 10 M rectangles in the viewport. Pan at full-chip zoom: frame interval median **13.3 ms**, p95 16.0 ms, p99 16.3 ms. Same with vsync on or requested off. An empty canvas gives 8.3 ms, the 120 Hz display refresh.
- **Headless** (`hefesto-bench-canvas`, renderer only):
  - 1920 × 1080: brute force 42.5 ms median (fails); with the canvas index (tile culling plus level of detail) 4.9 ms pan and 0.7 ms zoom sweep.
  - 2880 × 1800: 9.9 ms pan.
- Raven's own layout plus paint per frame: median 0.007 ms. The frame time is dominated by the canvas's GPU work.

**Pending:**

- The reference M1 Mac (`hefesto-performance` names it). The M1 GPU is much smaller than the M3 Pro's, so the Retina full-chip pan may miss 16.7 ms there. If it does, the fix is in Hefesto's renderer (coverage textures at low zoom; see the spike report), not in Raven.
- Linux, under X11 and under Wayland, on a mid-range discrete GPU. Nothing was run on Linux.
- Whether `RInitDesc::disable_vsync` is honored on macOS. The 8.3 ms empty-canvas interval suggests that it is not, so Raven's own render cost could not be separated from the presentation wait.

## 2. Viewport embedding

**Verified:**

- **Draw callback with a shared device.** The canvas is a `R_WIDGET_VIEWPORT` in draw-callback mode. Hefesto's renderer records into Raven's command buffer and render pass on Raven's render thread. It uses Raven's VkDevice and queue (`Rvulkan_get_*`), and both sides take the queue lock (`Rvulkan_set_queue_lock_callbacks`). Benchmark runs of 600 frames with 10 M rectangles completed without Vulkan or Raven errors.
- **Docking.** The viewport is a docked panel ("Layout") among the Layers, Hierarchy and Inspector panels. Window captures (`hefesto --capture`) in dark and light themes show the canvas clipped to its panel, with the panels, command bar and status bar around it.
- **Resize.** The resize callback reaches the camera: the first layout auto-fits the die to the viewport's pixel size, and the status bar shows the resulting nm/px.
- **Clean shutdown.** It needs one rule: destroy the window (`Rwindow_destroy` waits for the last frame) before freeing anything the draw callback uses. The shell does this. Without it, the render thread used a destroyed mutex. This is now handled and documented in `MainWindow::~MainWindow`.

**Implemented, not verified:**

- **Input.** Mouse, wheel, trackpad and keyboard events arrive through `Rwidget_set_event_callback`. They are translated to toolkit-free `CanvasInput` and mapped to camera moves:
  - middle or right drag pans;
  - the wheel zooms at the cursor;
  - a two-finger trackpad gesture pans;
  - the arrow keys, + / − and F (fit) move the view;
  - a click focuses the viewport so that keys reach it.

  The mapping is unit-tested (`tests/unit/shell/canvas_controller_test.cpp`). Nobody drove the live window with a mouse or keyboard in this session, and there was no input automation.
- **Flicker and latency.** A live resize while the canvas redraws, and the input-to-photon latency, were not assessed.

## 3. Text and theming

**Verified:**

- The forge color tokens of `hefesto-design-system` are mapped onto Raven's theme roles (`Rtheme_apply` plus `Rtheme_set_color` / `Rtheme_set_accent`), for dark and for light. Captures show both.
- The canvas keeps the dark "canvas" color in the light theme, as the scenario "Theme switch" requires. The renderer clears its viewport area to that color.
- Unit tests check the token values, and that both text tokens reach WCAG 4.5:1 on all three surfaces in both themes.
- Raven's bundled fonts render: Roboto in the UI, and Roboto Mono in the command bar and status bar.
- Theme switching at run time is wired to the M0 `theme dark|light` command.

**Implemented, not verified:**

- `hefesto --font-dir DIR` loads `Inter-Regular.ttf` and `JetBrainsMono-Regular.ttf` (`Rfont_create_from_file`), sets Inter as the theme font, and uses JetBrains Mono for the command bar and status bar. The fonts are not in the repository: bundling them is `hefesto-licensing` work, and P-9 lists them under SIL OFL 1.1. This path was therefore not run.
- The type scale (sizes and weights of `hefesto-design-system` "Typography") is not applied yet.

**Conflict with `hefesto-licensing` (open):**

- `hefesto-licensing` "Bundled fonts and PDK data licensing" says fonts SHALL be kept as separate data files and SHALL NOT be compiled into code. Raven v0.2.2 compiles Roboto and Roboto Mono into the library as its default fonts (`THIRD_PARTY_NOTICES.md`), and has no build option to leave them out. Every Hefesto desktop binary therefore contains them, and the shell falls back to them when no `--font-dir` is given.
- M0 accepts this as a recorded deviation (design.md D9): the binaries are not distributed before M2, and the notices already carry the Roboto (Apache-2.0) and Roboto Mono (OFL-1.1) licenses.
- To close it before the first distributed build (M2): bundle Inter and JetBrains Mono as data files and always load them, and build Raven without its embedded fonts. That needs an upstream Raven option (or a pinned patch) that drops the embedded font data. If neither is possible, the spec needs an explicit exception for a toolkit's built-in fallback font, which is a `hefesto-licensing` change, not an implementation choice.

## 4. Accessibility hooks

**What Raven claims** (`include/raven/raven_accessibility.h` at the pin):

- Every window is exposed to the platform screen reader: NSAccessibility on macOS, AT-SPI on Linux (libdbus loaded at run time), UI Automation on Windows.
- Standard widgets report their role, name, value and state, and the focus is followed.
- `Rwidget_set_accessible_name` / `_description` / `_role` cover custom widgets.

The Cocoa backend has NSAccessibility code (`src/platform/cocoa/cocoa_platform.m`), and the Linux backend has an AT-SPI bridge (`src/platform/atspi.c`).

**Implemented:** the shell sets accessible names on the canvas ("Layout canvas"), the command bar ("Command"), the status bar ("Status"), and the Layers, Cell hierarchy and Inspector panels.

**Pending:**

- With VoiceOver (macOS) and with Orca (Linux, X11 and Wayland), check that the panel controls and the command bar are announced and can be operated.
- Check that the canvas has a sensible role. Raven has no canvas role, so it may need `Rwidget_set_accessible_role` and a description.

## 5. API stability risk

**Verified:**

- **Isolation.** Raven's header is included by exactly one translation unit, `src/shell/desktop/src/raven_ui.cpp`. Every other shell file uses the Raven-free wrappers in `raven_ui.hpp`: `Runtime`, `Window`, `Dock`, `Box`, `Label`, `ListPanel`, `TreePanel`, `CommandBar` and `Viewport`. `raven::raven` is a PRIVATE link dependency of `hefesto_shell_desktop`. The unit test `tests/unit/shell/public_headers_test.cpp` compiles every public shell header in a target without Raven's include path, and fails if one pulls Raven in.
- **Symbols used: 57 functions.**
  - Lifecycle: `Rinit_ex`, `Rshutdown`, `Rwait_frame`, `Rquit`, `Rset_frame_callback`, `Rget_version`.
  - Errors: `Rerror_set_callback`, `Rerror_get_last`, `Rerror_to_string`, `Rseverity_to_string`.
  - Vulkan sharing: `Rvulkan_set_queue_lock_callbacks`, `Rvulkan_get_instance`, `Rvulkan_get_physical_device`, `Rvulkan_get_device`, `Rvulkan_get_graphics_queue`, `Rvulkan_get_graphics_queue_family`.
  - Theme and fonts: `Rtheme_apply`, `Rtheme_set_color`, `Rtheme_set_accent`, `Rtheme_set_font`, `Rfont_create_from_file`, `Rfont_is_valid`, `Rfont_get_monospace`.
  - Widgets: `Rwidget_create`, `Rwidget_add_child`, `Rwidget_focus`, `Rwidget_invalidate`, `Rwidget_set_accessible_name`, `Rwidget_set_event_callback`, `Rwidget_set_focusable`, `Rwidget_set_font`, `Rwidget_set_tooltip`, `Rslot_set_fill`, `Rslot_set_padding`, `Rpanel_set_spacing`.
  - Controls: `Rlabel_create`, `Rlabel_set_text`, `Rlist_view_add_item`, `Rtree_view_add_item`, `Rtext_input_set_committed_callback`, `Rtext_input_set_placeholder`, `Rtext_input_set_text`.
  - Shell: `Rdock_area_create`, `Rdock_area_add_panel`.
  - Viewport: `Rviewport_set_draw_callback`, `Rviewport_set_resize_callback`, `Rviewport_set_realtime`.
  - Window: `Rwindow_create`, `Rwindow_destroy`, `Rwindow_set_content`, `Rwindow_is_valid`, `Rwindow_request_redraw`, `Rwindow_get_scale`, `Rwindow_get_frame_stats`, `Rwindow_capture`, `Rwindow_get_native_handle` (window-system check of the smoke test), `Rpixels_free`.

  Structures read: `RInitDesc`, `REvent`, `RViewportDrawInfo`, `RFrameStats`, `RNativeWindow` (only `system`).
- **v0.2.0 → v0.2.2** (2026-10-30 → 2026-11-06, 145 commits), comparing the `RAVEN_API` declarations of `include/` at both tags:
  - No public function was removed or renamed.
  - 56 of the 57 functions used exist in v0.2.0 with the same declaration. `Rwidget_set_font` was added after v0.2.0.
  - `REvent` gained a trailing field, `wheel_precise`, which the shell uses. This is source-compatible. It changes the structure's size, which matters only for a shared Raven library; Hefesto links Raven statically.
  - `RAVEN_API_VERSION` went from "0.1" to "0.2". About 156 `RAVEN_API` declarations were added.
- Raven's README still says that the public API is subject to change before 1.0.

**Judgment: acceptable.** The exact-commit pin, the one-file wrapper and the symbol list above bound the cost of an upgrade to editing `raven_ui.cpp`. Keep this list current, and repeat the diff at each upgrade (P-11: re-evaluate at each milestone gate).

## Other observations

- **Vulkan loading.** Raven loads the Vulkan library at run time (`libvulkan.1.dylib`, then `libMoltenVK.dylib` on macOS; `libvulkan.so.1` on Linux) and links no Vulkan library. Hefesto's renderer links `Vulkan::Vulkan`. On this machine the Vulkan library was Homebrew's MoltenVK, so both sides used the same loaded image. On a machine with both the LunarG loader and a separate MoltenVK, the two sides must resolve to the same library. Not tested.
- **Raven's build warnings.** On macOS, Raven's Cocoa backend emits `NSUserNotification` deprecation warnings. They come from third-party code and are harmless.
- **No Vulkan SDK locally.** The local machine has Homebrew MoltenVK but neither the LunarG SDK nor the `vulkan-headers` / `vulkan-loader` formulae. The builds used Khronos Vulkan-Headers v1.4.321 from a scratch clone (see `docs/spikes/canvas-spike.md`, "How to reproduce").

## Checklist to close the decision

- [ ] Run `hefesto-bench-canvas` and `hefesto --bench-frames 600 --rects 10000000` on an M1 Mac, and commit the JSON (criterion 1).
- [ ] Run the same on Linux x86_64 with a mid-range discrete GPU, under X11 and under Wayland (criterion 1).
- [ ] Drive the live window: drag-pan, wheel and trackpad zoom, keys after a click, a live resize with the canvas animating, and docking or undocking the Layout panel; check for flicker (criterion 2).
- [ ] Bundle Inter and JetBrains Mono (with their licenses) and check them through `--font-dir`, or through the bundled path once it exists (criterion 3).
- [ ] Resolve the embedded-Roboto conflict with `hefesto-licensing` (section 3): a Raven build option or pinned patch without embedded fonts, or a spec exception (criterion 3).
- [ ] VoiceOver on macOS; Orca on Linux under X11 and Wayland (criterion 4).
- [ ] Decide, and log the outcome as a P-11 follow-up in `docs/decisions/product.md`.
