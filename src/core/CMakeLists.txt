# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_core: the platform-independent core (capability hefesto-architecture).
# Depends only on the C++20 standard library, zlib and nlohmann/json.
# No windowing, GPU, OS-UI or process-spawning API may be used here.
# Owner: INTEGRATION (openspec/changes/m1-core-viewer/design.md D2). The M1
# areas add their sources in src/<area>/sources.cmake, never here (D3).

configure_file(include/hefesto/version.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/include/hefesto/version.hpp" @ONLY)

add_library(hefesto_core STATIC
  src/version.cpp
  src/cmd/all_commands.cpp
  src/cmd/builtins.cpp
  src/cmd/command.cpp
  src/cmd/command_log.cpp
  src/cmd/coordinates.cpp
  src/cmd/dispatcher.cpp
  src/cmd/registry.cpp
  src/cmd/script_parser.cpp
  src/cmd/script_runner.cpp
  src/cmd/value.cpp
  src/compat/compat_settings.cpp
  src/diag/codes.cpp
  src/diag/diagnostic.cpp
  src/diag/format.cpp
  src/diag/sink.cpp
  src/editor/editor_state.cpp
  src/platform/local_file_system.cpp
  src/platform/logger.cpp
  src/platform/memory_file_system.cpp
  src/platform/services.cpp
  src/platform/session_log.cpp
  src/platform/storage.cpp
  src/platform/task_executor.cpp
  src/settings/project_settings.cpp
  src/util/record_order.cpp)
add_library(hefesto::core ALIAS hefesto_core)

# M1 areas (openspec/changes/m1-core-viewer/design.md D2/D3). Each fragment
# calls target_sources(hefesto_core PRIVATE ...) for its own directory and is
# edited only by the owning workstream:
#   tech/ TECH   db/ MODEL   cell/ CELL   display/ view/ VIEWER   pdk/ app/ PDK
set(HEFESTO_CORE_AREAS tech db cell display view pdk app)
foreach(_area IN LISTS HEFESTO_CORE_AREAS)
  include("${CMAKE_CURRENT_SOURCE_DIR}/src/${_area}/sources.cmake")
endforeach()

target_include_directories(hefesto_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_features(hefesto_core PUBLIC cxx_std_20)
target_link_libraries(hefesto_core
  PRIVATE
    hefesto::zlib
    nlohmann_json::nlohmann_json)
find_package(Threads REQUIRED)
target_link_libraries(hefesto_core PUBLIC Threads::Threads)
hefesto_set_warnings(hefesto_core)
