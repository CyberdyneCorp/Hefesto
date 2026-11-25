# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of extresist/network (network construction and reduction),
# included by src/core/src/extresist/sources.cmake.
# Owner: RESNET (openspec/changes/m7-extraction/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/builder.cpp
  ${CMAKE_CURRENT_LIST_DIR}/devices.cpp
  ${CMAKE_CURRENT_LIST_DIR}/fracture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/graph.cpp
  ${CMAKE_CURRENT_LIST_DIR}/heap.cpp
  ${CMAKE_CURRENT_LIST_DIR}/network.cpp
  ${CMAKE_CURRENT_LIST_DIR}/prepare.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reductions.cpp
  ${CMAKE_CURRENT_LIST_DIR}/resistors.cpp
  ${CMAKE_CURRENT_LIST_DIR}/simplify.cpp
  ${CMAKE_CURRENT_LIST_DIR}/squish.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tiles.cpp)
