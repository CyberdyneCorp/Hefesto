# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the select area, included by src/core/CMakeLists.txt.
# Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D1/D2).
# Add new files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/create.cpp
  ${CMAKE_CURRENT_LIST_DIR}/edit.cpp
  ${CMAKE_CURRENT_LIST_DIR}/select_command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/selection.cpp
  ${CMAKE_CURRENT_LIST_DIR}/short.cpp
  ${CMAKE_CURRENT_LIST_DIR}/what.cpp)
