# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/write (the LEF writer),
# included by src/core/src/lef/sources.cmake.
# Owner: LEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/args.cpp
  ${CMAKE_CURRENT_LIST_DIR}/chunk.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/header.cpp
  ${CMAKE_CURRENT_LIST_DIR}/macro.cpp
  ${CMAKE_CURRENT_LIST_DIR}/number.cpp
  ${CMAKE_CURRENT_LIST_DIR}/obstruction.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/properties.cpp
  ${CMAKE_CURRENT_LIST_DIR}/write.cpp)
