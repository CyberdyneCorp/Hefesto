# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the sim area (simulation), included by src/core/CMakeLists.txt.
# Owner: SCAFFOLD (openspec/changes/m9-lvs-simulation/design.md D3, tasks.md 4.1): this file lists the
# shared files and includes one sources.cmake per subdirectory, each owned by
# its workstream:
#   engine/  SIMENG
#   deck/    SIMDECK
#   result/  SIMRES
#   probe/   SIMPROBE
#   cmd/     SIMCMD
#   runset/  SIMADV
#   meas/    SIMMEAS
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/not_implemented.cpp)

foreach(_sim_sub IN ITEMS engine deck result probe cmd runset meas)
  include("${CMAKE_CURRENT_LIST_DIR}/${_sim_sub}/sources.cmake")
endforeach()
