# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the schem area (xschem-compatible schematic capture), included
# by src/core/CMakeLists.txt.
# Owner: SCAFFOLD (openspec/changes/m8-schematic/design.md D3, tasks.md 5.1):
# this file lists the shared files and includes one sources.cmake per
# subdirectory, each owned by its workstream:
#   file/   FORMAT     sym/ lib/ tcl/   SYMLIB     net/   NETLIST
#   edit/   EDIT       cmd/   CMD       sim/   SIMINT
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/not_implemented.cpp)

foreach(_schem_sub IN ITEMS file sym lib tcl net edit cmd sim)
  include("${CMAKE_CURRENT_LIST_DIR}/${_schem_sub}/sources.cmake")
endforeach()
