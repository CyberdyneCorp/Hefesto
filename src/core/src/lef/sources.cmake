# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the lef area (the LEF/DEF tokenizer, the tech `lef` section and
# LEF layer table, via geometry, LEF read and write and the `lef`/`def`
# commands),
# included by src/core/CMakeLists.txt. The area builds with -ffp-contract=off
# (set PUBLIC on hefesto_core; openspec/changes/m6-lef-def-oasis/design.md D3).
# Owner: INTEGRATION (openspec/changes/m6-lef-def-oasis/design.md D1/D2): this
# file lists the shared files and includes one sources.cmake per
# subdirectory, each owned by its workstream:
#   lex/     LEFR
#   tech/    LEFTECH
#   via/     LEFVIA
#   read/    LEFR
#   write/   LEFW
#   command/ INTEGRATION
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_lef_sub IN ITEMS lex tech via read write command)
  include("${CMAKE_CURRENT_LIST_DIR}/${_lef_sub}/sources.cmake")
endforeach()
