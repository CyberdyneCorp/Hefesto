# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of view3d/stack (the layer stack: drawable (type, style) pairs, draw and physical heights),
# included by src/core/src/view3d/sources.cmake.
# Owner: TECH (openspec/changes/m10-3d-view/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/stack.cpp)

# The ghost-class classifier (Q-MISC-85; owner SHELL, M10 round 4).
target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/ghost.cpp)
target_sources(hefesto_core PRIVATE ${CMAKE_CURRENT_LIST_DIR}/substrate.cpp)  # the board's substrate slab (SECTION, M10 round 5)
