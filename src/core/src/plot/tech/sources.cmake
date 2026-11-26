# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of plot/tech (the `plot` technology section reader and plot styles),
# included by src/core/src/plot/sources.cmake.
# Owner: PLOTTECH (openspec/changes/m10-plot-route/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/plot_tech.cpp)
