# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/hier (hierarchy: subtree, interactions, arrays, yank buffers),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTHIER (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/hier_stubs.cpp)
target_sources(hefesto_core PRIVATE ${CMAKE_CURRENT_LIST_DIR}/extract_cell.cpp)
