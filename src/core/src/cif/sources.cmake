# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the cif area (CIF read and write),
# included by src/core/CMakeLists.txt. The area builds with -ffp-contract=off
# (set PUBLIC on hefesto_core; openspec/changes/m5-mask-gds-cif/design.md D3).
# Owner: INTEGRATION (openspec/changes/m5-mask-gds-cif/design.md D1/D2): this
# file lists the shared files and includes one sources.cmake per
# subdirectory, each owned by its workstream:
#   read/     CIF
#   write/    CIF
#   command/  CIF
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_cif_sub IN ITEMS read write command)
  include("${CMAKE_CURRENT_LIST_DIR}/${_cif_sub}/sources.cmake")
endforeach()
