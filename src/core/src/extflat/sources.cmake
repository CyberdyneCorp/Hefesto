# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the extflat area (the `.ext` reader, flattener and name
# conversion of the netlisters; capabilities ext-flattening-read,
# -hierarchy and -names), included by src/core/CMakeLists.txt. M6 built the
# read-and-flatten subset that `def write` calls; M7 extends it.
# Owner: EXTFLAT (openspec/changes/m7-extraction/design.md D3, D13),
# except codes.cpp (INTEGRATION). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/args.cpp
  ${CMAKE_CURRENT_LIST_DIR}/build.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flatten.cpp
  ${CMAKE_CURRENT_LIST_DIR}/graph.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hier_name.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hierarchy.cpp
  ${CMAKE_CURRENT_LIST_DIR}/model.cpp
  ${CMAKE_CURRENT_LIST_DIR}/names.cpp
  ${CMAKE_CURRENT_LIST_DIR}/ports.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/text.cpp)
