# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of schem/net. Owner: NETLIST (openspec/changes/m8-schematic/design.md
# D3). Every *_test.cpp here is compiled into hefesto_schem_net_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(schem_net DATA_DIR schem/net)
# substring_index_test.cpp includes the netlister's internal header
# (src/core/src/schem/net/substring_index.hpp).
target_include_directories(hefesto_schem_net_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/core/src")
