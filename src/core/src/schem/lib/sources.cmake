# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/lib (symbol references, the library search path, `xschemrc` as data, the symbol cache and generators),
# included by src/core/src/schem/sources.cmake.
# Owner: SYMLIB (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/search_path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/symbol_cache.cpp)
