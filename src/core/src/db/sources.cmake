# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the MODEL area (layout database), included by
# src/core/CMakeLists.txt.
# Owner: MODEL (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/cell.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/command_util.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands_box.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands_cells.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands_labels.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands_misc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands_paint.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands_setlabel.cpp
  ${CMAKE_CURRENT_LIST_DIR}/edit_util.cpp
  ${CMAKE_CURRENT_LIST_DIR}/flatten.cpp
  ${CMAKE_CURRENT_LIST_DIR}/hierarchy.cpp
  ${CMAKE_CURRENT_LIST_DIR}/labels.cpp
  ${CMAKE_CURRENT_LIST_DIR}/layer_list.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lock.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nm_paint.cpp
  ${CMAKE_CURRENT_LIST_DIR}/nm_split.cpp
  ${CMAKE_CURRENT_LIST_DIR}/paint.cpp
  ${CMAKE_CURRENT_LIST_DIR}/paint_plane.cpp
  ${CMAKE_CURRENT_LIST_DIR}/plane.cpp
  ${CMAKE_CURRENT_LIST_DIR}/polygon.cpp
  ${CMAKE_CURRENT_LIST_DIR}/property.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tile_plane.cpp
  ${CMAKE_CURRENT_LIST_DIR}/transform.cpp
  # M5 INDEX (openspec/changes/m5-mask-gds-cif/design.md D2): the cell-use index.
  ${CMAKE_CURRENT_LIST_DIR}/use_index.cpp
  ${CMAKE_CURRENT_LIST_DIR}/use_index_bins.cpp
  ${CMAKE_CURRENT_LIST_DIR}/use_index_enum.cpp)
