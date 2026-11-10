# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the ui area, included by src/core/CMakeLists.txt.
# Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D1/D2).
# Add new files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/bindings.cpp
  ${CMAKE_CURRENT_LIST_DIR}/client.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/command_log.cpp
  ${CMAKE_CURRENT_LIST_DIR}/command_util.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/complete.cpp
  ${CMAKE_CURRENT_LIST_DIR}/history.cpp
  ${CMAKE_CURRENT_LIST_DIR}/input.cpp
  ${CMAKE_CURRENT_LIST_DIR}/keysyms.cpp
  ${CMAKE_CURRENT_LIST_DIR}/line.cpp
  ${CMAKE_CURRENT_LIST_DIR}/macro_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/macro_file.cpp
  ${CMAKE_CURRENT_LIST_DIR}/macros.cpp
  ${CMAKE_CURRENT_LIST_DIR}/measure_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/stack_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tool_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tools.cpp)
