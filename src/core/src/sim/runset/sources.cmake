# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of sim/runset (run sets, sweeps, DOE, corners, Monte Carlo and seeds),
# included by src/core/src/sim/sources.cmake.
# Owner: SIMADV (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/runset.cpp)
