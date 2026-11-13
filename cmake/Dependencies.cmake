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
#   hefesto::zstd                   Zstandard v1.5.6 decoder, static (PDK .tar.zst assets)
#   hefesto::curl                   libcurl 8.23.0, static, HTTPS with the system OpenSSL
#                                   (Linux only; macOS uses URLSession, hefesto-licensing D-13)
#   Catch2::Catch2WithMain          Catch2 v3.7.1             (HEFESTO_BUILD_TESTS)
#   raven::raven                    Raven v0.2.3              (HEFESTO_BUILD_DESKTOP; desktop shell only)
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

# --- Zstandard v1.5.6 (BSD-3-Clause), decoder only ------------------------
# PDK release assets are .tar.zst (design D9). Only the streaming decoder is
# built, so PDK unpacking can stop at its size bound while decoding.
FetchContent_Declare(hefesto_zstd
  URL https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
  URL_HASH SHA256=8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  ${_hefesto_no_cmake})

# --- libcurl 8.23.0 (curl license, MIT-style), Linux only ----------------
# The HTTPS client of src/platform_net on Linux (hefesto-licensing D-13);
# Apple platforms use URLSession and must not contain libcurl. Built static
# with HTTP(S) only, TLS from the system OpenSSL 3 (libssl-dev) and the
# system trust store (CA bundle found at configure time, OpenSSL's default
# paths as fallback).
FetchContent_Declare(hefesto_curl
  URL https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz
  URL_HASH SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SYSTEM)

# --- Catch2 v3.7.1 (BSL-1.0), tests only ----------------------------------
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # v3.7.1
  GIT_PROGRESS FALSE)

# --- Raven v0.2.3 (Apache-2.0), desktop shell only ------------------------
FetchContent_Declare(raven
  GIT_REPOSITORY https://github.com/oscar-soirey/Raven.git
  GIT_TAG 019ab35d039897ef427dee89092c5602b99f5abf # v0.2.3
  GIT_PROGRESS FALSE)

FetchContent_MakeAvailable(hefesto_zlib hefesto_json hefesto_zstd)

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

# zstd: static decoder library from the release sources (no zstd CMake, no
# assembly, no legacy formats, no threads).
set(_hefesto_zstd_sources
  common/debug.c common/entropy_common.c common/error_private.c common/fse_decompress.c
  common/pool.c common/threading.c common/xxhash.c common/zstd_common.c
  decompress/huf_decompress.c decompress/zstd_ddict.c decompress/zstd_decompress.c
  decompress/zstd_decompress_block.c)
list(TRANSFORM _hefesto_zstd_sources PREPEND "${hefesto_zstd_SOURCE_DIR}/lib/")
add_library(hefesto_zstd STATIC ${_hefesto_zstd_sources})
add_library(hefesto::zstd ALIAS hefesto_zstd)
target_include_directories(hefesto_zstd SYSTEM PUBLIC "$<BUILD_INTERFACE:${hefesto_zstd_SOURCE_DIR}/lib>")
target_compile_definitions(hefesto_zstd PRIVATE ZSTD_DISABLE_ASM=1 ZSTD_LEGACY_SUPPORT=0)
set_target_properties(hefesto_zstd PROPERTIES POSITION_INDEPENDENT_CODE ON C_EXTENSIONS ON)
if(NOT MSVC)
  target_compile_options(hefesto_zstd PRIVATE -w)
endif()

# nlohmann/json: header-only interface target with the upstream name.
add_library(hefesto_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS hefesto_json)
target_include_directories(hefesto_json SYSTEM INTERFACE "$<BUILD_INTERFACE:${hefesto_json_SOURCE_DIR}/include>")
target_compile_features(hefesto_json INTERFACE cxx_std_20)

# libcurl: its own CMake build, configured in a block so its options do not
# leak into the rest of the project.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  block()
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_STATIC_LIBS ON)
    set(BUILD_CURL_EXE OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_LIBCURL_DOCS OFF)
    set(BUILD_MISC_DOCS OFF)
    set(ENABLE_CURL_MANUAL OFF)
    set(CURL_DISABLE_INSTALL ON)
    set(CURL_ENABLE_EXPORT_TARGET OFF)
    set(PICKY_COMPILER OFF)
    set(CURL_DISABLE_DICT ON)
    set(CURL_DISABLE_FILE ON)
    set(CURL_DISABLE_FTP ON)
    set(CURL_DISABLE_GOPHER ON)
    set(CURL_DISABLE_IMAP ON)
    set(CURL_DISABLE_LDAP ON)
    set(CURL_DISABLE_LDAPS ON)
    set(CURL_DISABLE_MQTT ON)
    set(CURL_DISABLE_POP3 ON)
    set(CURL_DISABLE_RTSP ON)
    set(CURL_DISABLE_SMTP ON)
    set(CURL_DISABLE_TELNET ON)
    set(CURL_DISABLE_TFTP ON)
    set(CURL_DISABLE_IPFS ON)
    set(CURL_DISABLE_COOKIES ON)
    set(CURL_DISABLE_ALTSVC ON)
    set(CURL_DISABLE_HSTS ON)
    set(CURL_DISABLE_NETRC ON)
    set(CURL_DISABLE_WEBSOCKETS ON)
    set(CURL_USE_OPENSSL ON)
    set(CURL_CA_FALLBACK ON)
    set(CURL_ZLIB OFF)
    set(CURL_BROTLI OFF)
    set(CURL_ZSTD OFF)
    set(USE_NGHTTP2 OFF)
    set(USE_LIBIDN2 OFF)
    set(CURL_USE_LIBPSL OFF)
    set(CURL_USE_LIBSSH2 OFF)
    set(CURL_USE_LIBSSH OFF)
    set(CURL_USE_GSSAPI OFF)
    set(ENABLE_ARES OFF)
    FetchContent_MakeAvailable(hefesto_curl)
  endblock()
  add_library(hefesto::curl ALIAS libcurl_static)
endif()

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
