# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of pcell/native. Owner: PCELL (openspec/changes/m4-scripting/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_pcell_native_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(pcell_native DATA_DIR pcell/native)
# tcl_value_test.cpp checks the private Tcl value layer of the generator ports.
target_include_directories(hefesto_pcell_native_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/core/src")
