# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the tools area (readspice and the auxiliary tools), included by
# src/core/CMakeLists.txt.
# Owner: TOOLS (openspec/changes/m4-scripting/design.md D1/D2).
# Add new files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/auxiliary.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/readspice.cpp
  ${CMAKE_CURRENT_LIST_DIR}/spice_lines.cpp)
