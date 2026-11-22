# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/command (the `extract` command, run driver, incremental extraction and options),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTCMD (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/run_stubs.cpp)
