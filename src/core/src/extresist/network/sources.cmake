# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extresist/network (network construction and reduction),
# included by src/core/src/extresist/sources.cmake.
# Owner: RESNET (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/network_stubs.cpp)
