# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# hefesto_platform_net: the in-process HTTPS Fetcher of the PDK manager
# (capability hefesto-licensing, requirement "Third-party libraries for
# cryptography, networking and data formats", decision D-13): URLSession on
# Apple platforms (an Objective-C++ bridge; Foundation types never leave
# src/url_session_transport.mm) and the pinned libcurl (hefesto::curl,
# cmake/Dependencies.cmake) on Linux. The core must not contain either
# (scripts/check_core_includes.py). Linked by the CLI and the desktop on
# Linux and macOS. Added by the root CMakeLists.txt. Owner: INTEGRATION.
if(TARGET hefesto_platform_net)
  return()
endif()

add_library(hefesto_platform_net STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/file_url.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/http_fetcher.cpp)
add_library(hefesto::platform_net ALIAS hefesto_platform_net)
target_include_directories(hefesto_platform_net PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(hefesto_platform_net PUBLIC hefesto::core)

if(APPLE)
  target_sources(hefesto_platform_net PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/url_session_transport.mm)
  set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/url_session_transport.mm
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(hefesto_platform_net PRIVATE "-framework Foundation")
else()
  target_sources(hefesto_platform_net PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/curl_transport.cpp)
  target_link_libraries(hefesto_platform_net PRIVATE hefesto::curl)
endif()
hefesto_set_warnings(hefesto_platform_net)
