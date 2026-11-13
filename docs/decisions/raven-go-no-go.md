# Raven go/no-go (M0)

- **Status:** decided (2026-11-10): **GO**, final for M2 (see "Decision (2026-11-10)" below). The provisional recommendation of 2026-11-07 and the evidence gathered since are kept below. Logged as a follow-up of P-11 in `docs/decisions/product.md`.
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
| 3 | Text and theming | command bar and panels render Inter / JetBrains Mono and follow the design-system tokens (light and dark) | **Met (M1).** Inter 4.1 and JetBrains Mono 2.304 are bundled as data files under `assets/fonts/` (OFL-1.1, pins and SHA-256 in `assets/fonts/README.md`, notices in `THIRD_PARTY_NOTICES.md`) and loaded at startup; every text role uses the forge type scale (m1-core-viewer task 6.1, screenshot check in `docs/design/m1-ui-conformance.md`). Tokens in dark and light verified. Still open, but no longer blocking the criterion: Raven compiles Roboto into its code, which conflicts with `hefesto-licensing` (fonts as data files); see section 3. |
| 4 | Accessibility hooks | Raven exposes the platform accessibility tree for standard controls on macOS and Linux | **Pending.** Raven documents the support and the shell names its controls. Nothing was verified with VoiceOver or Orca. |
| 5 | API stability risk | every Raven call isolated; symbol count and v0.2.0 → v0.2.2 breaking changes recorded and judged acceptable | **Met.** 56 functions, all in one file. No removals and no signature changes among them since v0.2.0. |

**Provisional recommendation: GO, conditional.** Nothing measured so far argues against Raven:

- The viewport draw callback hosts Hefesto's renderer on Raven's own Vulkan device.
- The frame budget is dominated by the canvas, not by Raven.
- The API surface is small and contained.

The decision cannot be final until criteria 1 (M1 and Linux), 2 (live input) and 4 (screen readers) are checked; criterion 3 was met in M1 (fonts bundled and loaded). The checklist at the end lists each check. If a pending check fails, the fallback order is:

1. fix upstream (P-11 mitigation);
2. use texture mode instead of the draw callback for the canvas, which `hefesto_render_vk` already supports through its offscreen path;
3. no-go for the shell only.

## Decision (2026-11-10)

**GO.** The user decided on 2026-11-10 that the go/no-go is final for M2, on this evidence:

- **Frame rate (criterion 1):** the Apple M3 Pro measurements (13.3 ms median inside Raven with 10 M rectangles, section 1; the M2 run on the 100 x 100 `dfxtp_1` array, median 9.3 ms, files in `docs/spikes/results/2026-11-10-apple-m3-pro-m2/`).
- **Embedding on Linux (criterion 2):** the Linux lavapipe CI job, which starts the real app and renders inside the Raven viewport under X11 (Xvfb) and Wayland (headless Weston), with GCC and Clang, on every pull request; the macOS live-input checks of M2 (sections "M2 10.1 checks" and "M2 round 3").
- **Text and theming (criterion 3):** met in M1.
- **Accessibility (criterion 4):** the macOS AX tree checks of M2; the custom-widget press gap is an upstream Raven issue (issue text 3 below), not a reason for a no-go.
- **API stability (criterion 5):** met.

These checks move to M4 (Desktop Alpha, `docs/roadmap.md` M4 "Carried from M2") as **confirmation checks** of the decision; a failure there starts the fallback order above rather than reopening M2:

- the frame-rate run on the reference M1 Mac (and on Linux with a mid-range discrete GPU);
- live mouse, keyboard and trackpad input on Linux under X11 and under Wayland;
- Orca on Linux.

An audible VoiceOver run by a person is part of the user's guided review of M2 (m2-interactive-editing task 10.1). The embedded-Roboto licensing item (section 3) stays an open item and does not block the decision; filing the Raven issues upstream, the other open item, was done on 2026-11-12 (see "Filed upstream (2026-11-12)").

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

**Verified in M1 (m1-core-viewer):**

