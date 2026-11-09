# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the tech area. Owner: TECH
# (openspec/changes/m1-core-viewer/design.md D2). Every *_test.cpp here is
# compiled into hefesto_tech_tests; extra libraries or definitions go below.

hefesto_add_area_tests(tech)

# Real technology files, used when present (tests skip otherwise):
#  - Magic's minimum.tech.in and a generated scmos.tech from the reference
#    tree build/ref/magic (gitignored; see docs/building.md);
#  - the pinned sky130A tech file under .cache/pdks (gitignored).
set(_magic_ref "${PROJECT_SOURCE_DIR}/build/ref/magic")
set(_scmos_dir "${CMAKE_CURRENT_BINARY_DIR}/magic-tech")
if(EXISTS "${_magic_ref}/scmos/scmos.tech.in" AND NOT EXISTS "${_scmos_dir}/scmos.tech"
   AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  execute_process(COMMAND ${CMAKE_COMMAND} -DMAGIC=${_magic_ref} -DOUT=${_scmos_dir} -DCXX=${CMAKE_CXX_COMPILER}
                          -P ${PROJECT_SOURCE_DIR}/tests/data/tech/generate_scmos.cmake)
endif()
target_compile_definitions(hefesto_tech_tests PRIVATE
  HEFESTO_MAGIC_REF_DIR="${_magic_ref}"
  HEFESTO_SCMOS_TECH="${_scmos_dir}/scmos.tech")
