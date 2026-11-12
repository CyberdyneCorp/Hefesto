# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of script/pdk. Owner: PDKINT (openspec/changes/m4-scripting/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_script_pdk_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(script_pdk DATA_DIR script/pdk)