- Inter (Regular, Medium, SemiBold, Bold) and JetBrains Mono (Regular, Medium) are data files in `assets/fonts/` with their OFL-1.1 texts; `hefesto --asset-dir DIR` (default: the source tree's `assets/`) loads them with `Rfont_create_from_file`, Inter as the theme font and JetBrains Mono for coordinates, names, the command bar and the status bar.
- The type scale of `hefesto-design-system` "Typography" is applied per text role (`design_tokens.hpp`, unit-tested role table plus a screenshot check recorded in `docs/design/m1-ui-conformance.md`).

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

- **Isolation.** Raven's header is included by exactly one translation unit, `src/shell/desktop/src/raven_ui.cpp`. Every other shell file uses the Raven-free wrappers declared in `raven_ui.hpp`. `raven::raven` is a PRIVATE link dependency of `hefesto_shell_desktop`. The unit test `tests/unit/shell/public_headers_test.cpp` compiles every public shell header in a target without Raven's include path, and fails if one pulls Raven in.
- **Symbols used: 100 functions** in `raven_ui.cpp` after the M1 Desktop-Layout screen (counted at the M1 integration pass; the M0 list had 57):
  - Lifecycle and errors: `Rinit_ex`, `Rshutdown`, `Rwait_frame`, `Rquit`, `Rset_frame_callback`, `Rget_version`, `Rerror_set_callback`, `Rerror_get_last`, `Rerror_to_string`, `Rseverity_to_string`.
  - Vulkan sharing: `Rvulkan_set_queue_lock_callbacks`, `Rvulkan_get_instance`, `Rvulkan_get_physical_device`, `Rvulkan_get_device`, `Rvulkan_get_graphics_queue`, `Rvulkan_get_graphics_queue_family`.
  - Theme, look and accessibility settings: `Rtheme_set_dark`, `Rtheme_set_color`, `Rtheme_set_accent`, `Rtheme_set_font`, `Rtheme_set_material`, `Rtheme_set_metric`, `Rlook_set`, `Raccessibility_get_system_settings`, `Raccessibility_set_reduce_motion`.
  - Fonts: `Rfont_create_from_file`, `Rfont_is_valid`, `Rfont_get_default`, `Rfont_get_monospace`, `Rfont_get_variant`, `Rfont_measure_text`, `Rfont_set_fallback`.
  - Widgets (22): `Rwidget_create`, `Rwidget_add_child`, `Rwidget_clear_children`, `Rwidget_focus`, `Rwidget_invalidate`, `Rwidget_is_enabled`, `Rwidget_is_focused`, `Rwidget_is_hovered`, `Rwidget_set_accessible_name`, `Rwidget_set_accessible_description`, `Rwidget_set_accessible_role`, `Rwidget_set_cursor`, `Rwidget_set_enabled`, `Rwidget_set_event_callback`, `Rwidget_set_focusable`, `Rwidget_set_font`, `Rwidget_set_hit_testable`, `Rwidget_set_metric`, `Rwidget_set_min_size`, `Rwidget_set_size`, `Rwidget_set_tooltip`, `Rwidget_set_visibility`.
  - Layout containers: `Rslot_set_fill`, `Rslot_set_padding`, `Rslot_set_valign`, `Rpanel_set_spacing`, `Rborder_set_background`, `Rborder_set_corner_radius`, `Rborder_set_outline`, `Rborder_set_padding`, `Rscroll_box_set_orientation`, `Rtab_view_add_tab`, `Rtab_view_set_active`.
  - Controls: `Rlist_view_add_item`, `Rlist_view_clear_items`, `Rlist_view_get_item_count`, `Rlist_view_scroll_to_item`, `Rtext_input_set_changed_callback`, `Rtext_input_set_committed_callback`, `Rtext_input_set_placeholder`, `Rtext_input_set_text`, `Rfile_dialog_open`.
  - Custom drawing (rulers, minimap, status shapes): `Rcustom_set_paint_callback`, `Rdraw_rect`, `Rdraw_rect_outline`, `Rdraw_line`, `Rdraw_text`, `Rdraw_image`, `Rdraw_push_clip_rect`, `Rdraw_pop_clip_rect`, `Rtexture_create_from_svg`, `Rtexture_is_valid`.
  - Viewport: `Rviewport_set_draw_callback`, `Rviewport_set_resize_callback`, `Rviewport_set_realtime`.
  - Window: `Rwindow_create`, `Rwindow_destroy`, `Rwindow_set_content`, `Rwindow_set_title`, `Rwindow_set_min_size`, `Rwindow_set_clear_color`, `Rwindow_is_valid`, `Rwindow_request_redraw`, `Rwindow_get_scale`, `Rwindow_get_frame_stats`, `Rwindow_capture`, `Rwindow_get_native_handle` (window-system check of the smoke test), `Rpixels_free`.

  - Added in M2 (round 2, 114 functions in all): dialogs for `cmd::PromptHost` (`Rdialog_open`, `Rdialog_run`, `Rdialog_close`), real buttons (`Rbutton_create`, `Rbutton_set_style`, `Rbutton_set_clicked_callback`), labels (`Rlabel_create`, `Rlabel_set_text_size`, `Rlabel_set_wrap`), `Rtext_input_get_text` (Tab completion), `Raccessibility_announce`, `Rwidget_set_gesture_callback`, `Rwindow_get_size` and `Rwindow_inject_trackpad_gesture` (gesture tests); `Rwidget_set_cursor` is now also set on the viewport, and `R_WIDGET_SPACER` is used as a widget kind.

  The M0 dock (`Rdock_area_*`) and tree-view calls are no longer used (label calls are used again since M2). The 43 functions added in M1 were not diffed against v0.2.0; they all exist at the v0.2.2 pin, which is what the build uses.

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

## M2 10.1 checks (macOS, 2026-11-10): results, not final

This is the evidence recorded for m2-interactive-editing task 10.1. It does not change the summary table; the decision was made final in task 11.4 (see "Decision (2026-11-10)"). The benchmark JSON files, the step log of the recorded session and the AX tree dump are kept in `docs/spikes/results/2026-11-10-apple-m3-pro-m2/`. The session was recorded before SHELL routed canvas buttons through Magic's macro tables (middle- and right-drag no longer pan; Option-drag does), so it must be repeated on the current build.

**Setup:**

- Apple M3 Pro, macOS 27.0.1, MoltenVK 1.4.2. This is not the reference M1.
- Debug build of `m2-round2` at `24749a7`, with Raven at the pin.
- Input was real OS events: a CGEvent helper posting at the HID event tap, which AppKit delivers as ordinary NSEvents. There was no Raven injection.
- The reactions were observed in compositor window captures (`screencapture -l`) and through the macOS AX API.
- This host already had the permissions to post events and record the screen; nothing extra had to be allowed.

**Criterion 2: live input in a real window, macOS. Partial pass.**

- **Passed:**
  - hover updates the status x/y;
  - right and middle drag pan;
  - the wheel zooms at the cursor;
  - a trackpad two-finger scroll pans (precise path);
  - a trackpad pinch zooms through `magnifyWithEvent`. The pinch was synthesized with private CGS gesture fields, not a physical trackpad;
  - a click focuses the canvas, then ← → ↓, +, − and F work;
  - typing in the command bar plus Return runs the command;
  - a layer row click toggles visibility;
  - the wheel scrolls the layer list;
  - the Console button works;
  - a live corner-drag resize reflows. 20 captures during the drag showed no blank or torn frame. This is a sampling check, not frame-exact.
- **Failed, a Raven bug at the pin:**
  - In the command bar, ↑ (history) appends U+F700, and ← / → in a text field move the caret and also insert U+F702 / U+F703.
  - Cause: `cocoa_platform.m` `keyDown:` passes `[event characters]` to `rvn_cocoa_emit_text`, which does not drop AppKit's function-key private-use range U+F700–U+F8FF.
  - A physical key press produces the same NSEvent, so this is not an artifact of the synthetic events.
  - `keyDown:` also emits no text for repeated keys, so held keys do not auto-repeat in text fields.
  - Proposed upstream fix: skip U+F700–U+F8FF and C0 controls/DEL in `rvn_cocoa_emit_text`.
- **Not done:**
  - docking or undocking: M1 has no dock; dockable panels are task 10.7;
  - input-to-photon latency;
  - Linux under X11 and Wayland, which cannot be run on this Mac.

**Criterion 4: accessibility, macOS AX tree. Partial.**

VoiceOver was not switched on; this check read the NSAccessibility tree that VoiceOver consumes. Orca on Linux cannot be checked here.

- **Present:**
  - the window's role and title;
  - named groups: Tools, Layers and hierarchy, Layout canvas, Inspector and Status;
  - the "Command" `AXTextField`, whose value and focus can be set;
  - per-layer buttons named with their plane and state ("nwell, plane well, visible"), with help text;
  - tool buttons with "pressed" / `AXEnabled=0`;
  - `AXFrame` on every element;
  - `AXFocusedUIElement` following clicks and Tab / Shift-Tab through Open, PDKs, Zoom to fit, Filter layers and the layer rows;
  - the Console / Diagnostics tabs, as `AXRadioButton` with a working `AXPress`;
  - Space activating a focused layer row.
- **Gaps:**
  1. The shell's custom-painted controls do not expose `AXPress`, and `AXPress` on them does nothing. These are the layer rows, tool rail, Open, PDKs, Zoom to fit and the console toggle, so VoiceOver's VO-Space cannot activate them. Raven marks only its built-in buttons, checkboxes, menu items and list/tree items as invokable, and has no API for a custom widget's invoke action or checked state. The fix is upstream, or SHELL switches to `R_WIDGET_BUTTON` / checkbox.
  2. Toggle state exists only as name text ("pressed", "visible/hidden"), not as an AX value.
  3. Activating or clicking a layer row moves the focus to the window root (the shell rebuilds the list).
  4. Minor:
     - one unnamed static text;
     - the canvas exposed as two nested groups that are both named "Layout canvas";
     - no announcement of command results (`Raccessibility_announce` is unused).

**Criterion 1: 100 × 100 `sky130_fd_sc_hd__dfxtp_1` array.**

The run used `--run "expand all" --bench-frames 300` at 1440 × 900 on the Retina 2× framebuffer: 13,692,241 instances. The machine was loaded (load average about 11) by parallel builds.

| Build | vsync | Median | p95 | p99 | Max |
|---|---|---|---|---|---|
| Debug | on | **9.30 ms** | 17.4 ms | 21.6 ms | 132 ms |
| Debug | off | 3.53 ms | 10.0 ms | 13.9 ms | 78 ms |
| Release, pre-M2 binary | on | 8.41 ms | 16.6 ms | 24.1 ms | 98 ms |

- The median target is met on the M3 Pro.
- The tails need a rerun on an idle machine.
- `disable_vsync` now appears to be honored on macOS.
- The reference M1 Mac and Linux are still pending.

**Left for 10.1:**

- Linux live input under X11 and Wayland;
- Orca;
- VoiceOver driven audibly on the layer palette and the command bar;
- the upstream reports for the key-text bug and the custom-widget accessibility gap;
- dock/undock after 10.7.

## M2 round 3 (SHELL, 2026-11-10): rerun on the current build and Raven issues

**Rerun with real OS events on the current build** (Debug `build/macos-bench` of `m2-round3`, Apple M3 Pro, the same CGEvent helper with a new `keyhold` verb that posts auto-repeat key downs as AppKit does; window captures and AX focus per step under the session scratchpad `m3-shell/live/`):

- Command bar: `echo abc`, ← ←, `X`, then Home, End, Page Up, Page Down and F1 ran `echo aXbc`: the caret moved and no U+F7xx character was inserted.
- A held `k` (one press, eight auto-repeats) typed nine `k`s.
- ^R in the command bar opened the history popover; Tab on `paint m` opened the completion popover (119 sky130A matches, the first 12 listed); a right click on Undo opened the undo history; a right click on a layer row opened its menu; Escape closed each.
- F6 moved the focus canvas → Console → Command → Box tool → Filter layers → canvas (the Inspector is skipped when it has no action), Shift-F6 back.

Both text-field bugs are fixed on Hefesto's side without patching Raven (the repository has no patch mechanism for pinned dependencies, and design.md R1 says to propose fixes upstream): `Window::install_key_hook` (Raven's public `Rwindow_set_event_hook`) runs every event through `shell::TextKeyFilter`, which drops text in U+F700-U+F8FF and, on Cocoa only, retypes a held key's text with `Rwindow_inject_text` on each repeat while a text field has the focus. Regression tests: `tests/unit/shell/text_key_filter_test.cpp`. The workaround can go once Raven fixes the backend.

