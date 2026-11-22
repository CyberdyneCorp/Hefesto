# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop shell tests: the toolkit-independent parts (command bar, canvas
# input mapping, view commands, options, design tokens, the Desktop-Layout
# view models, the canvas overlay geometry, the PDK panel model, bundled
# assets) and a compile check
# that the shell's public headers do not need Raven. No window is opened,
# except by the opt-in smoke tests at the end (HEFESTO_TEST_DESKTOP_WINDOW).
# Owner: SHELL (openspec/changes/m1-core-viewer/design.md D2).

hefesto_add_unit_test(hefesto_shell_tests
  SOURCES
    assets_test.cpp
    canvas_controller_test.cpp
    canvas_overlay_test.cpp
    command_bar_test.cpp
    design_tokens_test.cpp
    drc_link_test.cpp
    drc_models_test.cpp
    editing_models_test.cpp
    host_services_test.cpp
    input_router_test.cpp
    options_test.cpp
    pdk_job_test.cpp
    pdk_panel_model_test.cpp
    public_headers_test.cpp
    theme_tokens_test.cpp
    view_commands_test.cpp
    screen_models_test.cpp
    text_key_filter_test.cpp
    view_models_test.cpp
  LIBS
    hefesto::shell_desktop)
# M4 manager panels, text helper, toolbar, Tcl console and FilePicker host
# (openspec/changes/m4-scripting tasks 10.4, 10.5).
target_sources(hefesto_shell_tests PRIVATE
  console_test.cpp
  file_picker_host_test.cpp
  manager_views_test.cpp
  toolbar_views_test.cpp)
# M4 menu bar, Place device and PCell dialog, script consent and trust
# (openspec/changes/m4-scripting tasks 10.1-10.3, SHELL-A).
target_sources(hefesto_shell_tests PRIVATE
  m4_menu_bar_test.cpp
  m4_pcell_dialog_test.cpp
  m4_script_consent_test.cpp)
# M2 items carried into M4 (openspec/changes/m4-scripting tasks 11.2-11.7, CARRY).
target_sources(hefesto_shell_tests PRIVATE
  input_script_test.cpp
  key_bindings_sheet_test.cpp
  label_tool_test.cpp
  minimap_test.cpp
  panels_layout_test.cpp
  wiring_hud_test.cpp)
# M5 GDS menu items and the host of long GDS/CIF reads and writes
# (openspec/changes/m5-mask-gds-cif tasks 12.1, 12.2, SHELL).
target_sources(hefesto_shell_tests PRIVATE
  m5_gds_menu_test.cpp
  m5_mask_job_test.cpp)
# M6 open paths and the LEF/DEF/OASIS jobs through the M5 hosts
# (openspec/changes/m6-lef-def-oasis tasks 14.1-14.3, SHELL).
target_sources(hefesto_shell_tests PRIVATE
  m6_open_test.cpp)
# M10 3D view: the 3D tab's view models, the layer stack panel and `see`,
# gestures, focus routing and picking (openspec/changes/m10-3d-view tasks
# 9.1, 9.2, 9.6, SHELL).
target_sources(hefesto_shell_tests PRIVATE
  view3d_models_test.cpp)
