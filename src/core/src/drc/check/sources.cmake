# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of drc/check (the basic check kernel), included by
# src/core/src/drc/sources.cmake.
# Owner: CHECK (openspec/changes/m3-drc/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/basic_check.cpp
  ${CMAKE_CURRENT_LIST_DIR}/diagonal.cpp
  ${CMAKE_CURRENT_LIST_DIR}/edges.cpp
  ${CMAKE_CURRENT_LIST_DIR}/euclidean.cpp
  ${CMAKE_CURRENT_LIST_DIR}/maxrect.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nonstandard.cpp
  ${CMAKE_CURRENT_LIST_DIR}/standard.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tiles.cpp)
