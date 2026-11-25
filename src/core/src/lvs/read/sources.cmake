# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/read (the SPICE, CDL, Verilog, .sim and .ext readers),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSREAD (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp)
