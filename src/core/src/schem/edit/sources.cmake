# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of schem/edit (the document model, editing operations and net highlighting),
# included by src/core/src/schem/sources.cmake.
# Owner: EDIT (openspec/changes/m8-schematic/design.md D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/clipboard.cpp
  ${CMAKE_CURRENT_LIST_DIR}/context.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hierarchy.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hilight.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lifecycle.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nets.cpp
  ${CMAKE_CURRENT_LIST_DIR}/operations.cpp
  ${CMAKE_CURRENT_LIST_DIR}/order.cpp
  ${CMAKE_CURRENT_LIST_DIR}/palette.cpp
  ${CMAKE_CURRENT_LIST_DIR}/place.cpp
  ${CMAKE_CURRENT_LIST_DIR}/props.cpp
  ${CMAKE_CURRENT_LIST_DIR}/select.cpp
  ${CMAKE_CURRENT_LIST_DIR}/symbol_ops.cpp
  ${CMAKE_CURRENT_LIST_DIR}/transform_ops.cpp
  ${CMAKE_CURRENT_LIST_DIR}/wires.cpp)
