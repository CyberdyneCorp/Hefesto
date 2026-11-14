# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of cif/read (the CIF lexer, parser, symbol table and read session),
# included by src/core/src/cif/sources.cmake.
# Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/lexer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/messages.cpp
  ${CMAKE_CURRENT_LIST_DIR}/parser.cpp
  ${CMAKE_CURRENT_LIST_DIR}/parser_cells.cpp
  ${CMAKE_CURRENT_LIST_DIR}/parser_labels.cpp
  ${CMAKE_CURRENT_LIST_DIR}/parser_shapes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/symbols.cpp
  ${CMAKE_CURRENT_LIST_DIR}/wire.cpp)
