# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of gds/write. Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_gds_write_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(gds_write DATA_DIR gds/write)
