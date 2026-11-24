# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the lvs area (layout versus schematic), included by src/core/CMakeLists.txt.
# Owner: SCAFFOLD (openspec/changes/m9-lvs-simulation/design.md D3, tasks.md 4.1): this file lists the
# shared files and includes one sources.cmake per subdirectory, each owned by
# its workstream:
#   net/     LVSREAD
#   read/    LVSREAD
#   setup/   LVSSETUP
#   cmp/     LVSALG
#   reduce/  LVSRED
#   prop/    LVSPROP
#   hier/    LVSHIER
#   report/  LVSREP
#   cmd/     LVSCMD
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/not_implemented.cpp)

foreach(_lvs_sub IN ITEMS net read setup cmp reduce prop hier report cmd)
  include("${CMAKE_CURRENT_LIST_DIR}/${_lvs_sub}/sources.cmake")
endforeach()
