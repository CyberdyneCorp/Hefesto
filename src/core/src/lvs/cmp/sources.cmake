# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/cmp (the comparison graph, partition, refinement and verify),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSALG (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/compare.cpp
  ${CMAKE_CURRENT_LIST_DIR}/graph.cpp
  ${CMAKE_CURRENT_LIST_DIR}/refine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/resolve.cpp
  ${CMAKE_CURRENT_LIST_DIR}/verify.cpp)
