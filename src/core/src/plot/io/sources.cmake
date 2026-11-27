# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of plot/io (atomic output files and the default extension rule),
# included by src/core/src/plot/sources.cmake.
# Owner: PLOTCMD (openspec/changes/m10-plot-route/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/atomic_file.cpp)
