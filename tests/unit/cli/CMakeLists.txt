# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# CLI tests: unit tests of the CLI logic, plus smoke tests that run the
# hefesto-cli executable for each exit status.
# Owner: workstream "Core & CLI" (design.md D2).

hefesto_add_unit_test(hefesto_cli_tests
  SOURCES
    cli_test.cpp
  LIBS
    hefesto_cli_lib)

# [cap:hefesto-architecture][req:Headless command-line tool]
set(_log_dir "${CMAKE_CURRENT_BINARY_DIR}/session-logs")

function(hefesto_cli_smoke name expected_regex)
  add_test(NAME "hefesto-cli: ${name}" COMMAND hefesto-cli --log-dir=${_log_dir} ${ARGN})
  set_tests_properties("hefesto-cli: ${name}" PROPERTIES
    LABELS "unit;cli"
    PASS_REGULAR_EXPRESSION "${expected_regex}")
endfunction()

add_test(NAME "hefesto-cli: --version exits 0"
         COMMAND hefesto-cli --version)
set_tests_properties("hefesto-cli: --version exits 0" PROPERTIES
  LABELS "unit;cli"
  PASS_REGULAR_EXPRESSION "^hefesto-cli [0-9]+\\.[0-9]+\\.[0-9]+")

hefesto_cli_smoke("commands exit 0" "^hello\n" -c "echo hello; quit")

add_test(NAME "hefesto-cli: usage error exits 2"
         COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli> -DEXPECT=2 -DARGS=--no-such-option
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_exit.cmake")
set_tests_properties("hefesto-cli: usage error exits 2" PROPERTIES LABELS "unit;cli")

add_test(NAME "hefesto-cli: failing command exits 1"
         COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli> -DEXPECT=1 "-DARGS=--no-log;-c;nosuch"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_exit.cmake")
set_tests_properties("hefesto-cli: failing command exits 1" PROPERTIES LABELS "unit;cli")

# Regression: a directory given as a script read as an empty script and exited 0.
add_test(NAME "hefesto-cli: directory as script exits 1"
         COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli> -DEXPECT=1
                 "-DARGS=--no-log;${CMAKE_CURRENT_SOURCE_DIR}"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_exit.cmake")
set_tests_properties("hefesto-cli: directory as script exits 1" PROPERTIES LABELS "unit;cli")
