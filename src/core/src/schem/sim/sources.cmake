# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/sim (the schematic circuit source and deck building (M8 part)),
# included by src/core/src/schem/sources.cmake.
# Owner: SIMINT (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/deck.cpp)
