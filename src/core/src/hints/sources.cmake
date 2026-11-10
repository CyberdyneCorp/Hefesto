# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the hints area (mask hints), included by src/core/CMakeLists.txt.
# Owner: HINTS (openspec/changes/m3-drc/design.md D1/D2).
# Add new files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hints.cpp
  ${CMAKE_CURRENT_LIST_DIR}/region.cpp)
