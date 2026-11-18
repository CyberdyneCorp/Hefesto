# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the extflat area (the `.ext` read-and-flatten subset that
# `def write` calls, pulled forward from M7 by Open Question 1, decided (a),
# and built to the M7 specs ext-flattening-read, -hierarchy and -names),
# included by src/core/CMakeLists.txt. M7 extends these files.
# Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D1/D2, D10),
# except codes.cpp (INTEGRATION). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/build.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flatten.cpp
  ${CMAKE_CURRENT_LIST_DIR}/graph.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hier_name.cpp
  ${CMAKE_CURRENT_LIST_DIR}/model.cpp
  ${CMAKE_CURRENT_LIST_DIR}/read.cpp
  ${CMAKE_CURRENT_LIST_DIR}/reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/text.cpp)
