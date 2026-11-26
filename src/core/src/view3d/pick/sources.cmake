# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of view3d/pick (pick ids, pick mapping, the 3D/2D link and
# highlight sets), included by src/core/src/view3d/sources.cmake.
# Owner: INSPECT (openspec/changes/m10-3d-view/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/highlight.cpp
  ${CMAKE_CURRENT_LIST_DIR}/link.cpp
  ${CMAKE_CURRENT_LIST_DIR}/net.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pick.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pick_id.cpp)
