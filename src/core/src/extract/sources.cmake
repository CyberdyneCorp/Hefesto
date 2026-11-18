# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the extract area (the extraction-style subset of the `extract` tech section reader),
# included by src/core/CMakeLists.txt. The area builds with -ffp-contract=off
# (set PUBLIC on hefesto_core; openspec/changes/m6-lef-def-oasis/design.md D3).
# Owner: INTEGRATION (openspec/changes/m6-lef-def-oasis/design.md D1/D2): this
# file lists the shared files and includes one sources.cmake per
# subdirectory, each owned by its workstream:
#   tech/ EXTSUB
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_extract_sub IN ITEMS tech)
  include("${CMAKE_CURRENT_LIST_DIR}/${_extract_sub}/sources.cmake")
endforeach()
