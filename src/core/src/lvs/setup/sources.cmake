# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/setup (setup-file commands in the sandbox),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSSETUP (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/equate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/equate_pins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hierarchy.cpp
  ${CMAKE_CURRENT_LIST_DIR}/model.cpp
  ${CMAKE_CURRENT_LIST_DIR}/permute.cpp
  ${CMAKE_CURRENT_LIST_DIR}/property.cpp
  ${CMAKE_CURRENT_LIST_DIR}/property_merge.cpp
  ${CMAKE_CURRENT_LIST_DIR}/query.cpp
  ${CMAKE_CURRENT_LIST_DIR}/runtime.cpp
  ${CMAKE_CURRENT_LIST_DIR}/setup.cpp
  ${CMAKE_CURRENT_LIST_DIR}/words.cpp)
