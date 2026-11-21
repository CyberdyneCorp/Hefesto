# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/cmd (the `schematic` and `xschem` commands),
# included by src/core/src/schem/sources.cmake.
# Owner: CMD (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/document_options.cpp
  ${CMAKE_CURRENT_LIST_DIR}/options.cpp
  ${CMAKE_CURRENT_LIST_DIR}/query_options.cpp
  ${CMAKE_CURRENT_LIST_DIR}/xschem_command.cpp)
