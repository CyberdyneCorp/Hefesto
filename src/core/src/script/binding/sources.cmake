# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of script/binding (the magic:: command binding, tags, output,
# engine-read variables, the `cif` command), included by
# src/core/src/script/sources.cmake.
# Owner: BINDING (openspec/changes/m4-scripting/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/bind.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cif_command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/dispatch.cpp
  ${CMAKE_CURRENT_LIST_DIR}/drcstate.cpp
  ${CMAKE_CURRENT_LIST_DIR}/output.cpp
  ${CMAKE_CURRENT_LIST_DIR}/procedures.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tags.cpp
  ${CMAKE_CURRENT_LIST_DIR}/variables.cpp)
