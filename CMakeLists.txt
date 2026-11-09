# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Hefesto: a C++20 VLSI layout tool, data-compatible with Magic VLSI.
# Ownership of this file and of cmake/: openspec/changes/m0-foundations/design.md (D2).

cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not supported. Use a preset, e.g. "
    "`cmake --preset macos-debug`, or `cmake -S . -B build/<name>`.")
endif()

project(Hefesto
  VERSION 0.0.1
  DESCRIPTION "VLSI layout tool, data-compatible with Magic VLSI"
  HOMEPAGE_URL "https://github.com/CyberdyneCorp/Hefesto"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# --- Options --------------------------------------------------------------

set(HEFESTO_IS_IOS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(HEFESTO_IS_IOS ON)
endif()

# The desktop shell needs Vulkan (MoltenVK on macOS). Default it ON only when
# a Vulkan SDK is found, so core + CLI always configure on a bare machine.
if(NOT DEFINED HEFESTO_BUILD_DESKTOP AND NOT HEFESTO_IS_IOS)
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    set(_hefesto_desktop_default ON)
  else()
    set(_hefesto_desktop_default OFF)
    message(STATUS "Hefesto: Vulkan SDK not found; HEFESTO_BUILD_DESKTOP defaults to OFF "
                   "(core, CLI and tests are still built)")
  endif()
else()
  set(_hefesto_desktop_default OFF)
endif()

option(HEFESTO_BUILD_DESKTOP "Build the Vulkan renderer and the Raven desktop shell" ${_hefesto_desktop_default})
option(HEFESTO_BUILD_TESTS "Build unit tests and register golden tests" ON)
option(HEFESTO_BUILD_FUZZ "Build libFuzzer targets (Clang with libFuzzer only)" OFF)
option(HEFESTO_BUILD_BENCH "Build the canvas benchmark (requires HEFESTO_BUILD_DESKTOP)" OFF)
option(HEFESTO_WARNINGS_AS_ERRORS "Treat compiler warnings in first-party code as errors" OFF)
option(HEFESTO_TEST_DESKTOP_WINDOW
  "Register desktop smoke tests that open a window through Raven (needs a display; CI X11/Wayland legs)" OFF)
set(HEFESTO_SANITIZERS "" CACHE STRING
  "Comma-separated list passed to -fsanitize= for every target (e.g. address,undefined)")

if(HEFESTO_IS_IOS AND HEFESTO_BUILD_DESKTOP)
  message(WARNING "Hefesto: the desktop shell is not available on iOS; forcing HEFESTO_BUILD_DESKTOP=OFF")
  set(HEFESTO_BUILD_DESKTOP OFF CACHE BOOL "" FORCE)
endif()

if(HEFESTO_BUILD_DESKTOP)
  find_package(Vulkan REQUIRED)
endif()

if(HEFESTO_BUILD_BENCH AND NOT HEFESTO_BUILD_DESKTOP)
  message(WARNING "Hefesto: HEFESTO_BUILD_BENCH needs HEFESTO_BUILD_DESKTOP; the canvas benchmark is skipped")
endif()

# --- Toolchain settings and dependencies ---------------------------------

include(CompilerWarnings)
include(Sanitizers)
include(Dependencies)

# --- Areas (each directory is owned by one workstream, see design.md D2) --

add_subdirectory(src/core)

if(NOT HEFESTO_IS_IOS)
  add_subdirectory(src/cli)
else()
  add_subdirectory(tests/ios)  # link probe: the core links on iPadOS
endif()

if(HEFESTO_BUILD_DESKTOP)
  add_subdirectory(src/render/vulkan)
  add_subdirectory(src/shell/desktop)
endif()

if(HEFESTO_BUILD_TESTS AND NOT HEFESTO_IS_IOS)
  enable_testing()
  add_subdirectory(tests/unit)
  add_subdirectory(tools/golden)
endif()

# libFuzzer targets with HEFESTO_BUILD_FUZZ; otherwise their seed corpora
# are replayed as CTests (tests/fuzz/CMakeLists.txt).
if(HEFESTO_BUILD_FUZZ OR (HEFESTO_BUILD_TESTS AND NOT HEFESTO_IS_IOS))
  add_subdirectory(tests/fuzz)
endif()

if(HEFESTO_BUILD_BENCH AND HEFESTO_BUILD_DESKTOP)
  add_subdirectory(bench/canvas)
endif()

message(STATUS "Hefesto ${PROJECT_VERSION}: desktop=${HEFESTO_BUILD_DESKTOP} tests=${HEFESTO_BUILD_TESTS} "
               "fuzz=${HEFESTO_BUILD_FUZZ} bench=${HEFESTO_BUILD_BENCH} sanitizers='${HEFESTO_SANITIZERS}'")
