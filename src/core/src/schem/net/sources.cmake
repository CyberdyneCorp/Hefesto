# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/net (connectivity, net naming, format substitution, `expr`, post-processing and the netlist writer),
# included by src/core/src/schem/sources.cmake.
# Owner: NETLIST (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/netlist.cpp
  ${CMAKE_CURRENT_LIST_DIR}/expr.cpp
  ${CMAKE_CURRENT_LIST_DIR}/bus.cpp)