# assets_test reads the bundled fonts and the shell sources.
target_compile_definitions(hefesto_shell_tests PRIVATE HEFESTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_include_directories(hefesto_shell_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit/common")

# Window smoke tests (capability hefesto-build-and-distribution, "Dependency
# policy": Linux CI runs the desktop shell's tests under X11 and Wayland).
# Each starts the real app through Raven, renders frames and fails (exit 4)
# when Raven picked another window system than the one under test. CI runs
# label desktop-window-x11 under Xvfb and desktop-window-wayland under a
# headless Weston; both need a display, so they are opt-in.
if(HEFESTO_TEST_DESKTOP_WINDOW)
  # Magic's mos display styles for the 3D tab capture below (test input only).
  set(_view3d_magic "${PROJECT_SOURCE_DIR}/build/ref/magic")
  set(_view3d_style_files "${_view3d_magic}/scmos/mos.OpenGL.std.cmap" "${_view3d_magic}/scmos/mos.OpenGL.dstyle"
                          "${_view3d_magic}/graphics/color.glyphs")
  set(_view3d_cad_root "${CMAKE_CURRENT_BINARY_DIR}/view3d-cad")
  foreach(_file IN LISTS _view3d_style_files)
    if(NOT EXISTS "${_file}")
      set(_view3d_cad_root "")
    endif()
  endforeach()
  if(_view3d_cad_root)
    file(COPY ${_view3d_style_files} DESTINATION "${_view3d_cad_root}/magic/sys")
  endif()
  foreach(_system x11 wayland)
    add_test(NAME "hefesto: window smoke test (${_system})"
             COMMAND hefesto --smoke-frames 30 --rects 10000 --no-vsync --expect-window-system ${_system})
    set_tests_properties("hefesto: window smoke test (${_system})" PROPERTIES
      LABELS "desktop-window;desktop-window-${_system}"
      ENVIRONMENT "RAVEN_WINDOW_SYSTEM=${_system}"
      TIMEOUT 120)
    # A trackpad pinch injected through Raven reaches the canvas and zooms it
    # (hefesto-input-and-touch "Desktop canvas gestures").
    add_test(NAME "hefesto: trackpad pinch zooms the canvas (${_system})"
             COMMAND hefesto --rects 1000 --pinch 700,450,2 --capture "${CMAKE_CURRENT_BINARY_DIR}/pinch-${_system}.ppm"
                     --capture-frames 6 --no-vsync)
    set_tests_properties("hefesto: trackpad pinch zooms the canvas (${_system})" PROPERTIES
      LABELS "desktop-window;desktop-window-${_system}"
      ENVIRONMENT "RAVEN_WINDOW_SYSTEM=${_system}"
      PASS_REGULAR_EXPRESSION "pinch at 700,450 by 2: zoom 100% -> 200%"
      TIMEOUT 120)
    # A panel docked back after floating fills its dock (it kept the floating
    # card's overlay placement and sat inside a dock as wide as the canvas
    # offset; hefesto-app-ux "Panel layout and focus cycling"). The --input
    # script prints each panel's frame when it ends.
    add_test(NAME "hefesto: a panel docked after floating fills its dock (${_system})"
             COMMAND hefesto --rects 1000 --run "panel inspector float 700 150 300 300; panel inspector dock right"
                     --input "wait 100" --capture "${CMAKE_CURRENT_BINARY_DIR}/redock-${_system}.ppm"
                     --capture-frames 6 --no-vsync)
    set_tests_properties("hefesto: a panel docked after floating fills its dock (${_system})" PROPERTIES
      LABELS "desktop-window;desktop-window-${_system}"
      ENVIRONMENT "RAVEN_WINDOW_SYSTEM=${_system}"
      PASS_REGULAR_EXPRESSION "panel inspector 1156 [0-9]+ 284 [0-9][0-9][0-9]"
      TIMEOUT 120)
    # M10 3D window check (m10-3d-view tasks 5.1, 9.1): a mesh built by
    # view3d/mesh, drawn by VulkanMesh3dRenderer on Raven's device, shown in a
    # viewport in texture mode, then resized; the window's pixels and the pick
    # image are checked after each step (exit 1 on a mismatch).
    add_test(NAME "hefesto: 3D offscreen frame shown through viewport texture mode (${_system})"
             COMMAND hefesto-render3d-spike --capture "${CMAKE_CURRENT_BINARY_DIR}/render3d-spike-${_system}"
                     --expect-window-system ${_system})
    set_tests_properties("hefesto: 3D offscreen frame shown through viewport texture mode (${_system})" PROPERTIES
      LABELS "desktop-window;desktop-window-${_system}"
      ENVIRONMENT "RAVEN_WINDOW_SYSTEM=${_system}"
      TIMEOUT 120)
    # M10 3D tab capture (m10-3d-view tasks 9.1, 9.6): the product app opens
    # the 3D tab with render3d on a four-layer cell of view3d_tab.tech; CI
    # uploads view3d-tab-<system>.ppm for the Linux rows of
    # docs/design/m10-3d-ui-conformance.md. The display styles come from the
    # Magic reference tree (scripts/fetch_pdk.py magic-ref), copied into a
    # CAD_ROOT under the build tree; without that tree the test is not added.
    if(_view3d_cad_root)
      add_test(NAME "hefesto: 3D tab opened by render3d (${_system})"
               COMMAND hefesto -T "${PROJECT_SOURCE_DIR}/tests/data/shell/view3d_tab.tech" --width 1280 --height 800
                       --run "load view3d_tab; box 0 0 120 30; paint ndiff; box 50 -20 70 60; paint poly; box 10 10 110 20; paint metal1; box 90 -10 105 50; paint metal2; box -10 -30 130 70; magic::render3d"
                       --capture "${CMAKE_CURRENT_BINARY_DIR}/view3d-tab-${_system}.ppm" --capture-frames 60 --no-vsync)
      set_tests_properties("hefesto: 3D tab opened by render3d (${_system})" PROPERTIES
        LABELS "desktop-window;desktop-window-${_system}"
        ENVIRONMENT "RAVEN_WINDOW_SYSTEM=${_system};CAD_ROOT=${_view3d_cad_root}"
        TIMEOUT 120)
    endif()
  endforeach()
endif()

# No-display path (task 3.3): with no X11 or Wayland display the app must
# report a clear error and exit 3 instead of crashing. Linux only: macOS
# always has a window server. Needs no display, so it runs in every Linux
# desktop build.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME "hefesto: exits 3 without a display"
           COMMAND env -u DISPLAY -u WAYLAND_DISPLAY -u RAVEN_WINDOW_SYSTEM
                   sh -c "\"$0\" --smoke-frames 1; test $? -eq 3" $<TARGET_FILE:hefesto>)
  set_tests_properties("hefesto: exits 3 without a display" PROPERTIES LABELS "unit" TIMEOUT 60)
endif()
