# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/node (regions, nodes, labels, node names and unique naming),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTNODE (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/labels.cpp
  ${CMAKE_CURRENT_LIST_DIR}/names.cpp
  ${CMAKE_CURRENT_LIST_DIR}/regions.cpp
  ${CMAKE_CURRENT_LIST_DIR}/unique.cpp)
