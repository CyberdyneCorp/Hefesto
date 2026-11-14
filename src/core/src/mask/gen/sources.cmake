# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of mask/gen (flat and hierarchical generation, see, paint, coverage, statistics, the interaction finder),
# included by src/core/src/mask/sources.cmake.
# Owner: GEN (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/arrays.cpp
  ${CMAKE_CURRENT_LIST_DIR}/classify.cpp
  ${CMAKE_CURRENT_LIST_DIR}/context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flatten.cpp
  ${CMAKE_CURRENT_LIST_DIR}/generate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hier.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sliver.cpp
  ${CMAKE_CURRENT_LIST_DIR}/throttle.cpp
  ${CMAKE_CURRENT_LIST_DIR}/views.cpp)
