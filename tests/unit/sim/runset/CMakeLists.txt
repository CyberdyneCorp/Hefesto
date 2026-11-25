# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of sim/runset. Owner: SIMADV (openspec/changes/m9-lvs-simulation/design.md
# D3). Every *_test.cpp here is compiled into hefesto_sim_runset_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(sim_runset DATA_DIR sim/runset)
