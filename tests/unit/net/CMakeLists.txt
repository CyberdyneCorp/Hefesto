# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of hefesto::platform_net, the HTTPS Fetcher (URLSession on
# macOS, libcurl on Linux). Owner: INTEGRATION. Every *_test.cpp here is
# compiled into hefesto_net_tests; the system-transport tests talk to a
# loopback HTTP server (tests/unit/net/loopback_server.hpp), never to the
# network.

file(GLOB _net_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp")
hefesto_add_unit_test(hefesto_net_tests SOURCES ${_net_sources} LIBS hefesto::platform_net)
target_include_directories(hefesto_net_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/tests/unit/pdk")  # temp_dir.hpp