**Issues to file upstream** (texts ready to paste into https://github.com/oscar-soirey/Raven/issues; not filed from here):

1. *Cocoa: function keys insert private-use characters in text inputs.* `src/platform/cocoa/cocoa_platform.m`, `keyDown:` passes `[event characters]` to `rvn_cocoa_emit_text`, which emits every UTF-16 unit as `RVN_PEVENT_TEXT`. For the arrow keys, Home, End, Page Up/Down, Delete-forward and F1-F35, AppKit's characters are in the function-key range U+F700-U+F8FF (`NSUpArrowFunctionKey` ...), so a `TextInput` moves its caret *and* inserts a glyph-less character (e.g. ← inserts U+F702). Expected: no text event for those keys. Proposed fix: skip U+F700-U+F8FF in `rvn_cocoa_emit_text` (C0 controls and DEL are already dropped by `rvn_textedit_input`). Repro: any `TextInput`, type `abc`, press ←: the text becomes `abc` + U+F702. Found at v0.2.2 (4a05d6f).
2. *Cocoa: held keys do not auto-repeat text.* The same `keyDown:` emits text only `if (!e.repeat)`, so holding a letter in a `TextInput` types it once (X11 and Wayland repeat it). Expected: each auto-repeat inserts the text again, as `NSTextView` does. Proposed fix: emit the text for repeats too (the key event already carries `repeat`). Repro: hold `k` in a `TextInput`: one `k`.
3. *Accessibility: custom widgets cannot be pressed and have no checked state.* `R_WIDGET_CUSTOM` widgets that handle clicks are exposed to NSAccessibility without `AXPress`, and there is no API for a custom widget's invoke action or toggle state; only built-in buttons, checkboxes, menu items and list/tree items are invokable. VoiceOver's VO-Space therefore cannot activate an application's painted controls (Hefesto's tool rail, layer rows, top-bar buttons). Request: `Rwidget_set_accessible_action(widget, callback)` (or treat a focusable custom widget that consumes Enter/Space as pressable) and `Rwidget_set_accessible_checked(widget, state)`.
4. *An anchored popover widget.* Applications need a popover anchored to a widget (completion list under a text input, history list, a "label" editor on a canvas) that keeps the anchor's focus and can host arbitrary content (a filter field plus a list). Today only menus (`Rmenu_open`) and dialogs can float. Hefesto uses context menus for its popovers, which works for keyboard and screen readers but cannot show a filter field or two-column rows.
5. *Documentation: `Rslot_set_fill(child, 0)` makes a zero-wide slot.* A coefficient of 0 keeps the slot a fill slot with no share, so a fixed-width child disappears; `Rslot_set_auto` is the way back. Worth one line in `raven_widget.h`.

