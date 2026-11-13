# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of gds/stream (the GDS stream layer: byte sources and sinks, records, reals, characters, dates),
# included by src/core/src/gds/sources.cmake.
# Owner: GDSR (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/encode.cpp
  ${CMAKE_CURRENT_LIST_DIR}/io.cpp
  ${CMAKE_CURRENT_LIST_DIR}/records.cpp)
