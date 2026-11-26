# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of route/tech (the `mzrouter` technology section reader, styles, DRC derivation and the derived irouter style),
# included by src/core/src/route/sources.cmake.
# Owner: MZTECH (openspec/changes/m10-plot-route/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/derive.cpp
  ${CMAKE_CURRENT_LIST_DIR}/derived_style.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/styles.cpp)
