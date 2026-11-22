# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the netlist area (openspec/changes/m7-extraction/design.md D2/D3).
# Owner: INTEGRATION (this file and the shared tests here). Each
# subdirectory is owned by its workstream and builds its own executable once
# it has a *_test.cpp:
#   spice/    SPICE    hefesto_netlist_spice_tests   (data: tests/data/netlist/spice)
#   sim/      SIM      hefesto_netlist_sim_tests   (data: tests/data/netlist/sim)
#   extcheck/ SIM      hefesto_netlist_extcheck_tests   (data: tests/data/netlist/extcheck)

hefesto_add_area_tests(netlist)

foreach(_netlist_sub IN ITEMS spice sim extcheck)
  add_subdirectory(${_netlist_sub})
endforeach()
