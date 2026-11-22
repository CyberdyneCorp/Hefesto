# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/file (the file grammar, reader and writer and the property-string grammar),
# included by src/core/src/schem/sources.cmake.
# Owner: FORMAT (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/block_symbol.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lexer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/numbers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/properties.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/save.cpp
  ${CMAKE_CURRENT_LIST_DIR}/transform.cpp
  ${CMAKE_CURRENT_LIST_DIR}/writer.cpp)
