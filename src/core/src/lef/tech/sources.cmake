# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/tech (the tech `lef` section, the LEF layer table, via table
# and non-default rules),
# included by src/core/src/lef/sources.cmake.
# Owner: LEFTECH (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/table.cpp)
