# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of drc/engine (planes, marking, squares, hierarchy, worker),
# included by src/core/src/drc/sources.cmake.
# Owner: ENGINE (openspec/changes/m3-drc/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/array_overlap.cpp
  ${CMAKE_CURRENT_LIST_DIR}/background.cpp
  ${CMAKE_CURRENT_LIST_DIR}/context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/engine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flatten.cpp
  ${CMAKE_CURRENT_LIST_DIR}/host.cpp
  ${CMAKE_CURRENT_LIST_DIR}/interaction.cpp
  ${CMAKE_CURRENT_LIST_DIR}/marking.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pending.cpp)
