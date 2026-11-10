# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Core unit tests. Owner: workstream "Core & CLI" (design.md D2).

hefesto_add_unit_test(hefesto_core_tests
  SOURCES
    version_test.cpp
    cmd_test.cpp
    coordinate_units_test.cpp
    diag_test.cpp
    magic_hash_table_test.cpp
    m2_scaffold_test.cpp
    platform_test.cpp
    script_test.cpp
    settings_test.cpp
    util_test.cpp
  LIBS
    hefesto::core)
# diag_test.cpp checks docs/diagnostics/codes.md against the code table;
# m2_scaffold_test.cpp reads tests/golden/CMDEDIT/CASES.md.
target_compile_definitions(hefesto_core_tests PRIVATE HEFESTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# [cap:hefesto-architecture][req:Platform-independent core]
add_test(NAME "hefesto_core: no UI, GPU, Raven or process headers in src/core"
         COMMAND "${CMAKE_COMMAND}" -DCORE_DIR=${PROJECT_SOURCE_DIR}/src/core
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/check_core_includes.cmake")
set_tests_properties("hefesto_core: no UI, GPU, Raven or process headers in src/core" PROPERTIES
  LABELS "unit")
