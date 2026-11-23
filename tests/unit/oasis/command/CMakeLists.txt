# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of oasis/command. Owner: OASCMD (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_oasis_command_tests;
# extra libraries or definitions go below.

# The fixture reuses the oasis/read session and its test technology.
hefesto_add_area_tests(oasis_command DATA_DIR oasis/read)
