# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/tcl (`tcleval` values, the xschem Tcl procedures and the script permission model),
# included by src/core/src/schem/sources.cmake.
# Owner: SYMLIB (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/tcl_bridge.cpp)
