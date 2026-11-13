# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Unit tests of mask/input. Owner: INPUT (openspec/changes/m5-mask-gds-cif/design.md
# D1/D2). Every *_test.cpp here is compiled into hefesto_mask_input_tests;
# extra libraries or definitions go below.

hefesto_add_area_tests(mask_input DATA_DIR mask/input)
