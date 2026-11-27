# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of plot/ps (the PostScript writer and its embedded prologue),
# included by src/core/src/plot/sources.cmake.
# Owner: PS (openspec/changes/m10-plot-route/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/postscript.cpp)
