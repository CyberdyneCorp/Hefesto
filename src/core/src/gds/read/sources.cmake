# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of gds/read (the GDS reader, read-only vendor views and content-hash timestamps),
# included by src/core/src/gds/sources.cmake.
# Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/library.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/stamps.cpp
  ${CMAKE_CURRENT_LIST_DIR}/vendor.cpp)
