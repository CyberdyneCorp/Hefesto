# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the hints area (mask hints). Owner: HINTS
# (openspec/changes/m3-drc/design.md D1/D2). Every *_test.cpp here is
# compiled into hefesto_hints_tests; extra libraries or definitions go below.

hefesto_add_area_tests(hints)

# The db and cell fixtures (MtTech, CellIo with the tiny technology).
target_include_directories(hefesto_hints_tests PRIVATE
  "${PROJECT_SOURCE_DIR}/tests/unit/db"
  "${PROJECT_SOURCE_DIR}/tests/unit/cell")
