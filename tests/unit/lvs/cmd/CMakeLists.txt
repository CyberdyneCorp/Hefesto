# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of lvs/cmd. Owner: LVSCMD (openspec/changes/m9-lvs-simulation/design.md
# D3). Every *_test.cpp here is compiled into hefesto_lvs_cmd_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(lvs_cmd DATA_DIR lvs/cmd)

# [cap:lvs-command][req:Headless LVS]: hefesto-cli runs `lvs` on two SPICE
# files (tests/data/lvs/cmd) and writes comp.out and comp.json, which must
# equal the expected files.
if(TARGET hefesto-cli)
  add_test(NAME "hefesto-cli: lvs on two SPICE files writes comp.out and comp.json"
           COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli>
                   "-DDATA=${PROJECT_SOURCE_DIR}/tests/data/lvs/cmd" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_lvs"
                   -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_lvs.cmake")
  set_tests_properties("hefesto-cli: lvs on two SPICE files writes comp.out and comp.json" PROPERTIES
    LABELS "unit;cli;lvs")
endif()
