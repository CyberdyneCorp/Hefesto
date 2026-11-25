# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extresist/output (`.res.ext`, `.res.lump`, FastHenry and geometry output),
# included by src/core/src/extresist/sources.cmake.
# Owner: RESCMD (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/input.cpp
  ${CMAKE_CURRENT_LIST_DIR}/writer.cpp)
