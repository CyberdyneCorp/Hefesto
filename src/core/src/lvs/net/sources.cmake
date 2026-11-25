# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/net (the netlist database and device classes),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSREAD (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/device_class.cpp
  ${CMAKE_CURRENT_LIST_DIR}/expression.cpp
  ${CMAKE_CURRENT_LIST_DIR}/names.cpp
  ${CMAKE_CURRENT_LIST_DIR}/netlist_db.cpp
  ${CMAKE_CURRENT_LIST_DIR}/si_value.cpp)
