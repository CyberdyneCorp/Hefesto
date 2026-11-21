# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of schem/sym. Owner: SYMLIB (openspec/changes/m8-schematic/design.md
# D3). Every *_test.cpp here is compiled into hefesto_schem_sym_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(schem_sym DATA_DIR schem/sym)
