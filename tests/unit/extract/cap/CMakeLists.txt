# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of extract/cap. Owner: EXTCAP (openspec/changes/m7-extraction/design.md D2/D3).
# Every *_test.cpp here is compiled into hefesto_extract_cap_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(extract_cap DATA_DIR extract/cap)
if(TARGET hefesto_extract_cap_tests)
  # util/cr_atan.hpp, the private header of the correctly rounded atan.
  target_include_directories(hefesto_extract_cap_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/core/src")
endif()
