# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the display area (display styles, fonts, scene builder),
# included by src/core/CMakeLists.txt.
# Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/c_scan.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/font.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scene.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scene_splice.cpp
  ${CMAKE_CURRENT_LIST_DIR}/styles.cpp)
