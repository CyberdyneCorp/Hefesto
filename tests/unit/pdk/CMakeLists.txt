# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of the pdk area. Owner: PDK
# (openspec/changes/m1-core-viewer/design.md D2). Every *_test.cpp here is
# compiled into hefesto_pdk_tests; extra libraries or definitions go below.

hefesto_add_area_tests(pdk)

# End-to-end install over the network with the real Fetchers (task 7.3): the
# HTTPS Fetcher of hefesto::platform_net and the curl-process fallback.
# A separate executable with the ctest label `network` (not `unit`), so CI
# can leave it out with `ctest -LE network`; HEFESTO_SKIP_NETWORK_TESTS=1
# skips it at run time. POSIX only: it uses the posix_spawn process runner.
if(TARGET hefesto_platform_posix AND TARGET hefesto_platform_net)
  add_executable(hefesto_pdk_network_tests network_e2e.cpp)
  target_link_libraries(hefesto_pdk_network_tests PRIVATE Catch2::Catch2WithMain hefesto::core hefesto::platform_posix
  hefesto::platform_net)
  target_include_directories(hefesto_pdk_network_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  hefesto_set_warnings(hefesto_pdk_network_tests)
  catch_discover_tests(hefesto_pdk_network_tests
    TEST_PREFIX "hefesto_pdk_network_tests: "
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS network TIMEOUT 600)
endif()
