# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of mask/style (the cifoutput reader, OutputStyle, rescale, queries and the shared style/variant/scale core),
# included by src/core/src/mask/sources.cmake.
# Owner: STYLES (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/core.cpp
  ${CMAKE_CURRENT_LIST_DIR}/errors.cpp
  ${CMAKE_CURRENT_LIST_DIR}/finalize.cpp
  ${CMAKE_CURRENT_LIST_DIR}/layers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ops.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style.cpp)
