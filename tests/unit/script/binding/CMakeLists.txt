# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of script/binding. Owner: BINDING (openspec/changes/m4-scripting/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_script_binding_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(script_binding DATA_DIR script/binding)
