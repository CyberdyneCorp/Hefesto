# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of sim/cmd. Owner: SIMCMD (openspec/changes/m9-lvs-simulation/design.md
# D3). Every *_test.cpp here is compiled into hefesto_sim_cmd_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(sim_cmd DATA_DIR sim/cmd)