Correction to the M2 round-2 notes: `RListView` does have row activation (`Rlist_view_set_activated_callback`, double click or Enter) and selection callbacks; the Feedback and Keys tabs now use the selection callback, so that gap is closed.

**Docking:** Raven 0.2.2 has a dock area (`Rdock_area_*`), but the shell keeps its own fixed Desktop-Layout boxes; `panel layers|inspector dock left|right` moves a side panel to either side of the canvas and `panel reset` restores the M1 arrangement. Dragging a panel out to float (board M2-Panels) needs the shell to move onto `Rdock_area`; not done in M2.

## M4 confirmation checks (CARRY, 2026-11-11)

The checks the decision of 2026-11-10 moved to M4 (`openspec/changes/m4-scripting/tasks.md` 11.1). Only what this Mac can run was run; the rest is manual and stays open.

**Setup:** Apple M3 Pro (not the reference M1), macOS 27.0.1 (26A434), MoltenVK 1.4.2, Raven at the pin (v0.2.2, 4a05d6f). Debug build of `m4-round2` (`build/m4r2-carry`, macos-debug, `-Werror`, desktop on). The machine was heavily loaded by other builds (load average 120-126 during the runs), so the tails are pessimistic. The JSON files are kept in the session scratchpad, not committed; the numbers are below.

**Criterion 1, frame rate inside Raven** (`hefesto --rects 10000000 --bench-frames 600 --width 1440 --height 900`, Retina 2x framebuffer, 19,485,051 canvas instances, pan at full-chip zoom):

| vsync | Median | p95 | p99 | Max | Raven layout + paint (median) |
|---|---|---|---|---|---|
| on | **8.54 ms** | 10.2 ms | 13.6 ms | 151 ms | — |
| off | **8.49 ms** | 10.1 ms | 14.8 ms | 219 ms | 0.20 ms |

The median target (16.7 ms) still holds on the M3 Pro in a Debug build under load, with the highlight shader of this round (lines widened per layer, `rect.vert`). The maxima are scheduling stalls of the loaded machine. Not run: the reference M1 Mac (no M1 here) and Linux with a discrete GPU.

**Criterion 2, live input:** not rerun. Driving the window means posting system-wide CGEvents into the user's live session, which this unattended run does not do; the macOS results of "M2 10.1 checks" and "M2 round 3" stand. Linux X11 and Wayland live input cannot be run on this Mac.

