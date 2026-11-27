# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of sim/runset (run sets, sweeps, DOE, corners, Monte Carlo and seeds),
# included by src/core/src/sim/sources.cmake.
# Owner: SIMADV (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/corners.cpp
  ${CMAKE_CURRENT_LIST_DIR}/montecarlo.cpp
  ${CMAKE_CURRENT_LIST_DIR}/point_deck.cpp
  ${CMAKE_CURRENT_LIST_DIR}/random.cpp
  ${CMAKE_CURRENT_LIST_DIR}/runner.cpp
  ${CMAKE_CURRENT_LIST_DIR}/runset.cpp
  ${CMAKE_CURRENT_LIST_DIR}/storage.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sweep.cpp
  ${CMAKE_CURRENT_LIST_DIR}/verdict.cpp)
