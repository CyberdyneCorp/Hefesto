# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/read (the LEF reader, abstract views and annotation),
# included by src/core/src/lef/sources.cmake.
# Owner: LEFR (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/find_file.cpp)
