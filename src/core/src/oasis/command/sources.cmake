# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of oasis/command (the `oasis` command, its settings and format
# recognition on open),
# included by src/core/src/oasis/sources.cmake.
# Owner: OASCMD (openspec/changes/m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/format.cpp)
