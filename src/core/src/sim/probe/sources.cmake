# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of sim/probe (the net name map, probes, op annotation, 3D feed and node naming),
# included by src/core/src/sim/sources.cmake.
# Owner: SIMPROBE (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/net_name_map.cpp
  ${CMAKE_CURRENT_LIST_DIR}/overlay_feed.cpp
  ${CMAKE_CURRENT_LIST_DIR}/probe.cpp)
