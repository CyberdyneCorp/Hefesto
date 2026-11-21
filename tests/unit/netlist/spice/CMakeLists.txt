# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of netlist/spice. Owner: SPICE (openspec/changes/m7-extraction/design.md D2/D3).
# Every *_test.cpp here is compiled into hefesto_netlist_spice_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(netlist_spice DATA_DIR netlist/spice)
