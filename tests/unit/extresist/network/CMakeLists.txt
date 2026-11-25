# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of extresist/network. Owner: RESNET (openspec/changes/m7-extraction/design.md D2/D3).
# Every *_test.cpp here is compiled into hefesto_extresist_network_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(extresist_network DATA_DIR extresist/network)
# The working network's units (graph, heap, reducer, builder) are tested
# through their private headers.
if(TARGET hefesto_extresist_network_tests)
  target_include_directories(hefesto_extresist_network_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/core/src")
endif()
