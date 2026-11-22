# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the netlist area (the layout netlisters: ext2spice, ext2sim and extcheck),
# included by src/core/CMakeLists.txt. The area builds with -ffp-contract=off
# (set PUBLIC on hefesto_core; openspec/changes/m7-extraction/design.md D4).
# Owner: INTEGRATION (openspec/changes/m7-extraction/design.md D2/D3): this file lists the shared files and
# includes one sources.cmake per subdirectory, each owned by its workstream:
#   spice     SPICE
#   sim       SIM
#   extcheck  SIM
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/first_word.cpp
  ${CMAKE_CURRENT_LIST_DIR}/table_command.cpp)

foreach(_netlist_sub IN ITEMS spice sim extcheck)
  include("${CMAKE_CURRENT_LIST_DIR}/${_netlist_sub}/sources.cmake")
endforeach()
