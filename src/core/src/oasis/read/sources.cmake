# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of oasis/read (the OASIS reader, read-only vendor views and content-
# hash timestamps),
# included by src/core/src/oasis/sources.cmake.
# Owner: OASR (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D1/D2). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/circle.cpp
  ${CMAKE_CURRENT_LIST_DIR}/fracture.cpp
  ${CMAKE_CURRENT_LIST_DIR}/index.cpp
  ${CMAKE_CURRENT_LIST_DIR}/messages.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/stamps.cpp
  ${CMAKE_CURRENT_LIST_DIR}/transcode.cpp
  ${CMAKE_CURRENT_LIST_DIR}/transcode_placements.cpp
  ${CMAKE_CURRENT_LIST_DIR}/transcode_shapes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/vendor.cpp)
