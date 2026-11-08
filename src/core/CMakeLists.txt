# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_core: the platform-independent core (capability hefesto-architecture).
# Depends only on the C++20 standard library, zlib and nlohmann/json.
# No windowing, GPU, OS-UI or process-spawning API may be used here.
# Owner: workstream "Core & CLI" (openspec/changes/m0-foundations/design.md D2).

configure_file(include/hefesto/version.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/include/hefesto/version.hpp" @ONLY)

add_library(hefesto_core STATIC
  src/version.cpp
  src/cmd/builtins.cpp
  src/cmd/command.cpp
  src/cmd/command_log.cpp
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
