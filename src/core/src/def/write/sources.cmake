# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of def/write (the DEF writer),
# included by src/core/src/def/sources.cmake.
# Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/args.cpp
  ${CMAKE_CURRENT_LIST_DIR}/blockages.cpp
  ${CMAKE_CURRENT_LIST_DIR}/layers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nets.cpp
  ${CMAKE_CURRENT_LIST_DIR}/route.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sections.cpp
  ${CMAKE_CURRENT_LIST_DIR}/write.cpp)
