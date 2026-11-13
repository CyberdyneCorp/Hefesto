# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of cif/write (the CIF writer),
# included by src/core/src/cif/sources.cmake.
# Owner: CIF (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/write.cpp)