**Criterion 4, accessibility:** VoiceOver was not switched on (an audible run needs a person). Checked in the Raven source at the pin instead: a node is pressable only for Raven's built-in buttons, menu items, checkboxes and list/tree items (`src/core/accessibility.c` sets `RVN_A11Y_INVOKABLE` by widget class), `accessibilityPerformPress` and `isAccessibilitySelectorAllowed` (`src/platform/cocoa/cocoa_platform.m`) follow that flag, and `raven_accessibility.h` has no call to give a custom widget an action or a checked state. Hefesto's painted controls therefore still cannot be pressed with VO-Space; this is recorded as a gap at M4 exit (`docs/design/m2-ui-conformance.md`, "M4 closing notes") and issue 3 below. Orca cannot be run here.

**Crash-recovery dialog:** captured with the product app (`crash save FILE`, then `hefesto --run "crash recover FILE" --capture`): the prompt dialog reads "Recover from backup file …?" with Cancel and the primary, focused Recover, as the M2-Dialogs board. Noted: a long backup path widens the dialog to the path's length instead of wrapping.

**Still open (manual):** the M1 frame-rate run; Linux x86_64 with a mid-range discrete GPU under X11 and Wayland (frame rate and live mouse, keyboard and trackpad input); Orca; an audible VoiceOver run. Filing the issues below: done on 2026-11-12 (see "Filed upstream (2026-11-12)").

### Raven issues to file (drafts, not filed)

Issues 1-5 of "M2 round 3" above are still current at the pin and ready to paste into https://github.com/oscar-soirey/Raven/issues. Issue 3 is restated with the M4 detail, and three are added (7 on 2026-11-11, user decision on the PCell dialog window; 8 by the SHELL closing round):

