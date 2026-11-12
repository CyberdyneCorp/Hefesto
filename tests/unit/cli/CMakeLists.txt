# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# CLI tests: unit tests of the CLI logic, plus smoke tests that run the
# hefesto-cli executable for each exit status.
# Owner: workstream "Core & CLI" (M0 design.md D2); the M1 startup options
# and tests belong to PDK (m1-core-viewer design.md D2).

hefesto_add_unit_test(hefesto_cli_tests
  SOURCES
    cli_test.cpp
    interactive_test.cpp
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

# [cap:app-startup][req:Command-line options]: a bad Magic option exits 1.
add_test(NAME "hefesto-cli: bad Magic option exits 1"
         COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli> -DEXPECT=1 "-DARGS=--no-log;--magic;-x"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_exit.cmake")
set_tests_properties("hefesto-cli: bad Magic option exits 1" PROPERTIES LABELS "unit;cli")

# [cap:app-startup][req:Batch and headless operation]: Magic's scripted run,
# commands from standard input until quit, with the status quit gives.
add_test(NAME "hefesto-cli: batch commands from standard input exit 0"
         COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli> -DEXPECT=0
                 "-DARGS=--no-log;--magic;-dnull;-noconsole"
                 "-DSTDIN=box 0 0 10 10\nquit -noprompt\necho never\n" "-DEXPECT_OUT=^$"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_exit.cmake")
set_tests_properties("hefesto-cli: batch commands from standard input exit 0" PROPERTIES LABELS "unit;cli")

# [cap:app-startup][req:quit command and exit status]
add_test(NAME "hefesto-cli: quit 3 -noprompt exits 3"
         COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:hefesto-cli> -DEXPECT=3
                 "-DARGS=--no-log;--magic;-dnull;-noconsole" "-DSTDIN=quit 3 -noprompt\n"
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/expect_exit.cmake")
set_tests_properties("hefesto-cli: quit 3 -noprompt exits 3" PROPERTIES LABELS "unit;cli")

# [cap:scripting-api][req:Terminal input queue without console]: in --magic
# mode without a script, standard input is read as Tcl, one complete command
# at a time; an error prints its message and the next command runs.
add_test(NAME "hefesto-cli: --magic reads Tcl commands from standard input"
         COMMAND sh -c "printf 'set x 4\\nputs [expr {$x * 2}]\\nnosuch\\nforeach i {1 2} {\\n  puts -nonewline $i\\n}\\nputs {}\\n' | \"$<TARGET_FILE:hefesto-cli>\" --no-log --diagnostics=magic --magic -dnull -noconsole -norc")
set_tests_properties("hefesto-cli: --magic reads Tcl commands from standard input" PROPERTIES
  LABELS "unit;cli"
  PASS_REGULAR_EXPRESSION "8\ninvalid command name \"nosuch\"\n12\n")
