# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of pcell/framework (the gencell framework in C++: parameters,
# naming, dispatch, dialogs, toolkit menus, netlist seeding), included by
# src/core/src/pcell/sources.cmake.
# Owner: PCELL (openspec/changes/m4-scripting/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/dialog.cpp
  ${CMAKE_CURRENT_LIST_DIR}/gencell.cpp
  ${CMAKE_CURRENT_LIST_DIR}/naming.cpp
  ${CMAKE_CURRENT_LIST_DIR}/netlist.cpp
  ${CMAKE_CURRENT_LIST_DIR}/parameters.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tcl_list.cpp
  ${CMAKE_CURRENT_LIST_DIR}/toolkit.cpp)