3. *(restated)* *Accessibility: custom widgets cannot be pressed and have no checked state.* In `src/core/accessibility.c`, `RVN_A11Y_INVOKABLE` / `RVN_A11Y_CHECKABLE` are set by widget class only (`R_WIDGET_BUTTON`, `R_WIDGET_MENU_ITEM`, `R_WIDGET_CHECKBOX`, list and tree items), so `accessibilityPerformPress` on Cocoa (and `DoAction` over AT-SPI, `src/platform/atspi.c`) is unavailable for an `R_WIDGET_CUSTOM` widget even when its role is set to `R_ROLE_BUTTON` or `R_ROLE_TOGGLE_BUTTON` with `Rwidget_set_accessible_role`. Request: `Rwidget_set_accessible_action(Rid widget, void (*press)(Rid, void*), void* user_data)` that marks the node invokable and calls back on press, and `Rwidget_set_accessible_checked(Rid widget, int checked)` (with `-1` for "not checkable") so toggle buttons report their state as a value rather than in their name. Repro: a custom widget with role `R_ROLE_BUTTON` and a click handler; VoiceOver VO-Space does nothing and Accessibility Inspector lists no `AXPress`.
6. *Dock area: float a panel inside the main window.* `Rdock_area_float_panel` takes a panel to an OS window of its own. Applications whose design keeps floating panels inside the main window (a card above the content that can be dragged back to a dock, with the dock's drop-zone highlight while dragging) cannot get that from `Rdock_area`. Request: an option (`Rdock_area_set_float_mode(area, R_DOCK_FLOAT_IN_WINDOW)`) or `Rdock_area_float_panel_in_window(area, panel, x, y, w, h)` that keeps the floating panel in the dock area's window, clamped to it, with the same drag-to-dock behaviour and `Rdock_area_save_layout` support.
7. *Owned utility windows (a window that stays above its owner only).* Applications need a non-modal tool window (an inspector, a parameter dialog) that stays above its owner window but not above other applications, hides with it when the owner is minimized, and does not get its own Dock/taskbar entry. Raven's only option is `Rwindow_set_topmost`, which keeps the window above every window of every application. Request: `Rwindow_set_owner(Rid window, Rid owner)` (or an `owner` field in the window description) mapping to an `NSWindow` child window or `NSPanel` with `NSWindowStyleMaskUtilityWindow` / `isFloatingPanel` scoped to the app on Cocoa, `XSetTransientForHint` plus `_NET_WM_WINDOW_TYPE_UTILITY` on X11, and `xdg_toplevel_set_parent` on Wayland. Repro: open a second window with `Rwindow_set_topmost(w, 1)` and switch to another application: the window stays above it. Hefesto's PCell dialog uses topmost until this exists (accepted by the user on 2026-11-11; `docs/design/m4-ui-conformance.md`, "Non-modal utility window above the layout window").

8. *(SHELL closing round, 2026-11-11)* *Menu items: shortcut text without a binding.* `Rmenu_item_set_shortcut(item, key, modifiers)` both shows the shortcut next to the item and triggers the item when the keys are pressed in the menu bar's window (`raven_controls.h`). An application whose keys are dispatched by its own tables (Magic-compatible key macros that depend on the current tool, and unmodified keys such as `z` or `v` that must still reach text fields) needs the text only: binding them through the menu bar would steal the keys from the canvas and, with the native macOS bar, from text inputs. Request: `Rmenu_item_set_shortcut_text(Rid item, const char* text)` (display only; on Cocoa an attributed title or `NSMenuItem.keyEquivalent` with `allowsKeyEquivalentWhenHidden`-style suppression of the binding, in the in-window bar a right-aligned secondary label), or a flag on `Rmenu_item_set_shortcut` that shows the keys without binding them. Repro: `Rmenu_item_set_shortcut(item, R_KEY_Z, 0)` on a native macOS menu bar, then type `z` in a `TextInput`: the item fires instead of the text being typed. Hefesto's menu bar keeps the accelerators in its model (`MenuBarItem::accelerator`) and shows none until this exists (`docs/design/m4-ui-conformance.md`, Menu bar).

### CARRY round 3: putting the M2 items on screen (2026-11-11)

No new upstream issue. What the on-screen work found at the pin (4a05d6f):

- *Drag-to-dock and press-and-hold are possible in Raven*, so they are not Raven gaps: a mouse-down reaches the application through the window event hook (`Rwindow_set_event_hook`, `R_EVENT_MOUSE_DOWN` with the widget under the pointer), and an in-application drag through `Rdrag_begin` plus `Rwidget_set_drop_callback` (`R_DROP_ENTER/MOVE/LEAVE/DROPPED` with window coordinates; Escape cancels). Hefesto's wrapper (`raven_ui.hpp`) does not expose them yet; that is shell work (m4-scripting task 11.2 and 11.5 open items), not an upstream request.
- *Floating panels inside the main window* are drawn as cards in the canvas overlay (`ui::Overlay`, placed with `place_top_right`): issue 6 still stands for a dock area that does it natively.
- *The label popover* is a card in the canvas overlay next to the box, and *the key-binding sheet* a card that holds the keyboard while open: issue 4 (anchored popover) still stands; a modal sheet with custom content is covered by the same request (an anchored or window-modal popover that can host any widgets).

### SHELL closing round (2026-11-11)

No upstream issue is needed for drag-to-dock or press-and-hold (as CARRY round 3 found): `Rwidget_set_event_callback` delivers `R_EVENT_MOUSE_DOWN`, `R_EVENT_MOUSE_MOVE` and `R_EVENT_MOUSE_UP` to a custom widget, `Rtimer_create` gives a one-shot UI-thread timer and `Rwindow_is_mouse_button_down` the button state. Press-and-hold on Undo is built on them (`ui::Painted::on_hold`, `shell::PressHold`); the header drag is still shell work (`PanelDrag` ready, task 11.2). Neither was checked with a real pointer here. Added issue 8 (display-only menu shortcut text) above.

### CARRY M4 polish round (2026-11-12)

Checked against the Raven pin (`cmake/Dependencies.cmake`: 4a05d6f, v0.2.2) for what the remaining M2 items need: mouse press, drag and hold, key capture, and an accessible press.

- *Press, drag, hold and keys: Raven has them.* `Rwidget_set_event_callback` (`R_EVENT_MOUSE_DOWN/MOVE/UP`), `Rdrag_begin` with `Rwidget_set_drop_callback`, `Rtimer_create`, and the window event hook (`Rwindow_set_event_hook`, which also sees `R_EVENT_MOUSE_UP` before Raven ends an in-application drag) cover the header drag, press-and-hold on Undo and the key-binding sheet's key capture. Raven's injection calls (`Rwindow_inject_mouse_move/_button`, `Rwindow_inject_key`, `Rwindow_inject_text`) take the same dispatch after the platform layer, so the shell now drives these states in scripted captures (`hefesto --input`, `docs/design/m2-ui-conformance.md` "M4 polish round"); every one was captured on macOS and compared with its M2 board.
- *The M4 final round's "the menu bar covers the top header row" is not a Raven gap:* for the first frames after the viewport is ready, the in-window menu bar still takes its 30 px in the layout (the hit-test rectangles of the rail and headers are 30 px lower than drawn); it settles after two frames. `--input` waits ten frames before its first step, and the Layers and Inspector headers then drag like the others.
- *Drop-zone highlight (new issue 9).* Raven delivers `R_DROP_DROPPED` only to a target that accepted the last `R_DROP_ENTER`/`R_DROP_MOVE`, and it always draws its own highlight over an accepting target (`rvn_window_paint_drop`, `src/widget/window.c`: a shadow, a 16 % accent fill and a 2 px accent border over the whole widget). A dock or canvas-column drop zone therefore lights up as a whole, not as the board's dashed band. Shell workaround (no Raven patch): the panel drop zones refuse every phase (no highlight), keep the pointer from `R_DROP_ENTER/MOVE`, draw the board's band in an overlay above the window content, and drop on the left-button release seen by the window event hook while `Rdrag_is_active`.
- *VoiceOver press (issue 3, restated with both causes).* Besides `RVN_A11Y_INVOKABLE` being set by widget class only, the press itself could not reach a custom widget: `rvn_a11y_act` presses through `rvn_a11y_key`, which calls `widget->cls->event` with a synthetic Enter and skips the widget's event callback (`Rwidget_set_event_callback` runs in `src/widget/events.c`, not in the class handler), and `R_WIDGET_CUSTOM` has no class event handler at all (`rvn_class_custom`, `src/widgets/viewport.c`); `rvn_a11y_act` also returns false for it before getting there. A shell-side workaround, wrapping each painted control in an `R_WIDGET_BUTTON` (whose class handler clicks on Enter), was considered and rejected: the Button paints its own body and hover, squashes its child with its press spring, and adds a second Tab stop. The gap stays recorded (m2-ui-conformance "VoiceOver press on custom-drawn controls").

Issue drafts (ready to paste into https://github.com/oscar-soirey/Raven/issues; not filed, the user files them):

3. *(restated, M4 polish)* *Accessibility: custom widgets cannot be pressed and have no checked state.* (a) `src/core/accessibility.c` sets `RVN_A11Y_INVOKABLE` / `RVN_A11Y_CHECKABLE` from the widget class only (`R_WIDGET_BUTTON`, `R_WIDGET_MENU_ITEM`, `R_WIDGET_CHECKBOX`, combo box, list and tree items), so `accessibilityPerformPress` (Cocoa), `DoAction` (AT-SPI) and `Invoke` (UIA) are unavailable for an `R_WIDGET_CUSTOM` widget even with `Rwidget_set_accessible_role(w, R_ROLE_BUTTON)`; (b) even when invoked, `rvn_a11y_act` reaches a widget through `rvn_a11y_key` → `widget->cls->event(widget, KEY_DOWN Enter)`, which bypasses the callback set with `Rwidget_set_event_callback`, where custom widgets handle input. Request, either: `Rwidget_set_accessible_action(Rid widget, void (*press)(Rid widget, void* user), void* user)` marking the node invokable and calling back on press; or make a node invokable when its role is button, toggle button, link, tab, menu item or list item and it has an event callback, and deliver the press as `R_EVENT_KEY_DOWN`/`UP` Enter through the normal event dispatch (event callback first). Plus `Rwidget_set_accessible_checked(Rid widget, int checked)` (`-1`: not checkable) for toggle buttons. Repro: an `R_WIDGET_CUSTOM` with role `R_ROLE_BUTTON` and an event callback that handles Enter; VoiceOver VO-Space does nothing and Accessibility Inspector lists no `AXPress`.
9. *In-application drag: let a drop target draw its own highlight.* `Rwidget_set_drop_callback` targets that accept `R_DROP_ENTER`/`R_DROP_MOVE` are always highlighted by Raven over their whole rectangle (`rvn_window_paint_drop`), and a target that refuses gets no `R_DROP_DROPPED`. An application that shows where the drop will land inside a large target (a dock band at the edge of a canvas, an insertion line in a list) cannot turn the built-in highlight off without losing the drop. Request: a per-widget flag (`Rwidget_set_drop_highlight(Rid widget, int enabled)`) or a return code from the callback meaning "accepted, no highlight" (for example `R_DROP_ACCEPT_QUIET`), or a drop rectangle the callback can set (`RDropEvent.highlight_rect`). Repro: `Rdrag_begin` from a custom widget, a large `R_WIDGET_BOX` with a drop callback that returns `R_TRUE`: the whole box is tinted while the pointer is over it.

### M4 polish round: issues 10–14 (SCREENS, 2026-11-12)

Drafted by the SCREENS polish pass from the board differences of the M4 captures (`docs/design/m4-ui-conformance.md` "SCREENS polish pass"; first numbered 9–13 there, renumbered after CARRY's issue 9 above). Ready to paste into https://github.com/oscar-soirey/Raven/issues; not filed, the user files them.

10. *MenuBar: a divider, a title tint and a non-item header row.* The board puts the PDK's menus after a divider with an ember title tint and a "Devices 1 · pdk1" source row at the top of the menu. `R_WIDGET_MENU_BAR` draws titles only and `R_WIDGET_MENU` holds items, separators and sub-menus. Request: `Rmenu_bar_add_divider(bar)`, `Rmenu_set_title_color(menu, r, g, b, a)` and `Rmenu_add_header(menu, text)` (a disabled, non-focusable caption row); on the native macOS bar the tint and divider may be ignored.
11. *Table: per-cell text color, Left/Right keys, several selected rows.* Gaps G-1, G-3 and G-4 of `docs/design/m4-ui-conformance.md` in one issue: a cell color callback (or `RTableCell.color`), the key callback (or tree rows) for Left/Right, and `Rtable_set_selection_mode(table, R_SELECT_MULTIPLE)`.
12. *Text input: an outline state.* A field corrected by the application (the PCell dialog's `_check`) needs a warning outline that stays until the next edit; `Rtext_input` has the theme's focus ring only. Request: `Rwidget_set_state_outline(widget, r, g, b, a)` or an `R_STATE_WARNING` style.
13. *Buttons: a selected state in a group.* The text helper's justification grid marks the chosen square (ember fill); Raven buttons have no checked state outside a toggle and a segmented control has one row. Request: `Rbutton_set_checked(button, checked)` (a toggle look without a toggle behavior) or a grid of radio buttons.
14. *Popup menu: a filter field and right-aligned key text.* The Place device popover has a filter field and the ^P key of the last device; `Rmenu_open` shows items only (draft 8 covers key text in the bar).

## Filed upstream (2026-11-12)

The user filed the drafts above in https://github.com/oscar-soirey/Raven/issues on 2026-11-12, after checking each one against Raven HEAD v0.2.3 (`019ab35`). Hefesto still pins v0.2.2 (`4a05d6f`, `cmake/Dependencies.cmake`); the pin was not bumped. The Raven calls named below as available were checked in Raven's headers at the pin (`include/raven/*.h` at `4a05d6f`).

| Draft | Upstream | Note |
|---|---|---|
| 1 Cocoa function keys insert U+F7xx | [#89](https://github.com/oscar-soirey/Raven/issues/89) | Hefesto's `TextKeyFilter` workaround stays until a fixed Raven is pinned |
| 2 Cocoa held key does not repeat | [#90](https://github.com/oscar-soirey/Raven/issues/90) | same workaround |
| 3 accessible press and checked state for custom widgets | [#91](https://github.com/oscar-soirey/Raven/issues/91) | also takes the screen-reader part of draft 13 (a checked state for the justification grid) |
| 4 anchored popover | not filed | fixed at Raven HEAD by PR #81 (`Rpopup_open` / `Rpopup_close` / `Rpopup_is_open`, `raven_dialogs.h`); usable after a future pin bump. The filter-field half of draft 14 is the same |
| 5 `Rslot_set_fill(child, 0)` documentation | not filed | a docs-only note |
| 6 float a dock panel inside the window | [#92](https://github.com/oscar-soirey/Raven/issues/92) | |
| 7 owned utility window | [#93](https://github.com/oscar-soirey/Raven/issues/93) | |
| 8 display-only menu shortcut text | [#94](https://github.com/oscar-soirey/Raven/issues/94) | also takes the key-text part of draft 14 (^P in the Place device popover); repro corrected, see below |
| 9 a drop target draws its own highlight | [#95](https://github.com/oscar-soirey/Raven/issues/95) | |
| 10 menu bar divider, title color, header row | [#96](https://github.com/oscar-soirey/Raven/issues/96) | |
| 11 Table per-cell text color, Left/Right, multi-select | [#97](https://github.com/oscar-soirey/Raven/issues/97) | filed for G-3 (multi-select) and G-4 (per-cell color) only; G-1 was not a Raven gap, see below |
| 12 text input outline state | not filed | possible at the pin, see below |
| 13 button selected in a group | not filed | the look is possible at the pin, see below; the screen-reader checked state is in [#91](https://github.com/oscar-soirey/Raven/issues/91) |
| 14 popup filter field and key text | split | filter field: PR #81 (draft 4); key text: [#94](https://github.com/oscar-soirey/Raven/issues/94) (draft 8) |

**Draft 8, repro corrected.** The draft's repro (`Rmenu_item_set_shortcut(item, R_KEY_Z, 0)` on the native macOS bar, then `z` typed in a `TextInput` fires the item) was wrong: `rvn_textedit_key` consumes printable keys, and the Cocoa native menu gives an unmodified printable key no key equivalent, so typing `z` in a TextInput does not fire the item. The real gaps, as filed: the macOS native bar cannot show `Z`; the in-window bar fires the item when no widget consumed the key (so a binding would take the key from the canvas's own macros); and the shortcut is one key plus modifiers only, not free text.

**Not Raven gaps after all (closed in Hefesto, `docs/design/m4-ui-conformance.md`):**

- *G-1 (Table Left/Right).* `Rwidget_set_event_callback` on a Table gets `R_EVENT_KEY_DOWN` before the table's own handler (`rvn_widget_deliver_scoped` in `src/widget/events.c` runs the event callback, then `widget->cls->event`; the same at the pin). The Cells tab now opens and closes nodes with Left/Right through it (`ui::Table::on_key`, `CellsView::tree_key`). Raven's TreeView also has Left/Right, multi-select and `Rtree_item_set_text_color`, should the managers move to it.
- *Draft 12 (warning outline).* `Rwidget_set_color(input, R_COLOR_BORDER, ...)` and `R_COLOR_FOCUS` (the colors a TextInput draws its outline and focus ring with, `src/widgets/text_input.c`), cleared with `Rwidget_clear_color` on the next edit; a style class (`Rwidget_add_class` with `Rtheme_set_style_color`) would do too. The PCell dialog now outlines a corrected field in the forge `warning` color (#E5B13A) until it is typed into again (`ui::TextInput::set_outline`, `PCellEditState::begin_typing`).
- *Draft 13 (selected button in a group), the look.* `Rbutton_set_style(b, R_BUTTON_PRIMARY)` (or `Rwidget_set_color`); the text helper's justification grid now marks the chosen square in the primary (ember) style (`ui::Button::set_primary`, `TextHelperView::justification_marks`). Only the screen-reader checked state is missing ([#91](https://github.com/oscar-soirey/Raven/issues/91)); until then the button's accessible name ends in ", chosen".

These closures add two Raven calls to `raven_ui.cpp`, `Rwidget_set_color` and `Rwidget_clear_color` (both declared in `raven_widget.h` at the pin); `Rwidget_set_event_callback` is now also set on Tables.

## Checklist to close the decision

- [ ] Run `hefesto-bench-canvas` and `hefesto --bench-frames 600 --rects 10000000` on an M1 Mac, and commit the JSON (criterion 1).
- [ ] Run the same on Linux x86_64 with a mid-range discrete GPU, under X11 and under Wayland (criterion 1).
- [ ] Drive the live window: drag-pan, wheel and trackpad zoom, keys after a click, a live resize with the canvas animating, and docking or undocking the Layout panel; check for flicker (criterion 2). Most of this was done on macOS in the M2 10.1 checks above; the arrow-key text and key-repeat bugs are worked around on Hefesto's side (M2 round 3); still open are floating panels and Linux.
- [x] Bundle Inter and JetBrains Mono (with their licenses) and load them through the bundled path (`--asset-dir`) (criterion 3, M1).
- [ ] Resolve the embedded-Roboto conflict with `hefesto-licensing` (section 3): a Raven build option or pinned patch without embedded fonts, or a spec exception (criterion 3).
- [ ] VoiceOver on macOS; Orca on Linux under X11 and Wayland (criterion 4). The macOS AX tree was checked in the M2 10.1 checks above; it has gaps, and neither a VoiceOver run nor Orca has been done.
- [x] Decide, and log the outcome as a P-11 follow-up in `docs/decisions/product.md`: GO, final for M2 (2026-11-10); the unchecked items above are M4 confirmation checks or open items, see "Decision (2026-11-10)".
