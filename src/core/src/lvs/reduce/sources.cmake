# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/reduce (series/parallel reduction and zero-valued removal),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSRED (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/reduce.cpp)
