# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of route/maze (the maze engine (blockage, estimate, search, heaps, paths, paint conversion)),
# included by src/core/src/route/sources.cmake.
# Owner: MZ (openspec/changes/m10-plot-route/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/route.cpp)
