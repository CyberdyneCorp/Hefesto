# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/tech (the extraction-style subset of the extract-tech-
# section reader and StyleView),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.
# M10 TECH extends the same reader with the 3D height subset (HeightView,
# openspec/changes/m10-3d-view/design.md D5).

# M7 EXTTECH completes the reader (openspec/changes/m7-extraction/design.md
# D7): one file per keyword group, the final pass, the conversions and the
# style table.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/antenna_rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cap_rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/conversion.cpp
  ${CMAKE_CURRENT_LIST_DIR}/default_caps.cpp
  ${CMAKE_CURRENT_LIST_DIR}/device_rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/finalize.cpp
  ${CMAKE_CURRENT_LIST_DIR}/height_view.cpp
  ${CMAKE_CURRENT_LIST_DIR}/resist_rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/rule_context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scale_rules.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_table.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_view.cpp)
