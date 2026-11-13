# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the cell-use index (db/use_index*). Owner: INDEX
# (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Every *_test.cpp here
# is compiled into hefesto_db_use_index_tests; extra libraries or definitions
# go below.

hefesto_add_area_tests(db_use_index DATA_DIR db/use_index)
