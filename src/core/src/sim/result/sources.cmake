# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of sim/result (result storage, raw, CSV and waveform measurements),
# included by src/core/src/sim/sources.cmake.
# Owner: SIMRES (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/result_store.cpp
  ${CMAKE_CURRENT_LIST_DIR}/progress.cpp
  ${CMAKE_CURRENT_LIST_DIR}/raw_write.cpp
  ${CMAKE_CURRENT_LIST_DIR}/raw_read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/csv.cpp
  ${CMAKE_CURRENT_LIST_DIR}/measure.cpp
  ${CMAKE_CURRENT_LIST_DIR}/analysis.cpp)
