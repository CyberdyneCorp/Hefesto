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
# Owner: workstream "Desktop shell & canvas spike" (design.md D2).

add_library(hefesto_shell_desktop STATIC
  src/canvas_controller.cpp
  src/command_bar_model.cpp
  src/core_command_sink.cpp
  src/main_window.cpp
  src/options.cpp
  src/raven_ui.cpp
  src/shell.cpp
  src/theme_tokens.cpp
  src/view_commands.cpp)
add_library(hefesto::shell_desktop ALIAS hefesto_shell_desktop)

target_include_directories(hefesto_shell_desktop
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(hefesto_shell_desktop
  PUBLIC
    hefesto::core
    hefesto::render_vk
  PRIVATE
    raven::raven
    nlohmann_json::nlohmann_json)
hefesto_set_warnings(hefesto_shell_desktop)

add_executable(hefesto
  src/main.cpp)
target_link_libraries(hefesto PRIVATE hefesto::shell_desktop)
hefesto_set_warnings(hefesto)
