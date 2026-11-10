# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of drc/commands (the `drc` command and reporting), included by
# src/core/src/drc/sources.cmake.
# Owner: COMMANDS (openspec/changes/m3-drc/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/control_options.cpp
  ${CMAKE_CURRENT_LIST_DIR}/report_options.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reporting.cpp)
