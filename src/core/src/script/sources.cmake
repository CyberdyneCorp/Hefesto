# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the script area, included by src/core/CMakeLists.txt.
# Owner: INTEGRATION (openspec/changes/m4-scripting/design.md D1/D2): this
# file lists the shared files (types, events, the project interpreter, the
# code table) and includes one sources.cmake per subdirectory, each owned
# by its workstream:
#   tcl/      TCL       binding/  BINDING       pdk/  PDKINT
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/events.cpp
  ${CMAKE_CURRENT_LIST_DIR}/project.cpp
  ${CMAKE_CURRENT_LIST_DIR}/types.cpp)

foreach(_script_sub IN ITEMS tcl binding pdk)
  include("${CMAKE_CURRENT_LIST_DIR}/${_script_sub}/sources.cmake")
endforeach()
