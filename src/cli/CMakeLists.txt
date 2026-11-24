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

# hefesto-extcheck: the standalone extcheck (openspec/changes/m7-extraction
# design.md D2, D15; Open Question 6 (a)). Owner: SIM (the target);
# INTEGRATION (install and packaging, design D20).
add_subdirectory(extcheck)

# M8-D10 (approved 2026-11-23; openspec/changes/m8-schematic design D8): the
# default CAD_ROOT is the install root holding the executable, so the
# schematic system library (<CAD_ROOT>/assets/schematic = XSCHEM_SHAREDIR)
# is copied next to hefesto-cli in a build tree and installed under the
# prefix (<prefix>/bin/hefesto-cli finds <prefix>/assets/schematic).
add_custom_command(TARGET hefesto-cli POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PROJECT_SOURCE_DIR}/assets/schematic" "$<TARGET_FILE_DIR:hefesto-cli>/assets/schematic"
  VERBATIM)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/schematic" DESTINATION assets)
