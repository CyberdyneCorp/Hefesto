# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of schem/sim. Owner: SIMINT (openspec/changes/m8-schematic/design.md
# D3). Every *_test.cpp here is compiled into hefesto_schem_sim_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(schem_sim DATA_DIR schem/sim)
if(TARGET hefesto_schem_sim_tests)
  # The deck tests use DEVLIB's netlist_support.hpp, which includes nlohmann/json.
  target_link_libraries(hefesto_schem_sim_tests PRIVATE nlohmann_json::nlohmann_json)
endif()
