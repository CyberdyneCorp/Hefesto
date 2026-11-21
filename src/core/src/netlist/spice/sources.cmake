# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of netlist/spice (`ext2spice`),
# included by src/core/src/netlist/sources.cmake.
# Owner: SPICE (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/spice_command.cpp)
