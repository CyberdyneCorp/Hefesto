# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of sim/engine (the engine interface, worker client, job manager and pool),
# included by src/core/src/sim/sources.cmake.
# Owner: SIMENG (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/engine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/job_manager.cpp
  ${CMAKE_CURRENT_LIST_DIR}/pool.cpp
  ${CMAKE_CURRENT_LIST_DIR}/job_manager_events.hpp)
