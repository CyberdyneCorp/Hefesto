# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Sources of the CELL area (.mag I/O, library resolution, I/O safety,
# timestamps), included by src/core/CMakeLists.txt.
# Owner: CELL (openspec/changes/m1-core-viewer/design.md, D2/D3). Add new
# files here; never edit src/core/CMakeLists.txt.

target_sources(hefesto_core PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/cell_io.cpp
  ${CMAKE_CURRENT_LIST_DIR}/codes.cpp
  ${CMAKE_CURRENT_LIST_DIR}/commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/crash.cpp
  ${CMAKE_CURRENT_LIST_DIR}/crash_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/fonts.cpp
  ${CMAKE_CURRENT_LIST_DIR}/host.cpp
  ${CMAKE_CURRENT_LIST_DIR}/io_safety.cpp
  ${CMAKE_CURRENT_LIST_DIR}/io_session.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_elements.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_input.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_reader.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_reader_records.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_reader_use.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_script.cpp
  ${CMAKE_CURRENT_LIST_DIR}/mag_writer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/paths.cpp
  ${CMAKE_CURRENT_LIST_DIR}/place_commands.cpp
  ${CMAKE_CURRENT_LIST_DIR}/search_path.cpp
  ${CMAKE_CURRENT_LIST_DIR}/session.cpp
  ${CMAKE_CURRENT_LIST_DIR}/timestamps.cpp)
