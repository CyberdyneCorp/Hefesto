# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the route area (openspec/changes/m10-plot-route/design.md D3).
# Owner: SCAFFOLD (this file and the shared tests here). Each subdirectory is
# owned by its workstream and builds its own executable once it has a
# *_test.cpp:
#   tech/    MZTECH   hefesto_route_tech_tests   (data: tests/data/route/tech)
#   maze/    MZ       hefesto_route_maze_tests   (data: tests/data/route/maze)
#   iroute/  IROUTE   hefesto_route_iroute_tests   (data: tests/data/route/iroute)
#   diag/    IROUTE   hefesto_route_diag_tests   (data: tests/data/route/diag)

hefesto_add_area_tests(route)

foreach(_route_sub IN ITEMS tech maze iroute diag)
  add_subdirectory(${_route_sub})
endforeach()
