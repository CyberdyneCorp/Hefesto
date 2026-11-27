# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The schematic editor's view-model tests without the desktop build
# (HEFESTO_BUILD_DESKTOP=OFF): the same test files as hefesto_shell_tests
# runs in a desktop build, linked to the Vulkan-free
# hefesto_shell_schematic_models (src/shell/desktop/schematic_models).
# Owner: SHELL (openspec/changes/m8-schematic task 13.2).

set(_tests "${CMAKE_CURRENT_SOURCE_DIR}/..")
hefesto_add_unit_test(hefesto_shell_schematic_tests
  SOURCES
    ${_tests}/schematic_bindings_test.cpp
    ${_tests}/schematic_canvas_test.cpp
    ${_tests}/schematic_clipboard_test.cpp
    ${_tests}/schematic_input_test.cpp
    ${_tests}/schematic_library_test.cpp
    ${_tests}/schematic_panels_test.cpp
    ${_tests}/schematic_symbols_test.cpp
  LIBS
    hefesto::shell_schematic_models)
target_compile_definitions(hefesto_shell_schematic_tests PRIVATE HEFESTO_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_include_directories(hefesto_shell_schematic_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/unit/common")
