# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/hier (the compare queue, flattening and pin matching),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSHIER (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/flatten.cpp
  ${CMAKE_CURRENT_LIST_DIR}/fragments.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pattern.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/prepare.cpp
  ${CMAKE_CURRENT_LIST_DIR}/queue.cpp
  ${CMAKE_CURRENT_LIST_DIR}/run.cpp)
