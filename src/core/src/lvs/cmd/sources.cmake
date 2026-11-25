# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of lvs/cmd (the lvs command, sources, result model and cross-probing),
# included by src/core/src/lvs/sources.cmake.
# Owner: LVSCMD (openspec/changes/m9-lvs-simulation/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp)
