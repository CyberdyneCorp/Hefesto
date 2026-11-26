# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/cap (capacitance and lumped resistance),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTCAP (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/cap_context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/compat_branches.cpp
  ${CMAKE_CURRENT_LIST_DIR}/corners.cpp
  ${CMAKE_CURRENT_LIST_DIR}/coupling.cpp
  ${CMAKE_CURRENT_LIST_DIR}/edges.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lumped.cpp
  ${CMAKE_CURRENT_LIST_DIR}/node_caps.cpp
  ${CMAKE_CURRENT_LIST_DIR}/overlap.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sideoverlap.cpp)
