# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of plot/pnm. Owner: PNM (openspec/changes/m10-plot-route/design.md
# D3). Every *_test.cpp here is compiled into hefesto_plot_pnm_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(plot_pnm DATA_DIR plot/pnm)
