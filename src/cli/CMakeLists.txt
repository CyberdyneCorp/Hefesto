# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto-cli: the headless command-line tool (capability hefesto-architecture,
# requirement "Headless command-line tool"). Linux and macOS only.
# Owner: workstream "Core & CLI" (openspec/changes/m0-foundations/design.md D2).

# Everything but main(), so unit tests can link the CLI logic.
add_library(hefesto_cli_lib STATIC
  app.cpp
  interactive.cpp
  options.cpp
  tcl_lines.cpp)
target_include_directories(hefesto_cli_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hefesto_cli_lib PUBLIC hefesto::core hefesto::platform_posix hefesto::platform_net)
hefesto_set_warnings(hefesto_cli_lib)

add_executable(hefesto-cli
  main.cpp)
target_link_libraries(hefesto-cli PRIVATE hefesto_cli_lib)
hefesto_set_warnings(hefesto-cli)
