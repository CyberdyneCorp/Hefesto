# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/tech (the extraction-style subset of the extract-tech-
# section reader and StyleView),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.
# M10 TECH extends the same reader with the 3D height subset (HeightView,
# openspec/changes/m10-3d-view/design.md D5).

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/height_view.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style_view.cpp)
