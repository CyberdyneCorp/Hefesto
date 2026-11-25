# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the antenna area (`antennacheck` and the shared antenna rules),
# included by src/core/CMakeLists.txt. One directory, no subdirectories.
# Owner: ANTENNA (openspec/changes/m7-extraction/design.md D2/D3), except codes.cpp (INTEGRATION). Add new
# files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/antenna.cpp
  ${CMAKE_CURRENT_LIST_DIR}/evaluate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/measure.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/run.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp)
