# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the gui area (GUI manager view models and their commands),
# included by src/core/CMakeLists.txt.
# Owner: GUIMGR (openspec/changes/m4-scripting/design.md D1/D2).
# Add new files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/drc_report.cpp
  ${CMAKE_CURRENT_LIST_DIR}/gui_util.cpp
  ${CMAKE_CURRENT_LIST_DIR}/managers.cpp
  ${CMAKE_CURRENT_LIST_DIR}/prompts.cpp
  ${CMAKE_CURRENT_LIST_DIR}/tk_model.cpp
  ${CMAKE_CURRENT_LIST_DIR}/toolbar_presets.cpp
  ${CMAKE_CURRENT_LIST_DIR}/wrapper.cpp)
