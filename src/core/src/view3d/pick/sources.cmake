# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of view3d/pick (pick mapping and highlight sets),
# included by src/core/src/view3d/sources.cmake.
# Owner: INSPECT (openspec/changes/m10-3d-view/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/pick.cpp)
