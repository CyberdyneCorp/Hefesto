# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of view3d/export (the glTF and STL writers and `export3d`),
# included by src/core/src/view3d/sources.cmake.
# Owner: EXPORT (P2) (openspec/changes/m10-3d-view/design.md D2/D3). Add new files here.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/cgltf_impl.cpp
  ${CMAKE_CURRENT_LIST_DIR}/command.cpp
  ${CMAKE_CURRENT_LIST_DIR}/geometry.cpp
  ${CMAKE_CURRENT_LIST_DIR}/gltf.cpp
  ${CMAKE_CURRENT_LIST_DIR}/layers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/stl.cpp)

# cgltf (header-only, MIT): compiled once in cgltf_impl.cpp, whose warnings
# are third-party code's. The export tests link hefesto::cgltf themselves to
# read files back with cgltf_parse.
target_link_libraries(hefesto_core PRIVATE hefesto::cgltf)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/cgltf_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
