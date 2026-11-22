# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of netlist/sim (`ext2sim`),
# included by src/core/src/netlist/sources.cmake.
# Owner: SIM (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/from_netlist.cpp
  ${CMAKE_CURRENT_LIST_DIR}/run.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sim_command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/writer.cpp)
