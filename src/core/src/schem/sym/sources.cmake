# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/sym (the symbol model, pins, pin order and text tokens),
# included by src/core/src/schem/sources.cmake.
# Owner: SYMLIB (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/instance.cpp
  ${CMAKE_CURRENT_LIST_DIR}/symbol.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tokens.cpp
  ${CMAKE_CURRENT_LIST_DIR}/types.cpp)
