# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of drc/rules (rule compiler, rule table, cifoutput catalog),
# included by src/core/src/drc/sources.cmake.
# Owner: RULES (openspec/changes/m3-drc/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/cif_catalog.cpp
  ${CMAKE_CURRENT_LIST_DIR}/compiler.cpp
  ${CMAKE_CURRENT_LIST_DIR}/keywords_basic.cpp
  ${CMAKE_CURRENT_LIST_DIR}/keywords_cif.cpp
  ${CMAKE_CURRENT_LIST_DIR}/keywords_shapes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/keywords_spacing.cpp
  ${CMAKE_CURRENT_LIST_DIR}/optimize.cpp
  ${CMAKE_CURRENT_LIST_DIR}/query.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_machine.cpp)
