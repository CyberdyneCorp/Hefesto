# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the TECH area, included by src/core/CMakeLists.txt.
# Owner: TECH (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/compose.cpp
  ${CMAKE_CURRENT_LIST_DIR}/connect.cpp
  ${CMAKE_CURRENT_LIST_DIR}/contacts.cpp
  ${CMAKE_CURRENT_LIST_DIR}/loader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/names.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scale_layout.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sections.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tech_file.cpp
  ${CMAKE_CURRENT_LIST_DIR}/technology.cpp
  ${CMAKE_CURRENT_LIST_DIR}/type_names.cpp)
