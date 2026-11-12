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
  key_bindings_sheet_test.cpp
  label_tool_test.cpp
  minimap_test.cpp
  panels_layout_test.cpp
  wiring_hud_test.cpp)
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
