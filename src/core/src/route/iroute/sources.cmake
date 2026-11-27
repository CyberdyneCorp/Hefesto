# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of route/iroute (the `iroute` command, its parameters, `saveParameters` and endpoints),
# included by src/core/src/route/sources.cmake.
# Owner: IROUTE (openspec/changes/m10-plot-route/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session.cpp)
