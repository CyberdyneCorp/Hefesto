# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of mask/input (the cifinput reader, InputStyle and the read engine behind ReadSink),
# included by src/core/src/mask/sources.cmake.
# Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/current.cpp
  ${CMAKE_CURRENT_LIST_DIR}/engine.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/scale.cpp
  ${CMAKE_CURRENT_LIST_DIR}/style.cpp)
