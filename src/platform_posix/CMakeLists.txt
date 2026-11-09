# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_platform_posix: POSIX host services that the core must not
# contain (scripts/check_core_includes.py): the fcntl() cell FileLockService,
# a cell FileHost with ownership, ctime and process liveness, and the
# posix_spawn ProcessRunner. Linked by the CLI and the desktop on Linux and
# macOS (not iPadOS, whose host provides its own). Added by the root
# CMakeLists.txt. Owners: CELL (lock service, file host), INTEGRATION
# (process runner, build).
#
# Guarded so a directory configured on its own can still add it.
if(TARGET hefesto_platform_posix)
  return()
endif()

add_library(hefesto_platform_posix STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/file_host.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/lock_service.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/process_runner.cpp)
add_library(hefesto::platform_posix ALIAS hefesto_platform_posix)
target_include_directories(hefesto_platform_posix PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(hefesto_platform_posix PUBLIC hefesto::core)
hefesto_set_warnings(hefesto_platform_posix)
