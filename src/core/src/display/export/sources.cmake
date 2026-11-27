# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of display/export (view export to SVG and PNG: the scene to SVG,
# the CPU scan-converter and the PNG encoder), included by
# src/core/src/display/sources.cmake.
# Owner: EXPORT (openspec/changes/m10-plot-route/design.md D3, D10). Add new
# files here. The -ffp-contract=off flag of design D20 is already PUBLIC on
# hefesto_core (src/core/CMakeLists.txt), so these sources and font.cpp
# compile without contraction.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/export.cpp)
