# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of cif/read. Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_cif_read_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(cif_read DATA_DIR cif/read)
