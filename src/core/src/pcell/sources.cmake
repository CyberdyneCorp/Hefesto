# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the pcell area, included by src/core/CMakeLists.txt.
# Owner: INTEGRATION (openspec/changes/m4-scripting/design.md D1/D2): this
# file lists the shared code table and includes one sources.cmake per
# subdirectory, owned by PCELL:
#   framework/  the gencell framework      native/  the native generators
#               (native/{sky130,gf180mcu,sg13g2}/ are listed by native/sources.cmake)
# Add files to your subdirectory's sources.cmake; never edit this file.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)

foreach(_pcell_sub IN ITEMS framework native)
  include("${CMAKE_CURRENT_LIST_DIR}/${_pcell_sub}/sources.cmake")
endforeach()
