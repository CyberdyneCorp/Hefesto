# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the wiring area, included by src/core/CMakeLists.txt.
# Owner: WIRE (openspec/changes/m2-interactive-editing/design.md D1/D2).
# Add new files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/segment.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tool.cpp
  ${CMAKE_CURRENT_LIST_DIR}/wiring.cpp)
