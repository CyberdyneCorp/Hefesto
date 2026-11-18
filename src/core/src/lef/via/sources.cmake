# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lef/via (the via geometry generator: LEF via rules, DEF generated
# vias and VIAS properties, cut growing, contact regions),
# included by src/core/src/lef/sources.cmake.
# Owner: LEFVIA (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/cut.cpp)
