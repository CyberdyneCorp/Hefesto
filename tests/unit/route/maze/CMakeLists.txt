# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of route/maze. Owner: MZ (openspec/changes/m10-plot-route/design.md
# D3). Every *_test.cpp here is compiled into hefesto_route_maze_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(route_maze DATA_DIR route/maze)
