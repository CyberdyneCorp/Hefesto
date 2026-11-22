# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the extract area (the `extract` tech section reader and
# ExtStyle, and the extractor), included by src/core/CMakeLists.txt. The
# area builds with -ffp-contract=off (set PUBLIC on hefesto_core;
# openspec/changes/m6-lef-def-oasis/design.md D3, openspec/changes/m7-extraction/design.md D4).
# Owner: INTEGRATION (openspec/changes/m6-lef-def-oasis/design.md D1/D2;
# openspec/changes/m7-extraction/design.md D2/D3): this file lists the shared files and includes one
# sources.cmake per subdirectory, each owned by its workstream (tech/ was
# EXTSUB's in M6 and is EXTTECH's in M7):
#   tech      EXTTECH
#   node      EXTNODE
#   file      EXTNODE
#   cap       EXTCAP
#   device    EXTDEV
#   hier      EXTHIER
#   command   EXTCMD
#   length    EXTCMD
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_extract_sub IN ITEMS tech node file cap device hier command length)
  include("${CMAKE_CURRENT_LIST_DIR}/${_extract_sub}/sources.cmake")
endforeach()
