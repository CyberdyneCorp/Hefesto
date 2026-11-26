# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of def/read (the DEF reader),
# included by src/core/src/def/sources.cmake.
# Owner: DEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/components.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nets.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/routes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/top_level.cpp
  ${CMAKE_CURRENT_LIST_DIR}/undo.cpp
  ${CMAKE_CURRENT_LIST_DIR}/vias.cpp)
