# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of lvs/net. Owner: LVSREAD (openspec/changes/m9-lvs-simulation/design.md
# D3). Every *_test.cpp here is compiled into hefesto_lvs_net_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(lvs_net DATA_DIR lvs/net)
