# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of pcell/native (the native generator registry, the bit-identical
# PDK helpers, and the generators in native/{sky130,gf180mcu,sg13g2}/),
# included by src/core/src/pcell/sources.cmake.
# Owner: PCELL (openspec/changes/m4-scripting/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/helpers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/registry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tcl_number.cpp)
