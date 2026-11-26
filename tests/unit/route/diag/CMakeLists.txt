# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of route/diag. Owner: IROUTE (openspec/changes/m10-plot-route/design.md
# D3). Every *_test.cpp here is compiled into hefesto_route_diag_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(route_diag DATA_DIR route/diag)
