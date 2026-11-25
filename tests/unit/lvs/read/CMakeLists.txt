# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of lvs/read. Owner: LVSREAD (openspec/changes/m9-lvs-simulation/design.md
# D3). Every *_test.cpp here is compiled into hefesto_lvs_read_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(lvs_read DATA_DIR lvs/read)
