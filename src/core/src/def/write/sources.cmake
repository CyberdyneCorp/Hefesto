# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of def/write (the DEF writer),
# included by src/core/src/def/sources.cmake.
# Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/write.cpp)
