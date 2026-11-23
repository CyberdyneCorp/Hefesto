# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of lef/write. Owner: LEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_lef_write_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(lef_write DATA_DIR lef/write)
