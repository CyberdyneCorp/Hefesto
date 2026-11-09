# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the cell area. Owner: CELL
# (openspec/changes/m1-core-viewer/design.md D2). Every *_test.cpp here is
# compiled into hefesto_cell_tests; extra libraries or definitions go below.

hefesto_add_area_tests(cell)

# POSIX host services (src/platform_posix: fcntl locks, ownership/ctime/
# liveness FileHost), added by the root CMakeLists.txt on Linux and macOS.
if(TARGET hefesto_platform_posix)
  target_link_libraries(hefesto_cell_tests PRIVATE hefesto::platform_posix)
  target_compile_definitions(hefesto_cell_tests PRIVATE HEFESTO_HAVE_PLATFORM_POSIX=1)
endif()
