# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the PDK area (PDK manager), included by src/core/CMakeLists.txt.
# Owner: PDK (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/catalog.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hosts.cpp
  ${CMAKE_CURRENT_LIST_DIR}/manager.cpp
  ${CMAKE_CURRENT_LIST_DIR}/sha256.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tar_reader.cpp)

# Zstandard (PDK `.tar.zst` assets, hefesto-licensing D-13): the pinned
# decoder target from cmake/Dependencies.cmake.
target_link_libraries(hefesto_core PRIVATE hefesto::zstd)
