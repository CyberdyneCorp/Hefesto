# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of lvs/report. Owner: LVSREP (openspec/changes/m9-lvs-simulation/design.md
# D3). Every *_test.cpp here is compiled into hefesto_lvs_report_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(lvs_report DATA_DIR lvs/report)

# [cap:lvs-reports][req:Canonical golden comparison]: tools/golden/compare/lvs_canonical.py on
# Hefesto's comp.out and comp.json (tests/data/lvs/report/top.*, kept current by report_test.cpp).
find_package(Python3 3.11 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME "lvs-reports: lvs_canonical on Hefesto output"
           COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/canonical_test.py"
                   "${PROJECT_SOURCE_DIR}/tools/golden" "${PROJECT_SOURCE_DIR}/tests/data/lvs/report")
  set_tests_properties("lvs-reports: lvs_canonical on Hefesto output" PROPERTIES LABELS "unit;lvs")
endif()
