# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/device (devices and substrate),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTDEV (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/device_dims.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_params.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_property.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_scan.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_terms.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_tiles.cpp
  ${CMAKE_CURRENT_LIST_DIR}/devices.cpp
  ${CMAKE_CURRENT_LIST_DIR}/substrate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/subcell_priority.cpp)
