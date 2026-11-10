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
  src/command_bar_model.cpp
  src/core_command_sink.cpp
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
  src/shell.cpp
  src/side_panels.cpp
  src/theme_tokens.cpp
  src/view_commands.cpp
  src/view_models.cpp)
add_library(hefesto::shell_desktop ALIAS hefesto_shell_desktop)

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
