# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Third-party dependencies, each pinned to an exact version or commit
# (capability hefesto-build-and-distribution, requirement "Dependency policy").
#
# Offline rebuild: once a configure has populated FETCHCONTENT_BASE_DIR
# (the presets use .cache/deps/<preset>), reconfigure with
#   -DFETCHCONTENT_FULLY_DISCONNECTED=ON
# and no network access is attempted.
#
# Targets provided:
#   hefesto::zlib                   zlib v1.3.1, static, built from its sources
#   nlohmann_json::nlohmann_json    nlohmann/json v3.11.3, header-only
#   Catch2::Catch2WithMain          Catch2 v3.7.1             (HEFESTO_BUILD_TESTS)
#   raven::raven                    Raven v0.2.2              (HEFESTO_BUILD_DESKTOP; desktop shell only)
#
# When adding a dependency, also add its notice to THIRD_PARTY_NOTICES.md
# (capability hefesto-licensing).

include(FetchContent)

# Never re-check pinned sources for updates: the pins below are immutable.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update populated dependencies")

# Content fetched for its sources only: its own CMakeLists.txt is not run.
# (zlib 1.3.1 and nlohmann/json 3.11.3 declare cmake_minimum_required < 3.5,
# which CMake 4 rejects, and zlib's build adds a shared library and examples.)
set(_hefesto_no_cmake SOURCE_SUBDIR hefesto-sources-only)

# --- zlib v1.3.1 (zlib license) -------------------------------------------
FetchContent_Declare(hefesto_zlib
  URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
  URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  ${_hefesto_no_cmake})

# --- nlohmann/json v3.11.3 (MIT) ------------------------------------------
FetchContent_Declare(hefesto_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  ${_hefesto_no_cmake})

# --- Catch2 v3.7.1 (BSL-1.0), tests only ----------------------------------
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # v3.7.1
  GIT_PROGRESS FALSE)

# --- Raven v0.2.2 (Apache-2.0), desktop shell only ------------------------
FetchContent_Declare(raven
  GIT_REPOSITORY https://github.com/oscar-soirey/Raven.git
  GIT_TAG 4a05d6f11c79e5a6eae77d7fe26f702f5fea4331 # v0.2.2
  GIT_PROGRESS FALSE)

FetchContent_MakeAvailable(hefesto_zlib hefesto_json)

# zlib: static library from the release sources, no zlib CMake involved.
set(_hefesto_zlib_sources
  adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c
  infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
list(TRANSFORM _hefesto_zlib_sources PREPEND "${hefesto_zlib_SOURCE_DIR}/")
add_library(hefesto_zlib STATIC ${_hefesto_zlib_sources})
add_library(hefesto::zlib ALIAS hefesto_zlib)
target_include_directories(hefesto_zlib SYSTEM PUBLIC "$<BUILD_INTERFACE:${hefesto_zlib_SOURCE_DIR}>")
if(NOT WIN32)
  target_compile_definitions(hefesto_zlib PUBLIC Z_HAVE_UNISTD_H)
endif()
set_target_properties(hefesto_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON C_EXTENSIONS ON)
# Third-party code: silence its warnings (Xcode enables extra ones by default).
if(NOT MSVC)
  target_compile_options(hefesto_zlib PRIVATE -w)
endif()

# nlohmann/json: header-only interface target with the upstream name.
add_library(hefesto_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS hefesto_json)
target_include_directories(hefesto_json SYSTEM INTERFACE "$<BUILD_INTERFACE:${hefesto_json_SOURCE_DIR}/include>")
target_compile_features(hefesto_json INTERFACE cxx_std_20)

if(HEFESTO_BUILD_TESTS AND NOT HEFESTO_IS_IOS)
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

if(HEFESTO_BUILD_DESKTOP)
  set(RAVEN_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(RAVEN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(RAVEN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(RAVEN_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(RAVEN_BUILD_WEB OFF CACHE BOOL "" FORCE)
  set(RAVEN_BACKEND_VULKAN ON CACHE BOOL "" FORCE)
  # Use the Vulkan SDK that enabled the desktop build; never download headers.
  set(RAVEN_FETCH_VULKAN_HEADERS OFF CACHE BOOL "" FORCE)
  set(RAVEN_REBUILD_SHADERS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(raven)
endif()
