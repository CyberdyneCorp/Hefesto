# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/read (the LEF reader, abstract views and annotation),
# included by src/core/src/lef/sources.cmake.
# Owner: LEFR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/args.cpp
  ${CMAKE_CURRENT_LIST_DIR}/find_file.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/layer_section.cpp
  ${CMAKE_CURRENT_LIST_DIR}/layers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/macro.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nondefault_rule.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pin.cpp
  ${CMAKE_CURRENT_LIST_DIR}/port_geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session_host.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session_layers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/top_level.cpp
  ${CMAKE_CURRENT_LIST_DIR}/units.cpp
  ${CMAKE_CURRENT_LIST_DIR}/via_section.cpp)
