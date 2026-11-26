# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the route area (the maze router: the technology mzrouter section and styles, the maze engine, `iroute` and the `*iroute`/`*mzroute` diagnostics),
# included by src/core/CMakeLists.txt. Backend-free core code: no GPU, UI or
# Raven header (scripts/check_core_includes.py).
# Owner: SCAFFOLD (openspec/changes/m10-plot-route/design.md D3): this file
# lists the shared files and includes one sources.cmake per subdirectory,
# each owned by its workstream:
#   tech/    MZTECH
#   maze/    MZ
#   iroute/  IROUTE
#   diag/    IROUTE
# Add files to your subdirectory's sources.cmake; never edit this file.
# Floating point: hefesto_core is compiled with -ffp-contract=off (PUBLIC,
# src/core/CMakeLists.txt), which covers these sources (design D20, Q-MISC-54).

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/not_implemented.cpp)

foreach(_route_sub IN ITEMS tech maze iroute diag)
  include("${CMAKE_CURRENT_LIST_DIR}/${_route_sub}/sources.cmake")
endforeach()
