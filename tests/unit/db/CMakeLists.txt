# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the db area. Owner: MODEL
# (openspec/changes/m1-core-viewer/design.md D2). Every *_test.cpp here is
# compiled into hefesto_db_tests; extra libraries or definitions go below.

hefesto_add_area_tests(db)

# use_index/: INDEX (openspec/changes/m5-mask-gds-cif/design.md D1/D2), its
# own executable hefesto_db_use_index_tests (data: tests/data/db/use_index).
add_subdirectory(use_index)
