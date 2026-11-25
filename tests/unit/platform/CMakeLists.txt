# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Tests of the worker service (hefesto-architecture "Long-lived worker
# processes") and of hefesto-simd through it (simulation-engine-embedding).
# Owner: PLATFORM (openspec/changes/m9-lvs-simulation task 5.3), SIMD (the
# hefesto-simd tests, task 5.4).
#
#   hefesto_test_worker   a worker that echoes, exits, crashes and hangs
#   hefesto_fake_adapter  a hefesto_engine_v1 stand-in that hefesto-simd loads
#                         with --adapter (no ngspice)
#   hefesto_platform_tests  the tests; the ngspice ones only when the real
#                         plugin is built (HEFESTO_BUILD_NGSPICE=ON)

add_executable(hefesto_test_worker test_worker.cpp)
hefesto_set_warnings(hefesto_test_worker)

add_library(hefesto_fake_adapter MODULE fake_adapter.cpp)
set_target_properties(hefesto_fake_adapter PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_include_directories(hefesto_fake_adapter PRIVATE "${PROJECT_SOURCE_DIR}/src/simd")
target_link_libraries(hefesto_fake_adapter PRIVATE Threads::Threads)
hefesto_set_warnings(hefesto_fake_adapter)

# src/simd is added after tests/unit (top-level CMakeLists.txt), so its targets
# are named by condition here; generator expressions resolve them later.
set(_have_simd OFF)
if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_have_simd ON)
endif()
set(_platform_sources worker_host_test.cpp)
if(_have_simd)
  list(APPEND _platform_sources worker_host_simd_test.cpp)
endif()
if(_have_simd AND HEFESTO_BUILD_NGSPICE)
  list(APPEND _platform_sources worker_host_ngspice_test.cpp)
  if(HEFESTO_DEVELOPER_BUILD)
    list(APPEND _platform_sources worker_host_devhost_test.cpp)
  endif()
endif()
hefesto_add_unit_test(hefesto_platform_tests SOURCES ${_platform_sources} LIBS hefesto_cli_lib)
add_dependencies(hefesto_platform_tests hefesto_test_worker hefesto_fake_adapter)
target_compile_definitions(hefesto_platform_tests PRIVATE
  HEFESTO_TEST_WORKER="$<TARGET_FILE:hefesto_test_worker>"
  HEFESTO_FAKE_ADAPTER="$<TARGET_FILE:hefesto_fake_adapter>")
if(_have_simd)
  add_dependencies(hefesto_platform_tests hefesto-simd)
  target_compile_definitions(hefesto_platform_tests PRIVATE HEFESTO_SIMD="$<TARGET_FILE:hefesto-simd>")
endif()
if(_have_simd AND HEFESTO_BUILD_NGSPICE)
  add_dependencies(hefesto_platform_tests hefesto-ngspice-adapter)
  target_compile_definitions(hefesto_platform_tests PRIVATE
    HEFESTO_NGSPICE_ADAPTER="$<TARGET_FILE:hefesto-ngspice-adapter>")
  if(HEFESTO_DEVELOPER_BUILD)
    target_link_libraries(hefesto_platform_tests PRIVATE hefesto_simd_devhost)
  endif()
endif()
