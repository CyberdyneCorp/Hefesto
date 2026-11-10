# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the drc area, included by src/core/CMakeLists.txt.
# Owner: INTEGRATION (openspec/changes/m3-drc/design.md D1/D2): this file
# lists the shared files (session, types, events, the code table) and
# includes one sources.cmake per subdirectory, each owned by its workstream:
#   rules/     RULES      check/     CHECK
#   engine/    ENGINE     commands/  COMMANDS
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/events.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session.cpp
  ${CMAKE_CURRENT_LIST_DIR}/types.cpp)

foreach(_drc_sub IN ITEMS rules check engine commands)
  include("${CMAKE_CURRENT_LIST_DIR}/${_drc_sub}/sources.cmake")
endforeach()
