# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the view area (layout windows, core view commands), included
# by src/core/CMakeLists.txt.
# Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/command_util.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/display_ops.cpp
  ${CMAKE_CURRENT_LIST_DIR}/view_ops.cpp
  ${CMAKE_CURRENT_LIST_DIR}/window.cpp
  ${CMAKE_CURRENT_LIST_DIR}/window_ops.cpp)
