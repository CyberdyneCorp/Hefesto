# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/hier (hierarchy: subtree, interactions, arrays, yank buffers),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTHIER (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/hier_stubs.cpp
  ${CMAKE_CURRENT_LIST_DIR}/adjust.cpp
  ${CMAKE_CURRENT_LIST_DIR}/array.cpp
  ${CMAKE_CURRENT_LIST_DIR}/connect.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flat.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hard.cpp
  ${CMAKE_CURRENT_LIST_DIR}/interact.cpp
  ${CMAKE_CURRENT_LIST_DIR}/naming.cpp
  ${CMAKE_CURRENT_LIST_DIR}/node_table.cpp
  ${CMAKE_CURRENT_LIST_DIR}/substrate_hier.cpp
  ${CMAKE_CURRENT_LIST_DIR}/subtree.cpp
  ${CMAKE_CURRENT_LIST_DIR}/yank.cpp)
target_sources(hefesto_core PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extract_cell.cpp)
