# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of netlist/spice (`ext2spice`),
# included by src/core/src/netlist/sources.cmake.
# Owner: SPICE (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/device_lines.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_params.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flat_writer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hier_run.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hier_writer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/input.cpp
  ${CMAKE_CURRENT_LIST_DIR}/merge.cpp
  ${CMAKE_CURRENT_LIST_DIR}/numbers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/options.cpp
  ${CMAKE_CURRENT_LIST_DIR}/run.cpp
  ${CMAKE_CURRENT_LIST_DIR}/spice_command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_units.cpp
  ${CMAKE_CURRENT_LIST_DIR}/write_params.cpp)
