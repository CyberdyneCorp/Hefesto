# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of sim/deck (circuit sources, testbench, stimuli, descriptors and sanitisation),
# included by src/core/src/sim/sources.cmake.
# Owner: SIMDECK (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/deck.cpp)
