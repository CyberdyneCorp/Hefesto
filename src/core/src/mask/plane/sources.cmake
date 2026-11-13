# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of mask/plane (the mask planes: CIF paint and erase tables, AND-NOT, the scratch pool),
# included by src/core/src/mask/sources.cmake.
# Owner: OPS (openspec/changes/m5-mask-gds-cif/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/plane.cpp)
