# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of gds/write (the GDS writer, verbatim copy and full dump),
# included by src/core/src/gds/sources.cmake.
# Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/labels_sort.cpp
  ${CMAKE_CURRENT_LIST_DIR}/labels_text.cpp
  ${CMAKE_CURRENT_LIST_DIR}/merge_plane.cpp
  ${CMAKE_CURRENT_LIST_DIR}/merge_rings.cpp
  ${CMAKE_CURRENT_LIST_DIR}/write.cpp)
