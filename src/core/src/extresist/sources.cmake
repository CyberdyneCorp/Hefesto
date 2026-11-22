# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the extresist area (resistance extraction: networks, the `extresist` command and its output),
# included by src/core/CMakeLists.txt. The area builds with -ffp-contract=off
# (set PUBLIC on hefesto_core; openspec/changes/m7-extraction/design.md D4).
# Owner: INTEGRATION (openspec/changes/m7-extraction/design.md D2/D3): this file lists the shared files and
# includes one sources.cmake per subdirectory, each owned by its workstream:
#   network   RESNET
#   command   RESCMD
#   output    RESCMD
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_extresist_sub IN ITEMS network command output)
  include("${CMAKE_CURRENT_LIST_DIR}/${_extresist_sub}/sources.cmake")
endforeach()
