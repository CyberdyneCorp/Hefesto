# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extract/file (the `.ext` record writer and atomic output),
# included by src/core/src/extract/sources.cmake.
# Owner: EXTNODE (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/file_stubs.cpp)
