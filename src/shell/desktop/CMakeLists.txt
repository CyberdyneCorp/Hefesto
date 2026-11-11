# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_shell_desktop: C++20 wrapper over Raven's C API, and the desktop app
# executable "hefesto" (Linux and macOS). This directory is the ONLY place
# where Raven headers or types may appear (capability hefesto-architecture,
# requirement "App shells per platform"; P-11). Inside it, only
# src/raven_ui.cpp includes <Raven.h>. raven::raven is linked PRIVATE so that
# no Raven include path leaks to dependents; tests/unit/shell compiles every
# public header without Raven to keep it that way.
# Owner: SHELL (openspec/changes/m1-core-viewer/design.md D2).

add_library(hefesto_shell_desktop STATIC
  src/assets.cpp
  src/canvas_controller.cpp
  src/canvas_overlay.cpp
  src/color_panel.cpp
  src/command_bar_model.cpp
  src/core_command_sink.cpp
  src/drc_link.cpp
  src/drc_models.cpp
  src/drc_panel.cpp
  src/drc_window.cpp
  src/editing_models.cpp
  src/input_router.cpp
  src/layout_canvas.cpp
  src/main_window.cpp
  src/options.cpp
  src/pdk_job.cpp
  src/pdk_panel.cpp
  src/pdk_panel_model.cpp
  src/raven_ui.cpp
  src/screen_paint.cpp
  src/screen_models.cpp
  src/shell.cpp
  src/side_panels.cpp
  src/text_key_filter.cpp
  src/theme_tokens.cpp
  src/view_commands.cpp
  src/view_models.cpp)
add_library(hefesto::shell_desktop ALIAS hefesto_shell_desktop)

# M4 (openspec/changes/m4-scripting tasks 10.4, 10.5): the manager panels,
# text helper, layer toolbar and reorder dialog, the Tcl console and the
# native FilePicker host.
target_sources(hefesto_shell_desktop PRIVATE
  src/console_panel.cpp
  src/console_session.cpp
  src/file_picker_host.cpp
  src/m4_managers_window.cpp
  src/manager_panels.cpp
  src/manager_views.cpp
  src/tcl_console.cpp
  src/text_helper_view.cpp
  src/toolbar_views.cpp
  src/utility_windows.cpp)

# M4 (openspec/changes/m4-scripting tasks 10.1-10.3, SHELL-A): the menu
# bar, Place device and the PCell dialog, script consent and trust.
target_sources(hefesto_shell_desktop PRIVATE
  src/gui_script.cpp
  src/m4_window.cpp
  src/menu_bar_model.cpp
  src/pcell_dialog.cpp
  src/pcell_dialog_model.cpp
  src/script_consent_model.cpp)

# M4 (openspec/changes/m4-scripting tasks 11.2-11.7, CARRY): panel layout,
# key-binding sheet, label tool, minimap thumbnail, wiring HUD.
target_sources(hefesto_shell_desktop PRIVATE
  src/key_bindings_sheet.cpp
  src/key_bindings_view.cpp
  src/label_tool.cpp
  src/label_tool_view.cpp
  src/minimap.cpp
  src/panels_host.cpp
  src/panels_layout.cpp
  src/wiring_hud.cpp
  src/wiring_hud_view.cpp)

# POSIX host services (fcntl locks, file ownership/ctime, process liveness,
# posix_spawn process runner): hefesto::platform_posix, and the HTTPS Fetcher
# (URLSession or libcurl): hefesto::platform_net, both added by the root
# CMakeLists.txt.
target_include_directories(hefesto_shell_desktop
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(hefesto_shell_desktop
  PUBLIC
    hefesto::core
    hefesto::render_vk
  PRIVATE
    hefesto::platform_posix
    hefesto::platform_net
    raven::raven
    nlohmann_json::nlohmann_json)
hefesto_set_warnings(hefesto_shell_desktop)

# Development fallback for the asset folder (assets/: bundled fonts, icons);
# a build or install places assets/ next to the executable.
target_compile_definitions(hefesto_shell_desktop
  PRIVATE HEFESTO_SOURCE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")

add_executable(hefesto
  src/main.cpp)
target_link_libraries(hefesto PRIVATE hefesto::shell_desktop)
hefesto_set_warnings(hefesto)

# Ship the bundled fonts (OFL-1.1, with their license texts) and icons next
# to the app: <build>/src/shell/desktop/assets/.
add_custom_command(TARGET hefesto POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PROJECT_SOURCE_DIR}/assets/fonts" "$<TARGET_FILE_DIR:hefesto>/assets/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PROJECT_SOURCE_DIR}/assets/icons" "$<TARGET_FILE_DIR:hefesto>/assets/icons"
  VERBATIM)
