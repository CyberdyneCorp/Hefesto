# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of oasis/write (the OASIS writer, repetition detection and the
# transcoders),
# included by src/core/src/oasis/sources.cmake.
# Owner: OASW (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/emit.cpp
  ${CMAKE_CURRENT_LIST_DIR}/write.cpp)
