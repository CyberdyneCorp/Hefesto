# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the app area (settings layers, project file, startup),
# included by src/core/CMakeLists.txt.
# Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/project_file.cpp
  ${CMAKE_CURRENT_LIST_DIR}/settings_layers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/settings_stack.cpp
  ${CMAKE_CURRENT_LIST_DIR}/startup.cpp
  ${CMAKE_CURRENT_LIST_DIR}/startup_files.cpp
  ${CMAKE_CURRENT_LIST_DIR}/startup_options.cpp)
